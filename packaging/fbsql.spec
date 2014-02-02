Summary: A command line client for Firebird, similar to psql
Name: fbsql
Version: 0.1.3
Release: 1
Source: fbsql-0.1.3.tar.gz
URL: https://github.com/ibarwick/fbsql
License: PostgreSQL
Group: Productivity/Databases/Tools
Packager: Ian Barwick
BuildRequires: firebird-devel
BuildRequires: libfq
BuildRequires: libreadline6
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
fbsql is a simple command-line client for the Firebird database,
inspired by psql from PostgreSQL

%prep
%setup
./configure --prefix=%{_prefix} \
  --with-ibase=/usr/include/firebird \
  --with-readline=/usr/include/readline
%build

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
* Sun Feb 2 2014 Ian Barwick (barwick@gmail.com)
- First draft
