#!/bin/bash

# Process Orchestration System Test Script

echo "Process Orchestration System Test"
echo "=================================="
echo

# Check if running as root for cgroups
if [ "$EUID" -ne 0 ]; then
    echo "WARNING: Not running as root. cgroups functionality may not work."
    echo "For full testing, run: sudo $0"
    echo
fi

# Start agent in background
echo "1. Starting agent..."
./bin/agent &
AGENT_PID=$!
sleep 2

# Test basic functionality
echo "2. Testing controller help..."
./bin/controller
echo

echo "3. Testing process listing (should be empty)..."
./bin/controller list
echo

echo "4. Starting a test process (sleep 30)..."
./bin/controller start sleep 30
echo

echo "5. Listing processes (should show sleep)..."
./bin/controller list
echo

echo "6. Testing monitor (3 seconds)..."
timeout 3 ./bin/monitor
echo

echo "7. Showing monitored processes..."
./bin/controller monitor
echo

# Get the PID of the sleep process for constraint testing
SLEEP_PID=$(ps aux | grep "sleep 30" | grep -v grep | awk '{print $2}' | head -1)

if [ ! -z "$SLEEP_PID" ]; then
    echo "8. Testing constraints on PID $SLEEP_PID (100MB memory, 50% CPU)..."
    ./bin/controller constrain $SLEEP_PID 100 50
    echo

    echo "9. Stopping process $SLEEP_PID..."
    ./bin/controller stop $SLEEP_PID
    echo
else
    echo "8. Could not find sleep process for constraint testing"
    echo
fi

echo "10. Final process list (should be empty)..."
./bin/controller list
echo

# Clean up
echo "11. Stopping agent..."
kill $AGENT_PID
wait $AGENT_PID 2>/dev/null

echo "Test completed successfully!"
echo
echo "Manual testing:"
echo "  1. Start agent: sudo ./bin/agent"
echo "  2. In another terminal: ./bin/controller start <command>"
echo "  3. Monitor: ./bin/monitor"