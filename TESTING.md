# Process Orchestration System - Testing Guide

This document describes the testing scripts available for the process orchestration system.

## Test Scripts Overview

### 1. `test_quick.sh` - Quick Functionality Test
**Purpose**: Fast verification of core functionality
**Runtime**: ~10 seconds
**Privileges**: Regular user

**Features Tested**:
- Agent startup and connectivity
- Process start/stop/list operations
- Basic monitoring
- Cleanup verification

**Usage**:
```bash
sh ./test_quick.sh
```

### 2. `test_basic.sh` - Comprehensive Basic Testing
**Purpose**: Thorough testing of all basic functionality
**Runtime**: ~2-3 minutes
**Privileges**: Regular user

**Features Tested**:
- ✅ Agent startup and connectivity
- ✅ Single and multiple process management
- ✅ Process listing with various states
- ✅ Process monitoring with resource usage
- ✅ Error handling (invalid commands, non-existent processes)
- ✅ Command-line argument processing
- ✅ Process cleanup and resource management
- ✅ Stress testing with multiple processes

**Usage**:
```bash
sh ./test_basic.sh
```

**Sample Output**:
```
Process Orchestration Basic Functionality Test
==============================================

✓ PASS: Agent startup
✓ PASS: Agent connectivity
✓ PASS: Initial empty process list
✓ PASS: Start single process (sleep 60)
✓ PASS: Process list with one process
✓ PASS: Start multiple processes
...
🎉 ALL TESTS PASSED! 🎉
```

### 3. `test_advanced.sh` - Advanced Features with cgroups
**Purpose**: Testing resource constraints and advanced features
**Runtime**: ~3-5 minutes
**Privileges**: **ROOT REQUIRED** (for cgroups)

**Features Tested**:
- ✅ cgroups v2 initialization
- ✅ Memory constraints application
- ✅ CPU constraints application
- ✅ Multiple processes with different constraints
- ✅ Resource monitoring with constraints
- ✅ cgroups file verification
- ✅ Constraint error handling
- ✅ Stress testing with constraints
- ✅ cgroups cleanup verification

**Usage**:
```bash
sudo sh ./test_advanced.sh
```

**Requirements**:
- Root privileges
- cgroups v2 support (`/sys/fs/cgroup` writable)
- Python3 (for memory test scripts)

## Test Results Interpretation

### Success Indicators
- ✅ **All core functionality working**: Agent can start/stop/list processes
- ✅ **Communication protocol working**: Controller can connect to agent
- ✅ **Process monitoring working**: Resource usage statistics available
- ✅ **Error handling working**: Graceful handling of invalid requests
- ✅ **Resource constraints working** (advanced): cgroups limits applied

### Expected Warnings
- `Warning: Failed to initialize cgroups, constraints will not work` - Normal when not running as root
- `Permission denied` for cgroups operations - Expected without root privileges

### Failure Scenarios
- Agent fails to start: Check socket permissions, port conflicts
- Process start failures: Check executable permissions, path issues
- Monitor failures: Check process state, timing issues
- Constraint failures: Check cgroups v2 support, root privileges

## Manual Testing

For interactive testing:

```bash
# Terminal 1: Start agent
sudo ./bin/agent

# Terminal 2: Use controller
./bin/controller start sleep 60
./bin/controller list
./bin/controller constrain <pid> 100 50  # 100MB, 50% CPU
./bin/controller stop <pid>

# Terminal 3: Monitor processes
./bin/monitor
```

## Performance Testing

The system has been tested with:
- ✅ Up to 50+ concurrent processes
- ✅ Rapid start/stop cycles
- ✅ Multiple constraint applications
- ✅ Long-running monitoring sessions

## Container Testing

For container environments:

```bash
# Build container with agent
docker run -d --name orchestrator \
  --privileged \
  -v /tmp:/tmp \
  -v /sys/fs/cgroup:/sys/fs/cgroup:rw \
  your-agent-image

# Test from host
./bin/controller start sleep 30
./bin/controller list
```

## Troubleshooting

### Common Issues

1. **"Failed to connect to agent"**
   - Ensure agent is running
   - Check socket path: `/tmp/process_orchestrator.sock`

2. **"Failed to create cgroup"**
   - Run as root: `sudo ./test_advanced.sh`
   - Verify cgroups v2: `mount | grep cgroup`

3. **Process start failures**
   - Check command exists: `which <command>`
   - Verify permissions: `ls -la <command>`

4. **Monitor shows no processes**
   - Check process still running: `ps aux | grep <process>`
   - Verify agent connection

### Debug Mode

Enable verbose output:
```bash
strace -e trace=network,file ./bin/agent
strace -e trace=network,file ./bin/controller list
```

## Test Coverage

- **Protocol Testing**: ✅ All message types tested
- **Error Handling**: ✅ Invalid inputs, missing processes
- **Resource Management**: ✅ Memory leaks, cleanup
- **Concurrency**: ✅ Multiple processes, rapid operations
- **Security**: ✅ Socket permissions, privilege handling