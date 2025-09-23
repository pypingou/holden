# Holden Process Orchestration System

A high-performance process orchestration application written in C, featuring a stateless agent architecture for container-based process management. Named after the 19th century puppeteer Joseph Holden, it provides precise control over process lifecycles using pidfd-based monitoring.

## Architecture

- **Stateless Agent**: Spawns processes in containers and returns pidfd references via fd passing
- **pidfd Monitor**: Demonstrates pidfd-based process monitoring and restart capability
- **Communication**: Unix domain sockets for efficient IPC with fd passing
- **Process Management**: Caller-controlled using pidfds, no agent state

## Key Features

- **Stateless Agent**: No internal process tracking or state management
- **pidfd-based Monitoring**: Efficient process monitoring using Linux pidfds
- **Container Namespace Inheritance**: Spawned processes inherit agent's container context
- **File Descriptor Passing**: Agent returns pidfd references to caller via Unix sockets
- **Process Restart Logic**: Automatic restart capability using pidfd polling
- **Simplified Architecture**: Agent only spawns, caller manages everything else

## Building

```bash
make all
```

This creates two binaries in the `bin/` directory:
- `agent` - Stateless process spawning agent
- `pidfd_monitor` - pidfd-based monitor demonstration

## Usage

### 1. Start the Agent (typically in a container)

```bash
./bin/agent
```

The agent listens on `/tmp/process_orchestrator.sock` by default. Configure with `HOLDEN_SOCKET_PATH` environment variable.

### 2. Use the pidfd Monitor

The pidfd monitor demonstrates the intended usage pattern:

```bash
# Monitor local and agent-spawned processes with automatic restart
./bin/pidfd_monitor "/bin/sleep 5" "/usr/bin/sleep 10"
```

This example:
1. Spawns `/bin/sleep 5` locally using fork() and gets its pidfd
2. Spawns `/usr/bin/sleep 10` via the agent and receives its pidfd
3. Monitors both processes using poll() on their pidfds
4. Automatically restarts processes when they die


## Agent Architecture

The simplified agent:

1. **Takes command line from caller**
2. **Spawns process** (inherits container/qm context)
3. **Calls pidfd_open()** to get process reference
4. **Returns pidfd via fd passing** over Unix socket
5. **Maintains no state** - all management delegated to caller

```c
// Agent workflow:
pid_t pid = fork();
// ... child execs command ...
int pidfd = pidfd_open(pid, 0);
send_fd(socket, pidfd);  // Send pidfd to caller
close(pidfd);            // Agent doesn't keep it
```

## Process Management Philosophy

With the new architecture:

- **Agent**: Stateless process spawner only
- **Caller**: Receives pidfds and manages processes directly
- **No Agent State**: No process tracking, lists, or management in agent
- **pidfd Control**: Caller uses pidfds for stop, monitor, wait operations
- **Container Context**: Spawned processes inherit agent's namespace/cgroup context

## Container Usage

### Agent Container

```dockerfile
FROM alpine:latest
RUN apk add --no-cache libc6-compat
COPY bin/agent /usr/local/bin/
CMD ["/usr/local/bin/agent"]
```

### Running with Container Orchestration

```bash
# Agent provides qm/container context to spawned processes
podman run -d --name holden-agent \
  -v /run/holden:/run/holden \
  holden-agent

# Use pidfd monitor to spawn and manage processes
./bin/pidfd_monitor "your-app --config=prod" "monitoring-daemon"
```

## Requirements

- Linux with pidfd support (kernel 5.3+)
- Container environment for agent (optional)
- GCC with C99 support

## File Structure

- `protocol.h/c` - Communication protocol definitions
- `agent.c` - Stateless process spawning agent
- `orchestrator.c` - pidfd-based process orchestrator demonstration
- `Makefile` - Build system

## Protocol

The agent supports:

- `MSG_START_PROCESS` - Spawn process and return pidfd via fd passing
- `MSG_PING` - Health check

Removed operations (handled by caller):
- ~~`LIST_PROCESSES`~~ - No agent state to list
- ~~`STOP_PROCESS`~~ - Caller uses pidfd directly
- ~~`APPLY_CONSTRAINTS`~~ - Caller applies via pidfd

## Usage Philosophy

**Simple Model**: Agent spawns, caller manages via pidfd
```bash
pidfd_monitor app1 app2  # Agent spawns, returns pidfds
# Caller polls pidfds, handles restart, stop, etc.
```

The agent is a pure process spawner - all management is handled by the caller using the returned pidfds.

## Security

- Unix domain sockets provide secure local communication
- File descriptor passing for pidfd security
- No network exposure by default
- Container namespace inheritance
- No agent state to compromise

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.