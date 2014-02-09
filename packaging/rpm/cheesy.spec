Summary: A simple desktop casting tool 
%define version 1.0
License: GPLv3
Group: Productivity/Multimedia
Name: cheesy
Prefix: /usr 
%if %{defined fedora_version}
BuildRequires: gcc-c++ gtk2-devel pulseaudio-libs-devel gstreamer-devel boost-devel gstreamer-plugins-base-devel libX11-devel libXv-devel
Requires: libgtk-2_0-0 pulseaudio-libs gstreamer gstreamer-plugins-ffmpeg gstreamer-plugins-ugly gstreamer-plugins-base gstreamer-plugins-good gstreamer-plugins-base libXv libX11
%else
BuildRequires: gcc-c++ gtk2-devel libpulse-devel gstreamer-0_10-devel boost-devel gstreamer-0_10-plugins-base-devel libX11-devel libXv-devel
Requires: libgtk-2_0-0 libpulse0 libgstreamer-0_10-0 gstreamer-0_10-plugins-ffmpeg gstreamer-0_10-plugins-ugly  gstreamer-0_10-plugins-bad gstreamer-0_10-plugins-good gstreamer-0_10-plugins-base libXv1 libX11-6
%endif
Release: 1 
Source: cheesy-%{version}.tar.bz2
URL: http://github.com/kallaballa/Cheesy
Version: %{version} 
Buildroot: /tmp/cheesyrpm 
%description 
Cheesy is a simple desktop that lets you stream your local desktop to remote machines.

%prep 
%setup -q

%build 
make

%install 
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%clean 
rm -rf $RPM_BUILD_ROOT

%files 
%defattr(-,root,root) 
/usr/bin/cheesy
/etc/cheesy
/etc/cheesy/codecs
%doc %{_mandir}/man?/*

