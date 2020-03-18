Name:	openssl-gost-engine
Version:	1.1.0
Release:	1%{?dist}
Summary: Loadable module for openssl implementing GOST cryptoalgoritms	

Group:	Libraries/Cryptography	
License: OpenSSL	
URL: https://github.com/gost-engine/engine		
Source0: %{name}-%{version}.tar.bz2	

%if 0%{?rhel} == 7
%define cmake cmake3
%else
%define cmake cmake
%endif

BuildRequires: %{cmake}

Requires: openssl-libs	


%description

This package contains openssl module with software implementation of GOST cryptoalgorithms.

%package -n gostsum
Summary: utilities to compute GOST hashes
Group: Utilities/Cryptography
License: OpenSSL

%description -n gostsum
Gostsum and gost12sum are utilities, similar to md5sum or sha1sum which computes


%prep
%setup -q


%build
%{cmake} cmake3 -DOPENSSL_ROOT_DIR=/usr/local -DOPENSSL_LIBRARIES=/usr/lib64 .
make %{?_smp_mflags}

%install
install -d -m 755 %{buildroot}%{_libdir}/engines-1.1
install -c -m 755 bin/gost.so %{buildroot}%{_libdir}/engines-1.1
install -d -m 755 %{buildroot}%{_bindir}
install -d -m 755 %{buildroot}%{_mandir}/man1
install -c -m 755 bin/gostsum %{buildroot}%{_bindir}
install -c -m 755 bin/gost12sum %{buildroot}%{_bindir}
install -c -m 644 gostsum.1 %{buildroot}%{_mandir}/man1
install -c -m 644 gost12sum.1 %{buildroot}%{_mandir}/man1
mkdir -p %{buildroot}%{_sysconfdir}/openssl-micro
cat <<EOF >> %{buildroot}%{_sysconfdir}/openssl-micro/openssl.cnf
openssl_conf = openssl_def

[openssl_def]
engines = engine_section

[engine_section]
gost = gost_section

[gost_section]
default_algorithms = ALL
engine_id = gost
CRYPT_PARAMS = id-Gost28147-89-CryptoPro-A-ParamSet
EOF


%files
%config %{_sysconfdir}/openssl-micro/openssl.cnf
%doc README.gost
%doc README.md
%dir %{_libdir}/engines-1.1
%{_libdir}/engines-1.1/gost.so

%files -n gostsum
%{_bindir}/gostsum
%{_bindir}/gost12sum
%{_mandir}/man1/gostsum.1*
%{_mandir}/man1/gost12sum.1*

%changelog

* Wed Aug  2 2017 Victor Wagner <vitus@wagner.pp.ru>
- initial release

