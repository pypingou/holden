# Holden Process Orchestration System

A high-performance process orchestration application written in C, featuring a controller-agent architecture for container-based process management. Named after the 19th century puppeteer Joseph Holden, it provides precise control over process lifecycles.

## Architecture

- **Controller**: Orchestrates processes, provides CLI interface
- **Agent**: Runs in containers, manages processes, applies constraints
- **Monitor**: Continuously monitors process health and resource usage
- **Communication**: Unix domain sockets for efficient IPC
- **Constraints**: cgroups v2 for memory and CPU limits

## Features

- Start/stop processes remotely
- Process monitoring with resource usage
- Memory and CPU constraints via cgroups
- Container-friendly architecture
- Real-time process health monitoring

## Building

```bash
make all
```

This creates three binaries in the `bin/` directory:
- `agent` - Process agent
- `controller` - Main controller
- `monitor` - Process monitor

## Usage

### 1. Start the Agent (typically in a container)

```bash
sudo ./bin/agent
```

The agent listens on `/tmp/process_orchestrator.sock` for controller connections.

### 2. Use the Controller

Start a process:
```bash
./bin/controller start sleep 60
./bin/controller start python3 -c "import time; time.sleep(100)"
```

List running processes:
```bash
./bin/controller list
```

Stop a process:
```bash
./bin/controller stop <pid>
```

Apply resource constraints:
```bash
# Limit to 100MB memory, 50% CPU
./bin/controller constrain <pid> 100 50
```

Show monitored processes:
```bash
./bin/controller monitor
```

### 3. Monitor Processes

Start the continuous monitor:
```bash
./bin/monitor
```

This provides real-time monitoring with CPU and memory usage statistics.

## Container Usage

### Agent Container

```dockerfile
FROM alpine:latest
RUN apk add --no-cache libc6-compat
COPY bin/agent /usr/local/bin/
VOLUME ["/tmp"]
CMD ["/usr/local/bin/agent"]
```

### Running with Podman

```bash
# Build and run agent container
podman build -t holden-agent .
podman run -d --name holden-agent \
  --privileged \
  -v /tmp:/tmp \
  -v /sys/fs/cgroup:/sys/fs/cgroup:rw \
  holden-agent

# Use controller from host
./bin/controller start sleep 30
./bin/controller list
```

## Requirements

- Linux with cgroups v2 support
- Root privileges for cgroups operations
- GCC with C99 support

## File Structure

- `protocol.h/c` - Communication protocol
- `agent.c` - Process agent
- `controller.c` - Main controller
- `monitor.c` - Process monitor
- `cgroups.h/c` - cgroups v2 integration
- `Makefile` - Build system

## Protocol

The system uses a binary message protocol over Unix domain sockets:

- `START_PROCESS` - Start a new process
- `PROCESS_STARTED` - Process started successfully (returns PID)
- `PROCESS_ERROR` - Error occurred
- `LIST_PROCESSES` - List running processes
- `STOP_PROCESS` - Stop a process
- `APPLY_CONSTRAINTS` - Apply resource limits

## Security

- Unix domain sockets provide secure local communication
- cgroups provide process isolation
- No network exposure by default
- Designed for container environments

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.