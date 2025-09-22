#!/bin/bash

# Advanced Functionality Test Script
# Tests cgroups constraints and stress scenarios - REQUIRES ROOT PRIVILEGES

set -e

AGENT_PID=""
TEST_PIDS=()
FAILED_TESTS=0
TOTAL_TESTS=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

# Cleanup function
cleanup() {
    echo -e "\n${BLUE}Cleaning up...${NC}"

    # Stop any remaining test processes
    for pid in "${TEST_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            echo "Stopping test process $pid"
            ./bin/controller stop "$pid" >/dev/null 2>&1 || true
            sleep 1
        fi
    done

    # Stop agent
    if [ ! -z "$AGENT_PID" ] && kill -0 "$AGENT_PID" 2>/dev/null; then
        echo "Stopping agent (PID: $AGENT_PID)"
        kill "$AGENT_PID"
        wait "$AGENT_PID" 2>/dev/null || true
    fi

    # Clean up cgroups if they exist
    if [ -d "/sys/fs/cgroup/orchestrator" ]; then
        echo "Cleaning up cgroups..."
        find /sys/fs/cgroup/orchestrator -type d -name "proc_*" -exec rmdir {} + 2>/dev/null || true
        rmdir /sys/fs/cgroup/orchestrator 2>/dev/null || true
    fi

    # Clean up socket
    rm -f /tmp/process_orchestrator.sock
}

# Test result tracking
test_result() {
    local test_name="$1"
    local result="$2"
    local details="$3"
    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    if [ "$result" -eq 0 ]; then
        echo -e "${GREEN}✓ PASS${NC}: $test_name"
        [ ! -z "$details" ] && echo -e "  ${details}"
    else
        echo -e "${RED}✗ FAIL${NC}: $test_name"
        [ ! -z "$details" ] && echo -e "  ${RED}${details}${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
}

# Wait for agent to be ready
wait_for_agent() {
    local max_attempts=10
    local attempt=0

    while [ $attempt -lt $max_attempts ]; do
        if ./bin/controller list >/dev/null 2>&1; then
            return 0
        fi
        sleep 1
        attempt=$((attempt + 1))
    done

    return 1
}

# Extract PID from start command output
extract_pid() {
    local output="$1"
    echo "$output" | grep "Process started successfully with PID:" | sed 's/.*PID: \([0-9]*\).*/\1/'
}

# Check if cgroups v2 is available
check_cgroups() {
    if [ ! -d "/sys/fs/cgroup" ]; then
        return 1
    fi
    if [ ! -w "/sys/fs/cgroup" ]; then
        return 1
    fi
    return 0
}

# Get memory usage of a process in KB
get_memory_usage() {
    local pid="$1"
    if [ -f "/proc/$pid/status" ]; then
        grep "VmRSS:" "/proc/$pid/status" | awk '{print $2}'
    else
        echo "0"
    fi
}

# Set up signal handlers
trap cleanup EXIT INT TERM

echo -e "${PURPLE}Process Orchestration Advanced Functionality Test${NC}"
echo "================================================="
echo

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERROR: This script must be run as root for cgroups testing.${NC}"
    echo "Usage: sudo $0"
    exit 1
fi

# Check prerequisites
echo -e "${BLUE}Checking prerequisites...${NC}"
if [ ! -f "./bin/agent" ] || [ ! -f "./bin/controller" ] || [ ! -f "./bin/monitor" ]; then
    echo -e "${RED}Error: Binaries not found. Run 'make all' first.${NC}"
    exit 1
fi

if ! check_cgroups; then
    echo -e "${RED}Error: cgroups v2 not available or not writable.${NC}"
    exit 1
fi

echo -e "${GREEN}✓ All prerequisites met${NC}"
echo

# Test 1: Start Agent with cgroups
echo -e "${BLUE}Test 1: Starting Agent with cgroups Support${NC}"
./bin/agent &
AGENT_PID=$!
sleep 3

if kill -0 "$AGENT_PID" 2>/dev/null; then
    test_result "Agent startup with cgroups" 0
