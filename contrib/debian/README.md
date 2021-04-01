
Debian
====================
This directory contains files used to package cspnd/cspn-qt
for Debian-based Linux systems. If you compile cspnd/cspn-qt yourself, there are some useful files here.

## cspn: URI support ##


cspn-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install cspn-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your cspn-qt binary to `/usr/bin`
and the `../../share/pixmaps/cspn128.png` to `/usr/share/pixmaps`

cspn-qt.protocol (KDE)

