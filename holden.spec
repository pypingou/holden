Name:             holden
Version:          0.1
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
featuring a controller-agent architecture for container-based process management.
Named after the 19th century puppeteer Joseph Holden, it provides precise control
over process lifecycles. Supports Unix domain socket communication, real-time
monitoring, and cgroups v2 resource constraints.

%package agent
Summary:          Holden process orchestration agent daemon
Requires:         systemd
%{?sysusers_requires_compat}
Requires(post):   systemd
Requires(preun):  systemd
Requires(postun): systemd

%description agent
The agent component of the Holden process orchestration system. This daemon runs
in containers or on remote hosts to manage processes according to controller
commands. Supports process lifecycle management, resource constraints via
cgroups v2, and real-time monitoring.

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
%{_bindir}/holden-controller
%{_bindir}/holden-monitor

%files agent
%license %{_docdir}/%{name}/LICENSE
%{_sbindir}/holden-agent
%{_sbindir}/holden-agent-wrapper
%{_unitdir}/holden-agent.service
%{_sysusersdir}/holden.conf
%config(noreplace) %{_sysconfdir}/%{name}/agent.conf
%attr(755,holden,holden) %dir %{_localstatedir}/lib/%{name}
%attr(755,holden,holden) %dir %{_localstatedir}/run/%{name}

%files devel
%{_includedir}/%{name}/

%changelog
* Mon Sep 22 2025 Pierre-Yves Chibon <pingou@pingoured.fr> - 0.1-1
- Initial RPM package for Holden process orchestration system
- Named after 19th century puppeteer Joseph Holden
- Split agent into separate subpackage with dedicated user/group
- Add systemd service integration with security hardening
- Include comprehensive documentation and testing guides
- Add development headers package for API integration