else
    test_result "Agent startup with cgroups" 1
    echo -e "${RED}Agent failed to start. Exiting.${NC}"
    exit 1
fi

# Test 2: Agent Connectivity
echo -e "\n${BLUE}Test 2: Agent Connectivity${NC}"
wait_for_agent
test_result "Agent connectivity" $?

# Test 3: cgroups Directory Creation
echo -e "\n${BLUE}Test 3: cgroups Directory Creation${NC}"
if [ -d "/sys/fs/cgroup/orchestrator" ]; then
    test_result "cgroups orchestrator directory created" 0
else
    test_result "cgroups orchestrator directory created" 1
fi

# Test 4: Start Process for Constraint Testing
echo -e "\n${BLUE}Test 4: Start Memory-Intensive Process${NC}"
# Create a simple memory consumer
cat > /tmp/memory_test.py << 'EOF'
import time
import sys

# Allocate approximately 50MB of memory
data = []
for i in range(50):
    data.append('x' * (1024 * 1024))  # 1MB chunks

print(f"Allocated ~50MB, PID: {os.getpid()}")
time.sleep(300)  # Sleep for 5 minutes
EOF

OUTPUT=$(./bin/controller start python3 /tmp/memory_test.py)
if echo "$OUTPUT" | grep -q "Process started successfully"; then
    MEMORY_PID=$(extract_pid "$OUTPUT")
    TEST_PIDS+=("$MEMORY_PID")
    test_result "Start memory-intensive process" 0 "PID: $MEMORY_PID"
    sleep 2  # Let it allocate memory
else
    test_result "Start memory-intensive process" 1
    MEMORY_PID=""
fi

# Test 5: Apply Memory Constraints
echo -e "\n${BLUE}Test 5: Apply Memory Constraints${NC}"
if [ ! -z "$MEMORY_PID" ]; then
    OUTPUT=$(./bin/controller constrain "$MEMORY_PID" 30 50)
    if echo "$OUTPUT" | grep -q "Constraints applied"; then
        test_result "Apply memory constraints (30MB)" 0

        # Check if cgroup was created
        if [ -d "/sys/fs/cgroup/orchestrator/proc_$MEMORY_PID" ]; then
            test_result "Process cgroup directory created" 0
        else
            test_result "Process cgroup directory created" 1
        fi
    else
        test_result "Apply memory constraints (30MB)" 1 "$OUTPUT"
    fi
else
    test_result "Apply memory constraints (30MB)" 1 "No process available"
fi

# Test 6: Start CPU-Intensive Process
echo -e "\n${BLUE}Test 6: Start CPU-Intensive Process${NC}"
cat > /tmp/cpu_test.sh << 'EOF'
#!/bin/bash
# Simple CPU burner
while true; do
    echo "CPU intensive task..." > /dev/null
done
EOF
chmod +x /tmp/cpu_test.sh

OUTPUT=$(./bin/controller start /tmp/cpu_test.sh)
if echo "$OUTPUT" | grep -q "Process started successfully"; then
    CPU_PID=$(extract_pid "$OUTPUT")
    TEST_PIDS+=("$CPU_PID")
    test_result "Start CPU-intensive process" 0 "PID: $CPU_PID"
else
    test_result "Start CPU-intensive process" 1
    CPU_PID=""
fi

# Test 7: Apply CPU Constraints
echo -e "\n${BLUE}Test 7: Apply CPU Constraints${NC}"
if [ ! -z "$CPU_PID" ]; then
    OUTPUT=$(./bin/controller constrain "$CPU_PID" 0 25)
    if echo "$OUTPUT" | grep -q "Constraints applied"; then
        test_result "Apply CPU constraints (25%)" 0
    else
        test_result "Apply CPU constraints (25%)" 1 "$OUTPUT"
    fi
else
    test_result "Apply CPU constraints (25%)" 1 "No process available"
fi

# Test 8: Multiple Processes with Different Constraints
echo -e "\n${BLUE}Test 8: Multiple Processes with Different Constraints${NC}"
MULTI_CONSTRAINT_SUCCESS=0

