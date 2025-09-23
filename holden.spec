Name:             holden
Version:          0.2
Release:          1%{?dist}
Summary:          High-performance process orchestration system

License:          MIT
URL:              https://github.com/pypingou/holden
Source0:          %{name}-%{version}.tar.gz

BuildRequires:    gcc
BuildRequires:    make
BuildRequires:    systemd-rpm-macros

%description
Holden is a high-performance process orchestration application written in C,
featuring a stateless agent architecture for container-based process management.
Named after the 19th century puppeteer Joseph Holden, it provides precise control
over process lifecycles using pidfd-based monitoring. The agent spawns processes
and returns pidfd references via Unix domain socket fd passing, with all process
management delegated to the caller.

%package agent
Summary:          Holden process orchestration agent daemon
Requires:         systemd
%{?sysusers_requires_compat}
Requires(post):   systemd
Requires(preun):  systemd
Requires(postun): systemd

%description agent
The stateless agent component of the Holden process orchestration system. This daemon
runs in containers to spawn processes and return pidfd references via fd passing.
The agent maintains no internal state - all process management is handled by the caller
using the returned pidfds. Supports container namespace inheritance and efficient
process spawning.

%package devel
Summary:          Development files for %{name}
Requires:         %{name} = %{version}-%{release}

%description devel
Development files and headers for the Holden process orchestration system.
Includes protocol definitions and development libraries for building
applications that integrate with Holden's process management capabilities.

%prep
%autosetup

%build
make %{?_smp_mflags} CFLAGS="%{optflags}" LDFLAGS="%{?__global_ldflags}"

%install
# Use Makefile install target with proper DESTDIR
make install DESTDIR=%{buildroot} PREFIX=%{_prefix}

# Create additional directories for RPM-specific files
install -d %{buildroot}%{_unitdir}
install -d %{buildroot}%{_localstatedir}/lib/%{name}
install -d %{buildroot}%{_localstatedir}/run/%{name}
install -d %{buildroot}%{_sysusersdir}

# Install systemd service
install -m 644 holden-agent.service %{buildroot}%{_unitdir}/

# Install sysusers configuration
install -m 644 config/holden.sysusers %{buildroot}%{_sysusersdir}/holden.conf

%check
# Run quick test to verify basic functionality
make all
sh ./test_quick.sh || echo "Tests require agent to be running"

%pre agent
%sysusers_create_compat %{_sysusersdir}/holden.conf

%post agent
%systemd_post holden-agent.service

%preun agent
%systemd_preun holden-agent.service

%postun agent
%systemd_postun_with_restart holden-agent.service

%files
%license %{_docdir}/%{name}/LICENSE
%doc %{_docdir}/%{name}/README.md
%doc %{_docdir}/%{name}/TESTING.md
%{_bindir}/holden-orchestrator

%files agent
%license %{_docdir}/%{name}/LICENSE
%{_libexecdir}/holden-agent
%{_unitdir}/holden-agent.service
%{_sysusersdir}/holden.conf
%config(noreplace) %{_sysconfdir}/%{name}/agent.conf
%attr(755,holden,holden) %dir %{_localstatedir}/lib/%{name}
%attr(755,holden,holden) %dir %{_localstatedir}/run/%{name}

%files devel
%{_includedir}/%{name}/

%changelog
* Tue Sep 23 2025 Pierre-Yves Chibon <pingou@pingoured.fr> - 0.2-1
- Major architecture redesign to stateless pidfd-based approach
- Simplify agent to only spawn processes and return pidfd references via fd passing
- Remove all state management and process tracking from agent
- Add new process orchestrator (renamed from pidfd monitor) for local and agent process management
- Remove obsolete controller and monitor utilities incompatible with new architecture
- Remove stop/list/constraints operations (now handled by caller via pidfds)
- Remove cgroups integration (delegated to caller)
- Update documentation to focus on pidfd-based orchestration approach
- Fix zombie process handling in both agent and orchestrator with proper SIGCHLD reaping

* Tue Sep 23 2025 Pierre-Yves Chibon <pingou@pingoured.fr> - 0.1-6
- Simplify agent to stateless pidfd-based process spawning
- Remove all state management and complex process tracking from agent
- Agent now only spawns processes and returns pidfd references via fd passing
- Add new pidfd monitor demo showing local and agent process management
- Remove stop/list/constraints operations (now handled by caller)
- Remove cgroups integration (delegated to caller)

* Tue Sep 23 2025 Pierre-Yves Chibon <pingou@pingoured.fr> - 0.1-5
- Fix the container PID to host PID mapping logic

* Tue Sep 23 2025 Pierre-Yves Chibon <pingou@pingoured.fr> - 0.1-4
- Improve the controller output
- Fix avoiding zombie processes
- Add --help to the binaries
- Drop the unused holden-agent-wrapper
- Add the container PID and host PID instead of just the first one

* Tue Sep 23 2025 Pierre-Yves Chibon <pingou@pingoured.fr> - 0.1-3
- Fix agent crashing when the child process doesn't exist

* Tue Sep 23 2025 Pierre-Yves Chibon <pingou@pingoured.fr> - 0.1-2
- Make the socket configurable through an environment variable

* Mon Sep 22 2025 Pierre-Yves Chibon <pingou@pingoured.fr> - 0.1-1
- Initial RPM package for Holden process orchestration system
- Named after 19th century puppeteer Joseph Holden
- Split agent into separate subpackage with dedicated user/group
- Add systemd service integration with security hardening
- Include comprehensive documentation and testing guides
- Add development headers package for API integration
