# Holden Process Orchestration System v0.2 - Testing Guide

This document describes testing approaches for the Holden v0.2 pidfd-based stateless architecture.

Named after 19th century puppeteer Joseph Holden, this system provides precise control over process lifecycles using modern Linux pidfd technology.

## Architecture Overview

**v0.2 uses a fundamentally different architecture:**
- **Stateless Agent**: Spawns processes and returns pidfd references via fd passing
- **pidfd Orchestrator**: Manages processes directly using pidfds with poll() monitoring
- **No Agent State**: Agent maintains no process tracking or management
- **Caller Control**: All process management handled by caller via pidfds

## Testing Components

### 1. Agent Testing (Stateless Process Spawning)

**Manual Agent Test:**
```bash
# Terminal 1: Start agent
make all
sudo systemctl start holden-agent
# OR manually: ./bin/agent

# Check agent is listening
ss -lnx | grep holden
# Should show: /tmp/process_orchestrator.sock or configured socket
```

**Agent Verification:**
```bash
# Test agent responsiveness (ping operation)
echo "The agent only supports spawn + ping operations in v0.2"
# No list/stop commands - those are handled by orchestrator via pidfds
```

### 2. Orchestrator Testing (pidfd Management)

**Basic Orchestrator Test:**
```bash
# Build orchestrator
make all

# Test local + agent process orchestration
export HOLDEN_SOCKET_PATH=/tmp/process_orchestrator.sock
./bin/orchestrator 'sleep 5' 'sleep 10'

# Expected output:
# Starting pidfd orchestrator demo...
# Local command: sleep 5
# Agent command: sleep 10
# Spawned local process sleep with PID xxx, pidfd 3
# Spawned agent process sleep with PID yyy, pidfd 5
# Monitoring processes (restart count: 0)...
# [timestamp] Local process died, restarting...
# [timestamp] Agent process died, restarting...
```

**Features Demonstrated:**
- ✅ Local process spawning via fork() + pidfd_open()
- ✅ Agent process spawning via Unix socket + pidfd receiving
- ✅ poll() monitoring on pidfds for immediate death detection
- ✅ Automatic process restart when processes die
- ✅ Proper cleanup and resource management

### 3. Container/QM Testing (AutoSD Environment)

**QM Partition Testing:**
```bash
# In AutoSD environment with QM partition
export HOLDEN_SOCKET_PATH=/run/holden/qm_orchestrator.sock

# Test orchestrator managing processes across partitions
holden-orchestrator 'sleep 30' 'sleep 60'

# Verify processes running in different contexts:
# - Local process runs in current context
# - Agent process runs in QM partition context
```

**Container Verification:**
```bash
# Check agent running in container
podman exec qm ps aux | grep holden-agent
podman exec qm systemctl status holden-agent

# Check processes spawned by agent inherit container context
podman exec qm ps aux | grep sleep
```

## Integration Testing

### 4. End-to-End Workflow

**Complete Process Lifecycle:**
```bash
# 1. Start agent (if not running)
sudo systemctl start holden-agent

# 2. Run orchestrator with different process types
./bin/orchestrator '/bin/echo "Local process"' '/bin/echo "Agent process"'

# 3. Test with longer-running processes
./bin/orchestrator 'python3 -c "import time; time.sleep(30)"' 'bash -c "sleep 30"'

# 4. Test restart behavior
./bin/orchestrator '/bin/true' '/bin/true'  # Processes exit immediately, should restart
```

### 5. Error Handling

**Test Error Scenarios:**
```bash
# Non-existent commands
./bin/orchestrator '/nonexistent/command' '/bin/true'
# Expected: Error message for local process

./bin/orchestrator '/bin/true' '/nonexistent/command'
# Expected: Error from agent

# Socket connectivity issues
HOLDEN_SOCKET_PATH=/invalid/path ./bin/orchestrator '/bin/true' '/bin/true'
# Expected: Agent connection failure

# Permission issues
./bin/orchestrator '/root/some-restricted-file' '/bin/true'
# Expected: Local process permission error
```

## Performance Testing

### 6. Restart Performance

