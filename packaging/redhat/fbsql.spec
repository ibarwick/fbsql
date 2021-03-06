Summary: A command line client for Firebird, similar to psql
Name: fbsql
Version: 0.2.0
Release: 1
Source: fbsql-%{version}.tar.gz
URL: https://github.com/ibarwick/fbsql
License: PostgreSQL
Group: Productivity/Databases/Tools
Packager: Ian Barwick
BuildRequires: firebird-devel
BuildRequires: libfq
BuildRequires: readline-devel
BuildRequires: flex
BuildRoot: %{_tmppath}/%{name}-%{version}-build
Requires: libfq

%description
fbsql is a simple command-line client for the Firebird database,
inspired by psql from PostgreSQL

%prep
%setup
%configure
%build
./configure --prefix=%{_prefix} \
  --with-ibase=/usr/include/firebird \
  --with-readline=/usr/include/readline
make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
/usr/bin/fbsql

%changelog
* Sat Apr 21 2018 Ian Barwick (barwick@gmail.com)
- fbsql 0.2.0
* Tue Feb 11 2014 Ian Barwick (barwick@gmail.com)
- fbsql 0.1.4
* Sun Feb 2 2014 Ian Barwick (barwick@gmail.com)
- First draft
