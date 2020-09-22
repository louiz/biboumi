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
BuildRequires: sqlite-devel
BuildRequires: postgresql-devel
BuildRequires: botan2-devel
BuildRequires: cmake
BuildRequires: systemd
BuildRequires: python3-sphinx

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
      -DWITH_BOTAN=1 \
      -DWITH_SYSTEMD=1 \
      -DWITH_LIBIDN=1 \
      -DWITH_SQLITE3=1 \
      -DWITH_POSTGRESQL=1

make %{?_smp_mflags}
make man SPHINXBUILD=sphinx-build-3

%install
make install DESTDIR=%{buildroot}


%check
make check %{?_smp_mflags}


%files
%{_bindir}/%{name}
%{_mandir}/man1/%{name}.1*
%doc README.rst COPYING doc/*.rst
%{_unitdir}/%{name}.service
%config(noreplace) %{biboumi_confdir}/*policy.txt


%changelog
-* ${RPM_DATE} Le Coz Florent <louiz@louiz.org> - ${RPM_VERSION}-1
-- Build latest git revision

* Tue Sep 22 2020 Le Coz Florent <louiz@louiz.org> - 9.0-1
  Update to version 9.0

* Sat May 9 2020 Le Coz Florent <louiz@louiz.org> - 8.5-1
  Update to version 8.5

* Tue Feb 25 2020 Le Coz Florent <louiz@louiz.org> - 8.4-1
  Update to version 8.4

* Wed Jun 1 2018 Le Coz Florent <louiz@louiz.org> - 8.3-1
  Update to version 8.3

* Fri May 25 2018 Le Coz Florent <louiz@louiz.org> - 8.2-1
  Update to version 8.2

* Mon May 14 2018 Le Coz Florent <louiz@louiz.org> - 8.1-1
  Update to version 8.1

* Wed May 2 2018 Le Coz Florent <louiz@louiz.org> - 8.0-1
  Update to version 8.0

* Wed Jan 24 2018 Le Coz Florent <louiz@louiz.org> - 7.2-1
  Update to version 7.2

* Mon Jan 22 2018 Le Coz Florent <louiz@louiz.org> - 7.1-1
  Update to version 7.1

* Wed Jan 17 2018 Le Coz Florent <louiz@louiz.org> - 7.0-1
  Update to version 7.0

* Wed Oct 4 2017 Le Coz Florent <louiz@louiz.org> - 6.1-1
  Update to version 6.1

* Tue Sep 19 2017 Le Coz Florent <louiz@louiz.org> - 6.0-1
  Update to version 6.0

* Wed May 24 2017 Le Coz Florent <louiz@louiz.org> - 5.0-1
- Update to version 5.0

* Wed May 2 2017 Le Coz Florent <louiz@louiz.org> - 4.3-1
- Fix a segmentation fault that occured when trying to connect
  to an IRC server without any port configured.

* Wed Apr 26 2017 Le Coz Florent <louiz@louiz.org> - 4.2-1
- Fix a build issue when LiteSQL is absent from the system

* Tue Mar 21 2017 Le Coz Florent <louiz@louiz.org> - 4.1-1
- Update to 4.1 sources: compatibility with botan 2.0

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