# Start three different processes
for i in 1 2 3; do
    OUTPUT=$(./bin/controller start sleep $((60 + i * 10)))
    if echo "$OUTPUT" | grep -q "Process started successfully"; then
        PID=$(extract_pid "$OUTPUT")
        TEST_PIDS+=("$PID")

        # Apply different constraints
        MEMORY_LIMIT=$((10 + i * 5))
        CPU_LIMIT=$((20 + i * 10))

        CONSTRAINT_OUTPUT=$(./bin/controller constrain "$PID" "$MEMORY_LIMIT" "$CPU_LIMIT")
        if echo "$CONSTRAINT_OUTPUT" | grep -q "Constraints applied"; then
            MULTI_CONSTRAINT_SUCCESS=$((MULTI_CONSTRAINT_SUCCESS + 1))
        fi
    fi
done

if [ $MULTI_CONSTRAINT_SUCCESS -eq 3 ]; then
    test_result "Multiple processes with different constraints" 0 "Applied constraints to 3 processes"
else
    test_result "Multiple processes with different constraints" 1 "Only $MULTI_CONSTRAINT_SUCCESS/3 succeeded"
fi

# Test 9: Process Monitoring with Constraints
echo -e "\n${BLUE}Test 9: Process Monitoring with Constraints${NC}"
sleep 2
MONITOR_OUTPUT=$(timeout 8 ./bin/monitor 2>/dev/null || true)
if echo "$MONITOR_OUTPUT" | grep -q "Process Monitor Report" &&
   echo "$MONITOR_OUTPUT" | grep -q "Memory:" &&
   echo "$MONITOR_OUTPUT" | grep -q "CPU Time:"; then
    PROCESS_COUNT=$(echo "$MONITOR_OUTPUT" | grep -c "Process [0-9]")
    test_result "Monitor processes with constraints" 0 "Monitored $PROCESS_COUNT processes"
else
    test_result "Monitor processes with constraints" 1
fi

# Test 10: cgroups Files Verification
echo -e "\n${BLUE}Test 10: cgroups Files Verification${NC}"
CGROUP_FILES_OK=0

for pid in "${TEST_PIDS[@]}"; do
    if [ -d "/sys/fs/cgroup/orchestrator/proc_$pid" ]; then
        # Check if memory.max exists and has a value
        if [ -f "/sys/fs/cgroup/orchestrator/proc_$pid/memory.max" ]; then
            MEMORY_LIMIT=$(cat "/sys/fs/cgroup/orchestrator/proc_$pid/memory.max" 2>/dev/null || echo "")
            if [ ! -z "$MEMORY_LIMIT" ] && [ "$MEMORY_LIMIT" != "max" ]; then
                CGROUP_FILES_OK=$((CGROUP_FILES_OK + 1))
            fi
        fi
    fi
done

if [ $CGROUP_FILES_OK -gt 0 ]; then
    test_result "cgroups files verification" 0 "$CGROUP_FILES_OK processes have cgroup limits"
else
    test_result "cgroups files verification" 1
fi

# Test 11: Constraint Error Handling
echo -e "\n${BLUE}Test 11: Constraint Error Handling${NC}"
OUTPUT=$(./bin/controller constrain 999999 100 50 2>&1)
if echo "$OUTPUT" | grep -q "Error" || echo "$OUTPUT" | grep -q "not found"; then
    test_result "Constraint error handling (invalid PID)" 0
else
    test_result "Constraint error handling (invalid PID)" 1
fi

# Test 12: Stress Test - Many Processes
echo -e "\n${BLUE}Test 12: Stress Test - Start Multiple Processes${NC}"
STRESS_SUCCESS=0
STRESS_PIDS=()

for i in {1..10}; do
    OUTPUT=$(./bin/controller start sleep $((30 + i)))
    if echo "$OUTPUT" | grep -q "Process started successfully"; then
        PID=$(extract_pid "$OUTPUT")
        STRESS_PIDS+=("$PID")
        TEST_PIDS+=("$PID")
        STRESS_SUCCESS=$((STRESS_SUCCESS + 1))
    fi
    sleep 0.1
done

if [ $STRESS_SUCCESS -eq 10 ]; then
    test_result "Stress test - start 10 processes" 0
