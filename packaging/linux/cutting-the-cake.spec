Name:           cutting-the-cake
Version:        %{version}
Release:        1%{?dist}
Summary:        Cutting the Cake, a screensaver from "Anathem"
License:        MIT
Source0:        cutting-the-cake-%{version}.tar.gz
BuildRequires:  gcc, make, pkgconfig(sdl2)
Requires:       swayidle

%description
Cutting the Cake is the first calca in Neal Stephenson's novel Anathem: a boy is
asked to cut a square cake into eight equal servings, and finds that making the
square twice as long each way gives sixteen, not eight.

This screensaver plays the lesson through with Erasmas, Dath and Arsibalt:
two each way, four each way, three each way, and then the cut from corner to
corner that gives exactly eight.

On Wayland desktops it starts by itself after five minutes idle.
cutting-the-cake-screensaver changes the delay or turns it off.

%prep
%setup -q

%build
make %{?_smp_mflags} CFLAGS="%{optflags}" LDFLAGS="%{?__global_ldflags}"

%install
make install DESTDIR=%{buildroot} PREFIX=%{_prefix} SYSCONFDIR=%{_sysconfdir}
rm -rf %{buildroot}%{_datadir}/doc/cutting-the-cake

%files
%license LICENSE
%doc README.md
%{_bindir}/cutting-the-cake
%{_bindir}/cutting-the-cake-screensaver
%{_datadir}/applications/cutting-the-cake.desktop
%{_datadir}/icons/hicolor/256x256/apps/cutting-the-cake.png
%config(noreplace) %{_sysconfdir}/xdg/autostart/cutting-the-cake-screensaver.desktop
