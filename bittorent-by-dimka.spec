Name:           bittorent-by-dimka
Version:        0.0.1
Release:        0
Summary:        Bittorent your files
License:        GPL-2.0
URL:            https://dlisin.dev
BuildRequires:  libcurl-devel, cmake, ninja
Requires:       libcurl

%description
Dimka's Bittorent client

%prep

%build
make

%install
install -D %{buildroot}/bin/bittorent %{bindir}/bittorent

%files
%{bindir}/bittorent

%debug_package

%changelog
