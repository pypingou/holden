#!/bin/bash

# Basic Functionality Test Script
# Tests core process orchestration without requiring root privileges

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
NC='\033[0m' # No Color

# Cleanup function
cleanup() {
    echo -e "\n${BLUE}Cleaning up...${NC}"

    # Stop any remaining test processes
    for pid in "${TEST_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            echo "Stopping test process $pid"
            ./bin/controller stop "$pid" >/dev/null 2>&1 || true
        fi
    done

    # Stop agent
    if [ ! -z "$AGENT_PID" ] && kill -0 "$AGENT_PID" 2>/dev/null; then
        echo "Stopping agent (PID: $AGENT_PID)"
        kill "$AGENT_PID"
        wait "$AGENT_PID" 2>/dev/null || true
    fi

    # Clean up socket
    rm -f /tmp/process_orchestrator.sock
}

# Test result tracking
test_result() {
    local test_name="$1"
    local result="$2"
    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    if [ "$result" -eq 0 ]; then
        echo -e "${GREEN}✓ PASS${NC}: $test_name"
    else
        echo -e "${RED}✗ FAIL${NC}: $test_name"
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

# Set up signal handlers
trap cleanup EXIT INT TERM

echo -e "${BLUE}Process Orchestration Basic Functionality Test${NC}"
echo "=============================================="
echo

# Check if binaries exist
echo -e "${BLUE}Checking prerequisites...${NC}"
if [ ! -f "./bin/agent" ] || [ ! -f "./bin/controller" ] || [ ! -f "./bin/monitor" ]; then
    echo -e "${RED}Error: Binaries not found. Run 'make all' first.${NC}"
    exit 1
fi
echo -e "${GREEN}✓ All binaries found${NC}"
echo

# Test 1: Start Agent
echo -e "${BLUE}Test 1: Starting Agent${NC}"
./bin/agent &
AGENT_PID=$!
sleep 2

if kill -0 "$AGENT_PID" 2>/dev/null; then
    test_result "Agent startup" 0
else
    test_result "Agent startup" 1
    echo -e "${RED}Agent failed to start. Exiting.${NC}"
    exit 1
fi

# Test 2: Agent Connectivity
echo -e "\n${BLUE}Test 2: Agent Connectivity${NC}"
wait_for_agent
test_result "Agent connectivity" $?

# Test 3: Initial Process List (should be empty)
echo -e "\n${BLUE}Test 3: Initial Process List${NC}"
OUTPUT=$(./bin/controller list)
if echo "$OUTPUT" | grep -q "Running processes (0)"; then
    test_result "Initial empty process list" 0
else
    test_result "Initial empty process list" 1
fi

# Test 4: Start Single Process
echo -e "\n${BLUE}Test 4: Start Single Process${NC}"
OUTPUT=$(./bin/controller start sleep 60)
if echo "$OUTPUT" | grep -q "Process started successfully"; then
    PID=$(extract_pid "$OUTPUT")
    TEST_PIDS+=("$PID")
    test_result "Start single process (sleep 60)" 0
    echo "Started process with PID: $PID"
else
    test_result "Start single process (sleep 60)" 1
fi

# Test 5: Process List with One Process
echo -e "\n${BLUE}Test 5: Process List with One Process${NC}"
OUTPUT=$(./bin/controller list)
if echo "$OUTPUT" | grep -q "Running processes (1)" && echo "$OUTPUT" | grep -q "sleep"; then
    test_result "Process list with one process" 0
else
    test_result "Process list with one process" 1
fi

# Test 6: Start Multiple Processes
echo -e "\n${BLUE}Test 6: Start Multiple Processes${NC}"
MULTI_SUCCESS=0

# Start second process
OUTPUT=$(./bin/controller start sleep 120)
if echo "$OUTPUT" | grep -q "Process started successfully"; then
    PID=$(extract_pid "$OUTPUT")
    TEST_PIDS+=("$PID")
    MULTI_SUCCESS=$((MULTI_SUCCESS + 1))
fi

# Start third process with arguments
OUTPUT=$(./bin/controller start echo "Hello World")
if echo "$OUTPUT" | grep -q "Process started successfully"; then
    PID=$(extract_pid "$OUTPUT")
    TEST_PIDS+=("$PID")
    MULTI_SUCCESS=$((MULTI_SUCCESS + 1))
fi

if [ $MULTI_SUCCESS -eq 2 ]; then
    test_result "Start multiple processes" 0
else
    test_result "Start multiple processes" 1
fi

# Test 7: Process List with Multiple Processes
echo -e "\n${BLUE}Test 7: Process List with Multiple Processes${NC}"
sleep 1  # Give echo process time to finish
OUTPUT=$(./bin/controller list)
PROCESS_COUNT=$(echo "$OUTPUT" | grep "Running processes" | sed 's/.*(\([0-9]*\)).*/\1/')

if [ "$PROCESS_COUNT" -ge 2 ]; then
    test_result "Process list with multiple processes" 0
    echo "Found $PROCESS_COUNT running processes"
else
    test_result "Process list with multiple processes" 1
    echo "Expected >= 2 processes, found $PROCESS_COUNT"
fi

# Test 8: Monitor Functionality
echo -e "\n${BLUE}Test 8: Monitor Functionality${NC}"
MONITOR_OUTPUT=$(timeout 5 ./bin/monitor 2>/dev/null || true)
if echo "$MONITOR_OUTPUT" | grep -q "Process Monitor Report" && echo "$MONITOR_OUTPUT" | grep -q "Status: Running"; then
    test_result "Monitor functionality" 0
else
    test_result "Monitor functionality" 1
fi

# Test 9: Stop Specific Process
echo -e "\n${BLUE}Test 9: Stop Specific Process${NC}"
if [ ${#TEST_PIDS[@]} -gt 0 ]; then
    STOP_PID=${TEST_PIDS[0]}
    OUTPUT=$(./bin/controller stop "$STOP_PID")
    if echo "$OUTPUT" | grep -q "Process $STOP_PID stopped successfully"; then
        test_result "Stop specific process" 0
        # Remove from tracking array
        TEST_PIDS=("${TEST_PIDS[@]:1}")
    else
        test_result "Stop specific process" 1
    fi
else
    test_result "Stop specific process" 1
    echo "No processes available to stop"
fi

# Test 10: Process List After Stop
echo -e "\n${BLUE}Test 10: Process List After Stop${NC}"
sleep 1
OUTPUT=$(./bin/controller list)
PROCESS_COUNT=$(echo "$OUTPUT" | grep "Running processes" | sed 's/.*(\([0-9]*\)).*/\1/')

# Should have one less process now
if [ "$PROCESS_COUNT" -ge 1 ]; then
    test_result "Process list after stop" 0
    echo "Processes remaining: $PROCESS_COUNT"
else
    test_result "Process list after stop" 1
fi

# Test 11: Start Process with Arguments
echo -e "\n${BLUE}Test 11: Start Process with Arguments${NC}"
OUTPUT=$(./bin/controller start bash -c "sleep 30")
if echo "$OUTPUT" | grep -q "Process started successfully"; then
    PID=$(extract_pid "$OUTPUT")
    TEST_PIDS+=("$PID")
    test_result "Start process with arguments" 0
else
    test_result "Start process with arguments" 1
fi

# Test 12: Invalid Command Handling
echo -e "\n${BLUE}Test 12: Invalid Command Handling${NC}"
OUTPUT=$(./bin/controller start /nonexistent/command 2>&1)
if echo "$OUTPUT" | grep -q "Error starting process"; then
    test_result "Invalid command handling" 0
else
    test_result "Invalid command handling" 1
fi

# Test 13: Stop Non-existent Process
echo -e "\n${BLUE}Test 13: Stop Non-existent Process${NC}"
OUTPUT=$(./bin/controller stop 999999 2>&1)
if echo "$OUTPUT" | grep -q "Error stopping process" || echo "$OUTPUT" | grep -q "not found"; then
    test_result "Stop non-existent process" 0
else
    test_result "Stop non-existent process" 1
fi

# Test 14: Controller Help
echo -e "\n${BLUE}Test 14: Controller Help${NC}"
OUTPUT=$(./bin/controller 2>&1)
if echo "$OUTPUT" | grep -q "Usage:" && echo "$OUTPUT" | grep -q "Commands:"; then
    test_result "Controller help" 0
else
    test_result "Controller help" 1
fi

# Test 15: Monitor Command
echo -e "\n${BLUE}Test 15: Monitor Command${NC}"
OUTPUT=$(./bin/controller monitor)
if echo "$OUTPUT" | grep -q "Monitored processes"; then
    test_result "Monitor command" 0
else
    test_result "Monitor command" 1
fi

# Final Results
echo
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test Results Summary${NC}"
echo -e "${BLUE}========================================${NC}"

PASSED_TESTS=$((TOTAL_TESTS - FAILED_TESTS))
echo -e "Total Tests: $TOTAL_TESTS"
echo -e "${GREEN}Passed: $PASSED_TESTS${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${RED}Failed: $FAILED_TESTS${NC}"
    echo
    echo -e "${GREEN}🎉 ALL TESTS PASSED! 🎉${NC}"
    echo -e "${GREEN}Basic functionality is working correctly.${NC}"
    exit 0
else
    echo -e "${RED}Failed: $FAILED_TESTS${NC}"
    echo
    echo -e "${RED}❌ SOME TESTS FAILED${NC}"
    echo -e "${YELLOW}Please review the failed tests above.${NC}"
    exit 1
fi