Name:     biboumi
Version:  1.0
Release:  1%{?dist}
Summary:  Lightweight XMPP to IRC gateway

License:  zlib
URL:      http://biboumi.louiz.org
Source0:  http://biboumi.louiz.org/biboumi-1.0.tar.xz

BuildRequires: libidn-devel
BuildRequires: expat-devel
BuildRequires: libuuid-devel
BuildRequires: systemd-devel
BuildRequires: cmake
BuildRequires: systemd
BuildRequires: rubygem-ronn

%global biboumi_confdir %{_sysconfdir}/%{name}


%description
An XMPP gateway that connects to IRC servers and translates between the two
protocols. It can be used to access IRC channels using any XMPP client as if
these channels were XMPP MUCs.


%prep
%setup -q


%build
cmake . -DCMAKE_BUILD_TYPE=release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DPOLLER=EPOLL
# The documentation is in utf-8, ronn fails to build it if that locale is
# not specified
make %{?_smp_mflags}
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
* Wed Jun 25 2014 Le Coz Florent <louiz@louiz.org> - 1.0-1
- Spec file written from scratch
