Name:     biboumi
Version:  1.1
Release:  2%{?dist}
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
BuildRequires: rubygem-ronn

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

# The documentation is in utf-8, ronn fails to build it if that locale is
# not specified
LC_ALL=en_GB.utf-8 make doc


%install
make install DESTDIR=%{buildroot}

# Default config file
install -D -p -m 644 conf/biboumi.cfg \
    %{buildroot}%{biboumi_confdir}/biboumi.cfg

# Systemd unit file
install -D -p -m 644 unit/%{name}.service \
    %{buildroot}%{_unitdir}/%{name}.service


%check
make test_suite/fast VERBOSE=1

./test_suite || exit 1


%files
%{_bindir}/%{name}
%{_mandir}/man1/%{name}.1*
%doc README COPYING doc/biboumi.1.md
%{_unitdir}/%{name}.service
%config(noreplace) %{biboumi_confdir}/biboumi.cfg


%changelog
* Wed Nov 13 2014 Le Coz Florent <louiz@louiz.org> - 1.1-2
- Use the -DWITH(OUT) cmake flags for all optional dependencies
- Build with the correct optflags
- Use hardened_build

* Wed Aug 18 2014 Le Coz Florent <louiz@louiz.org> - 1.1-1
- Update to 1.1 release

* Wed Jun 25 2014 Le Coz Florent <louiz@louiz.org> - 1.0-1
- Spec file written from scratch
