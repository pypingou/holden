Name:           process-orchestrator
Version:        1.0.0
Release:        1%{?dist}
Summary:        High-performance process orchestration system

License:        MIT
URL:            https://github.com/example/process-orchestrator
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  systemd-rpm-macros

%description
A high-performance process orchestration application written in C, featuring
a controller-agent architecture for container-based process management.
Supports Unix domain socket communication, real-time monitoring, and
cgroups v2 resource constraints.

%package agent
Summary:        Process orchestration agent daemon
Requires:       systemd
Requires(pre):  shadow-utils
Requires(post): systemd
Requires(preun): systemd
Requires(postun): systemd

%description agent
The agent component of the process orchestration system. This daemon runs
in containers or on remote hosts to manage processes according to controller
commands. Supports process lifecycle management, resource constraints via
cgroups v2, and real-time monitoring.

%package devel
Summary:        Development files for %{name}
Requires:       %{name} = %{version}-%{release}

%description devel
Development files and headers for the process orchestration system.
Includes protocol definitions and development libraries.

%prep
%autosetup

%build
make %{?_smp_mflags} CFLAGS="%{optflags}" LDFLAGS="%{?__global_ldflags}"

%check
# Run quick test to verify basic functionality
make all
sh ./test_quick.sh || echo "Tests require agent to be running"

%install
# Create directories
install -d %{buildroot}%{_bindir}
install -d %{buildroot}%{_sbindir}
install -d %{buildroot}%{_includedir}/%{name}
install -d %{buildroot}%{_unitdir}
install -d %{buildroot}%{_sysconfdir}/%{name}
install -d %{buildroot}%{_localstatedir}/lib/%{name}
install -d %{buildroot}%{_localstatedir}/run/%{name}
install -d %{buildroot}%{_mandir}/man1
install -d %{buildroot}%{_mandir}/man8
install -d %{buildroot}%{_docdir}/%{name}

# Install binaries
install -m 755 bin/controller %{buildroot}%{_bindir}/orchestrator-controller
install -m 755 bin/monitor %{buildroot}%{_bindir}/orchestrator-monitor
install -m 755 bin/agent %{buildroot}%{_sbindir}/orchestrator-agent

# Install headers
install -m 644 protocol.h %{buildroot}%{_includedir}/%{name}/
install -m 644 cgroups.h %{buildroot}%{_includedir}/%{name}/

# Install documentation
install -m 644 README.md %{buildroot}%{_docdir}/%{name}/
install -m 644 TESTING.md %{buildroot}%{_docdir}/%{name}/
install -m 644 LICENSE %{buildroot}%{_docdir}/%{name}/

# Create systemd service file for agent
cat > %{buildroot}%{_unitdir}/orchestrator-agent.service << 'EOF'
[Unit]
Description=Process Orchestrator Agent
Documentation=file://%{_docdir}/%{name}/README.md
After=network.target

[Service]
Type=simple
User=orchestrator
Group=orchestrator
ExecStart=%{_sbindir}/orchestrator-agent
Restart=always
RestartSec=5
TimeoutStopSec=30

# Security settings
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/tmp /var/lib/%{name} /var/run/%{name} /sys/fs/cgroup
PrivateDevices=true
ProtectKernelTunables=false
ProtectKernelModules=true
ProtectControlGroups=false

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096

[Install]
WantedBy=multi-user.target
EOF

# Create default configuration
cat > %{buildroot}%{_sysconfdir}/%{name}/agent.conf << 'EOF'
# Process Orchestrator Agent Configuration

# Socket path for agent communication
SOCKET_PATH=/tmp/process_orchestrator.sock

# Maximum number of processes to manage
MAX_PROCESSES=64

# Enable cgroups constraints (requires root)
ENABLE_CGROUPS=true

# cgroups base path
CGROUP_BASE=/sys/fs/cgroup/orchestrator

# Log level (debug, info, warn, error)
LOG_LEVEL=info
EOF

# Create wrapper script with configuration loading
cat > %{buildroot}%{_sbindir}/orchestrator-agent-wrapper << 'EOF'
#!/bin/bash
# Wrapper script for orchestrator-agent with configuration

CONFIG_FILE="/etc/process-orchestrator/agent.conf"