else
    test_result "Stress test - start 10 processes" 1 "Only $STRESS_SUCCESS/10 succeeded"
fi

# Test 13: Bulk Process Listing
echo -e "\n${BLUE}Test 13: Bulk Process Listing${NC}"
OUTPUT=$(./bin/controller list)
LISTED_COUNT=$(echo "$OUTPUT" | grep -c "PID:" || echo "0")
EXPECTED_MIN=${#TEST_PIDS[@]}

if [ "$LISTED_COUNT" -ge "$EXPECTED_MIN" ]; then
    test_result "Bulk process listing" 0 "Listed $LISTED_COUNT processes (expected >= $EXPECTED_MIN)"
else
    test_result "Bulk process listing" 1 "Listed $LISTED_COUNT, expected >= $EXPECTED_MIN"
fi

# Test 14: Bulk Process Stopping
echo -e "\n${BLUE}Test 14: Bulk Process Stopping${NC}"
STOP_SUCCESS=0

for pid in "${STRESS_PIDS[@]}"; do
    OUTPUT=$(./bin/controller stop "$pid")
    if echo "$OUTPUT" | grep -q "stopped successfully"; then
        STOP_SUCCESS=$((STOP_SUCCESS + 1))
        # Remove from main tracking array
        TEST_PIDS=($(echo "${TEST_PIDS[@]}" | tr ' ' '\n' | grep -v "^$pid$" | tr '\n' ' '))
    fi
    sleep 0.1
done

if [ $STOP_SUCCESS -eq ${#STRESS_PIDS[@]} ]; then
    test_result "Bulk process stopping" 0 "Stopped ${#STRESS_PIDS[@]} processes"
else
    test_result "Bulk process stopping" 1 "Only $STOP_SUCCESS/${#STRESS_PIDS[@]} stopped"
fi

# Test 15: cgroups Cleanup Verification
echo -e "\n${BLUE}Test 15: cgroups Cleanup Verification${NC}"
sleep 2
REMAINING_CGROUPS=$(find /sys/fs/cgroup/orchestrator -type d -name "proc_*" 2>/dev/null | wc -l)
ACTIVE_PROCESSES=${#TEST_PIDS[@]}

# Should have cgroups for remaining active processes
if [ "$REMAINING_CGROUPS" -le "$ACTIVE_PROCESSES" ]; then
    test_result "cgroups cleanup verification" 0 "$REMAINING_CGROUPS cgroup dirs for $ACTIVE_PROCESSES processes"
else
    test_result "cgroups cleanup verification" 1 "Too many cgroup dirs: $REMAINING_CGROUPS"
fi

# Final Results
echo
echo -e "${PURPLE}===========================================${NC}"
echo -e "${PURPLE}Advanced Test Results Summary${NC}"
echo -e "${PURPLE}===========================================${NC}"

PASSED_TESTS=$((TOTAL_TESTS - FAILED_TESTS))
echo -e "Total Tests: $TOTAL_TESTS"
echo -e "${GREEN}Passed: $PASSED_TESTS${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${RED}Failed: $FAILED_TESTS${NC}"
    echo
    echo -e "${GREEN}🚀 ALL ADVANCED TESTS PASSED! 🚀${NC}"
    echo -e "${GREEN}Full functionality including cgroups is working correctly.${NC}"

    # Show cgroups status
    echo
    echo -e "${BLUE}cgroups Status:${NC}"
    echo "Orchestrator cgroup directory: $([ -d '/sys/fs/cgroup/orchestrator' ] && echo 'EXISTS' || echo 'MISSING')"
    echo "Active process cgroups: $(find /sys/fs/cgroup/orchestrator -type d -name 'proc_*' 2>/dev/null | wc -l)"

    exit 0
else
    echo -e "${RED}Failed: $FAILED_TESTS${NC}"
    echo
    echo -e "${RED}❌ SOME ADVANCED TESTS FAILED${NC}"
    echo -e "${YELLOW}Please review the failed tests above.${NC}"
    exit 1
fi

# Cleanup temp files
rm -f /tmp/memory_test.py /tmp/cpu_test.sh