Name:     biboumi
Version:  ${RPM_VERSION}
Release:  1%{?dist}
Summary:  Lightweight XMPP to IRC gateway

License:  zlib
URL:      http://biboumi.louiz.org
Source0:  http://git.louiz.org/biboumi/snapshot/biboumi-%{version}.tar.xz

BuildRequires: libidn-devel
BuildRequires: expat-devel
BuildRequires: libuuid-devel
BuildRequires: systemd-devel
BuildRequires: cmake
BuildRequires: systemd
BuildRequires: pandoc

%global _hardened_build 1

%global biboumi_confdir %{_sysconfdir}/%{name}


%description
An XMPP gateway that connects to IRC servers and translates between the two
protocols. It can be used to access IRC channels using any XMPP client as if
these channels were XMPP MUCs.


%prep
%setup -q


%build
cmake . -DCMAKE_CXX_FLAGS="%{optflags}" \
      -DCMAKE_BUILD_TYPE=release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DPOLLER=EPOLL \
      -DWITHOUT_BOTAN=1 \
      -DWITH_SYSTEMD=1 \
      -DWITH_LIBIDN=1

make %{?_smp_mflags}


%install
make install DESTDIR=%{buildroot}


%check
make check %{?_smp_mflags}


%files
%{_bindir}/%{name}
%{_mandir}/man1/%{name}.1*
%doc README.rst COPYING doc/biboumi.1.rst
%{_unitdir}/%{name}.service
%config(noreplace) %{biboumi_confdir}/biboumi.cfg


%changelog
* Wed Nov 9 2016 Le Coz Florent <louiz@louiz.org> - 4.0-1
- Update to 4.0 sources

* Thu Aug 4 2016 Le Coz Florent <louiz@louiz.org> - 3.0-1
- Update to 3.0 sources

* Wed Jan 13 2016 Le Coz Florent <louiz@louiz.org> - 2.0-2
- Do not install the systemd unit and configuration files, because
  “make install” does it itself now

* Fri May 29 2015 Le Coz Florent <louiz@louiz.org> - 2.0-1
- Update to 2.0 sources

* Thu Nov 13 2014 Le Coz Florent <louiz@louiz.org> - 1.1-2
- Use the -DWITH(OUT) cmake flags for all optional dependencies
- Build with the correct optflags
- Use hardened_build

* Mon Aug 18 2014 Le Coz Florent <louiz@louiz.org> - 1.1-1
- Update to 1.1 release

* Wed Jun 25 2014 Le Coz Florent <louiz@louiz.org> - 1.0-1
- Spec file written from scratch