**Rapid Restart Testing:**
```bash
# Test restart speed with fast-exiting processes
timeout 30s ./bin/orchestrator '/bin/true' '/bin/true'
# Should show many rapid restarts
```

**Resource Usage:**
```bash
# Monitor orchestrator resource usage
time timeout 60s ./bin/orchestrator 'sleep 2' 'sleep 3'

# Monitor agent memory (should be minimal - stateless)
ps aux | grep holden-agent
cat /proc/$(pgrep holden-agent)/status | grep -E 'VmRSS|VmSize'
```

## Expected Behavior

### Success Indicators

- ✅ **Agent Connectivity**: Orchestrator connects to agent socket
- ✅ **Process Spawning**: Both local and agent processes spawn successfully
- ✅ **pidfd Management**: pidfds received and managed properly
- ✅ **Death Detection**: Immediate detection when processes die (via poll())
- ✅ **Auto Restart**: Processes restart automatically without delay
- ✅ **Clean Timestamps**: Properly formatted restart timestamps
- ✅ **Resource Cleanup**: No resource leaks or zombie processes

### Normal Output Patterns

```
Starting pidfd orchestrator demo...
Local command: <cmd1>
Agent command: <cmd2>
Press Ctrl+C to exit

Spawned local process <cmd1> with PID <pid1>, pidfd <fd1>
Spawned agent process <cmd2> with PID <pid2>, pidfd <fd2>
Monitoring processes (restart count: 0)...
[timestamp] Local process died, restarting...
Spawned local process <cmd1> with PID <pid3>, pidfd <fd1>
[timestamp] Agent process died, restarting...
Spawned agent process <cmd2> with PID <pid4>, pidfd <fd2>
Monitoring processes (restart count: 2)...
```

## Troubleshooting

### Common Issues

1. **"Failed to spawn agent process"**
   - Check agent is running: `systemctl status holden-agent`
   - Verify socket path: `ls -la /tmp/process_orchestrator.sock`
   - Check HOLDEN_SOCKET_PATH environment variable

2. **"Connection refused"**
   - Agent not running or socket path incorrect
   - Check socket permissions
   - Verify agent listening: `ss -lnx | grep holden`

3. **No restart behavior**
   - Check if processes are actually exiting
   - Verify pidfd monitoring is working
   - Look for zombie processes: `ps aux | grep Z`

4. **Permission denied errors**
   - Local process: Check command exists and is executable
   - Agent process: Check agent has permission to spawn command

### Debug Mode

**Verbose Output:**
```bash
# Monitor system calls
strace -e pidfd_open,poll,sendmsg,recvmsg ./bin/orchestrator 'sleep 1' 'sleep 1'

# Monitor socket activity
strace -e connect,sendmsg,recvmsg ./bin/orchestrator '/bin/true' '/bin/true'

# Check agent logs
journalctl -u holden-agent.service -f
```

## Migration from v0.1

**Key Changes:**
- ❌ No `controller` or `monitor` utilities
- ❌ No `list`, `stop`, `constrain` commands
- ❌ No agent process tracking or state
- ❌ No cgroups integration in agent
- ✅ Simple `orchestrator` demonstration
- ✅ Direct pidfd management by caller
- ✅ Agent just spawns + returns pidfds
- ✅ Caller handles all process lifecycle

**Philosophy Shift:**
- **v0.1**: Agent managed process state and lifecycle
- **v0.2**: Agent is stateless, caller owns all process management via pidfds

## Container Testing Notes

For AutoSD/container environments:
- Agent inherits container context (namespaces, cgroups, etc.)
- Spawned processes automatically inherit agent's container context
- Host orchestrator can manage container processes via pidfds
- No container escape - processes run within agent's container boundaries

## Performance Characteristics

**v0.2 Performance Benefits:**
- **Faster Death Detection**: poll() on pidfd vs periodic process checking
- **Lower Agent Memory**: No process tracking state
- **Immediate Restart**: No agent-side delays
- **Better Scalability**: Event-driven monitoring vs polling
- **No Race Conditions**: pidfd eliminates PID reuse issues