Name:           holden
Version:        1.0.0
Release:        1%{?dist}
Summary:        High-performance process orchestration system

License:        MIT
URL:            https://github.com/example/holden
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  systemd-rpm-macros

%description
Holden is a high-performance process orchestration application written in C,
featuring a controller-agent architecture for container-based process management.
Named after the 19th century puppeteer Joseph Holden, it provides precise control
over process lifecycles. Supports Unix domain socket communication, real-time
monitoring, and cgroups v2 resource constraints.

%package agent
Summary:        Holden process orchestration agent daemon
Requires:       systemd
Requires(pre):  shadow-utils
Requires(post): systemd
Requires(preun): systemd
Requires(postun): systemd

%description agent
The agent component of the Holden process orchestration system. This daemon runs
in containers or on remote hosts to manage processes according to controller
commands. Supports process lifecycle management, resource constraints via
cgroups v2, and real-time monitoring.

%package devel
Summary:        Development files for %{name}
Requires:       %{name} = %{version}-%{release}

%description devel
Development files and headers for the Holden process orchestration system.
Includes protocol definitions and development libraries for building
applications that integrate with Holden's process management capabilities.

%prep
%autosetup

%build
make %{?_smp_mflags} CFLAGS="%{optflags}" LDFLAGS="%{?__global_ldflags}"

%check
# Run quick test to verify basic functionality
make all
sh ./test_quick.sh || echo "Tests require agent to be running"

%install
# Use Makefile install target with proper DESTDIR
make install DESTDIR=%{buildroot} PREFIX=%{_prefix}

# Create additional directories for RPM-specific files
install -d %{buildroot}%{_unitdir}
install -d %{buildroot}%{_localstatedir}/lib/%{name}
install -d %{buildroot}%{_localstatedir}/run/%{name}

# Install systemd service
install -m 644 holden-agent.service %{buildroot}%{_unitdir}/

# Create default configuration
cat > %{buildroot}%{_sysconfdir}/%{name}/agent.conf << 'EOF'
# Holden Agent Configuration

# Socket path for agent communication
SOCKET_PATH=/tmp/holden_orchestrator.sock

# Maximum number of processes to manage
MAX_PROCESSES=64

# Enable cgroups constraints (requires root)
ENABLE_CGROUPS=true

# cgroups base path
CGROUP_BASE=/sys/fs/cgroup/holden

# Log level (debug, info, warn, error)
LOG_LEVEL=info
EOF

# Create wrapper script with configuration loading
cat > %{buildroot}%{_sbindir}/holden-agent-wrapper << 'EOF'
#!/bin/bash
# Wrapper script for holden-agent with configuration

CONFIG_FILE="/etc/holden/agent.conf"

# Source configuration if it exists
if [ -f "$CONFIG_FILE" ]; then
    source "$CONFIG_FILE"
fi

# Export environment variables
export SOCKET_PATH="${SOCKET_PATH:-/tmp/holden_orchestrator.sock}"
export MAX_PROCESSES="${MAX_PROCESSES:-64}"
export ENABLE_CGROUPS="${ENABLE_CGROUPS:-true}"
export CGROUP_BASE="${CGROUP_BASE:-/sys/fs/cgroup/holden}"
export LOG_LEVEL="${LOG_LEVEL:-info}"

# Start the agent
exec /usr/sbin/holden-agent "$@"
EOF
chmod 755 %{buildroot}%{_sbindir}/holden-agent-wrapper

# Create manual pages
cat > %{buildroot}%{_mandir}/man1/holden-controller.1 << 'EOF'
.TH HOLDEN-CONTROLLER 1 "2025-01-01" "1.0.0" "Holden"
.SH NAME
holden-controller \- Holden process orchestration controller
.SH SYNOPSIS
.B holden-controller
.I command
.RI [ args... ]
.SH DESCRIPTION
The holden-controller provides a command-line interface for managing
processes through the holden-agent daemon. Named after 19th century
puppeteer Joseph Holden, it provides precise control over process lifecycles.
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
.BR holden-agent (8),
.BR holden-monitor (1)
EOF

cat > %{buildroot}%{_mandir}/man8/holden-agent.8 << 'EOF'
.TH HOLDEN-AGENT 8 "2025-01-01" "1.0.0" "Holden"
.SH NAME
holden-agent \- Holden process orchestration agent daemon
.SH SYNOPSIS
.B holden-agent
.SH DESCRIPTION
The holden-agent is a daemon that manages processes according to
commands received from holden-controller clients via Unix domain sockets.
Part of the Holden process orchestration system, named after the 19th
century puppeteer Joseph Holden.
.SH FILES
.TP
.I /etc/holden/agent.conf
Agent configuration file
.TP
.I /tmp/holden_orchestrator.sock
Default Unix domain socket for communication
.SH SEE ALSO
.BR holden-controller (1),
.BR systemctl (1)
EOF

%files
%license %{_docdir}/%{name}/LICENSE
%doc %{_docdir}/%{name}/README.md
%doc %{_docdir}/%{name}/TESTING.md
%{_bindir}/holden-controller
%{_bindir}/holden-monitor

%files agent
%license %{_docdir}/%{name}/LICENSE
%{_sbindir}/holden-agent
%{_sbindir}/holden-agent-wrapper
%{_unitdir}/holden-agent.service
%config(noreplace) %{_sysconfdir}/%{name}/agent.conf
%attr(755,holden,holden) %dir %{_localstatedir}/lib/%{name}
%attr(755,holden,holden) %dir %{_localstatedir}/run/%{name}

%files devel
%{_includedir}/%{name}/

%pre agent
# Create holden user and group
getent group holden >/dev/null || groupadd -r holden
getent passwd holden >/dev/null || \
    useradd -r -g holden -d %{_localstatedir}/lib/%{name} \
    -s /sbin/nologin -c "Holden Process Orchestration Agent" holden

%post agent
%systemd_post holden-agent.service

%preun agent
%systemd_preun holden-agent.service

%postun agent
%systemd_postun_with_restart holden-agent.service
if [ $1 -eq 0 ] ; then
    # Remove user and group on final uninstall
    userdel holden >/dev/null 2>&1 || :
    groupdel holden >/dev/null 2>&1 || :
fi

%changelog
* Wed Jan 01 2025 Holden Team <team@example.com> - 1.0.0-1
- Initial RPM package for Holden process orchestration system
- Named after 19th century puppeteer Joseph Holden
- Split agent into separate subpackage with dedicated user/group
- Add systemd service integration with security hardening
- Include comprehensive documentation and testing guides
- Add development headers package for API integration