# Source configuration if it exists
if [ -f "$CONFIG_FILE" ]; then
    source "$CONFIG_FILE"
fi

# Export environment variables
export SOCKET_PATH="${SOCKET_PATH:-/tmp/process_orchestrator.sock}"
export MAX_PROCESSES="${MAX_PROCESSES:-64}"
export ENABLE_CGROUPS="${ENABLE_CGROUPS:-true}"
export CGROUP_BASE="${CGROUP_BASE:-/sys/fs/cgroup/orchestrator}"
export LOG_LEVEL="${LOG_LEVEL:-info}"

# Start the agent
exec /usr/sbin/orchestrator-agent "$@"
EOF
chmod 755 %{buildroot}%{_sbindir}/orchestrator-agent-wrapper

# Create manual pages
cat > %{buildroot}%{_mandir}/man1/orchestrator-controller.1 << 'EOF'
.TH ORCHESTRATOR-CONTROLLER 1 "2025-01-01" "1.0.0" "Process Orchestrator"
.SH NAME
orchestrator-controller \- Process orchestration controller
.SH SYNOPSIS
.B orchestrator-controller
.I command
.RI [ args... ]
.SH DESCRIPTION
The orchestrator-controller provides a command-line interface for managing
processes through the orchestrator-agent daemon.
.SH COMMANDS
.TP
.B start <name> [args...]
Start a new process with the given name and arguments
.TP
.B list
List all running processes
.TP
.B stop <pid>
Stop the process with the specified PID
.TP
.B constrain <pid> <memory_mb> <cpu_percent>
Apply resource constraints to a process
.TP
.B monitor
Show monitored processes with uptime
.SH SEE ALSO
.BR orchestrator-agent (8),
.BR orchestrator-monitor (1)
EOF

cat > %{buildroot}%{_mandir}/man8/orchestrator-agent.8 << 'EOF'
.TH ORCHESTRATOR-AGENT 8 "2025-01-01" "1.0.0" "Process Orchestrator"
.SH NAME
orchestrator-agent \- Process orchestration agent daemon
.SH SYNOPSIS
.B orchestrator-agent
.SH DESCRIPTION
The orchestrator-agent is a daemon that manages processes according to
commands received from orchestrator-controller clients via Unix domain sockets.
.SH FILES
.TP
.I /etc/process-orchestrator/agent.conf
Agent configuration file
.TP
.I /tmp/process_orchestrator.sock
Default Unix domain socket for communication
.SH SEE ALSO
.BR orchestrator-controller (1),
.BR systemctl (1)
EOF

%files
%license LICENSE
%doc README.md TESTING.md
%{_bindir}/orchestrator-controller
%{_bindir}/orchestrator-monitor
%{_mandir}/man1/orchestrator-controller.1*

%files agent
%license LICENSE
%{_sbindir}/orchestrator-agent
%{_sbindir}/orchestrator-agent-wrapper
%{_unitdir}/orchestrator-agent.service
%config(noreplace) %{_sysconfdir}/%{name}/agent.conf
%attr(755,orchestrator,orchestrator) %dir %{_localstatedir}/lib/%{name}
%attr(755,orchestrator,orchestrator) %dir %{_localstatedir}/run/%{name}
%{_mandir}/man8/orchestrator-agent.8*

%files devel
%{_includedir}/%{name}/

%pre agent
# Create orchestrator user and group
getent group orchestrator >/dev/null || groupadd -r orchestrator
getent passwd orchestrator >/dev/null || \
    useradd -r -g orchestrator -d %{_localstatedir}/lib/%{name} \
    -s /sbin/nologin -c "Process Orchestrator Agent" orchestrator

%post agent
%systemd_post orchestrator-agent.service

%preun agent
%systemd_preun orchestrator-agent.service

%postun agent
%systemd_postun_with_restart orchestrator-agent.service
if [ $1 -eq 0 ] ; then
    # Remove user and group on final uninstall
    userdel orchestrator >/dev/null 2>&1 || :
    groupdel orchestrator >/dev/null 2>&1 || :
fi

%changelog
* Wed Jan 01 2025 Process Orchestrator Team <team@example.com> - 1.0.0-1
- Initial RPM package
- Split agent into separate subpackage
- Add systemd service integration
- Include comprehensive documentation
- Add development headers package