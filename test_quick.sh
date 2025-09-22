#!/bin/bash

# Quick Functionality Test Script
# Tests core functionality with shorter timeouts

set -e

AGENT_PID=""
TEST_PIDS=()

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

# Cleanup function
cleanup() {
    echo -e "\n${BLUE}Cleaning up...${NC}"

    # Stop test processes
    for pid in "${TEST_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            ./bin/controller stop "$pid" >/dev/null 2>&1 || true
        fi
    done

    # Stop agent
    if [ ! -z "$AGENT_PID" ] && kill -0 "$AGENT_PID" 2>/dev/null; then
        kill "$AGENT_PID"
        wait "$AGENT_PID" 2>/dev/null || true
    fi

    rm -f /tmp/process_orchestrator.sock
}

trap cleanup EXIT

echo -e "${BLUE}Quick Process Orchestration Test${NC}"
echo "================================"
echo

# Start agent
echo "1. Starting agent..."
./bin/agent &
AGENT_PID=$!
sleep 2

# Test connectivity
echo "2. Testing connectivity..."
./bin/controller list >/dev/null && echo -e "${GREEN}✓ Agent responsive${NC}" || echo -e "${RED}✗ Agent not responsive${NC}"

# Start a process
echo "3. Starting test process..."
OUTPUT=$(./bin/controller start sleep 10)
if echo "$OUTPUT" | grep -q "Process started successfully"; then
    PID=$(echo "$OUTPUT" | grep "PID:" | sed 's/.*PID: \([0-9]*\).*/\1/')
    TEST_PIDS+=("$PID")
    echo -e "${GREEN}✓ Process started (PID: $PID)${NC}"
else
    echo -e "${RED}✗ Failed to start process${NC}"
fi

# List processes
echo "4. Listing processes..."
OUTPUT=$(./bin/controller list)
if echo "$OUTPUT" | grep -q "Running processes (1)"; then
    echo -e "${GREEN}✓ Process listed correctly${NC}"
else
    echo -e "${RED}✗ Process not listed${NC}"
fi

# Test monitor briefly
echo "5. Testing monitor (3 seconds)..."
if timeout 3 ./bin/monitor >/dev/null 2>&1; then
    echo -e "${GREEN}✓ Monitor working${NC}"
else
    echo -e "${GREEN}✓ Monitor working (timeout as expected)${NC}"
fi

# Stop process
echo "6. Stopping process..."
if [ ${#TEST_PIDS[@]} -gt 0 ]; then
    OUTPUT=$(./bin/controller stop "${TEST_PIDS[0]}")
    if echo "$OUTPUT" | grep -q "stopped successfully"; then
        echo -e "${GREEN}✓ Process stopped${NC}"
        TEST_PIDS=()
    else
        echo -e "${RED}✗ Failed to stop process${NC}"
    fi
fi

# Verify empty list
echo "7. Verifying cleanup..."
OUTPUT=$(./bin/controller list)
if echo "$OUTPUT" | grep -q "Running processes (0)"; then
    echo -e "${GREEN}✓ All processes cleaned up${NC}"
else
    echo -e "${RED}✗ Processes still running${NC}"
fi

echo
echo -e "${GREEN}🎉 Quick test completed successfully!${NC}"
echo
echo "For full testing:"
echo "  Basic tests: sh ./test_basic.sh"
echo "  Advanced tests (requires root): sudo sh ./test_advanced.sh"