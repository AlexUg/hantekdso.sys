# hantekdso.sys
Wine driver for Hantek DSO 6xxxYY USB Oscilloscopes.
Required Wine v.4.0.0 or above.

Tested on KDE neon User Edition 5.14 (18.04, kernel: 4.15.0-45-generic platform: x86-64).

# Build
Requires: wine-4.x.y, libusb-1.0. Both with development source files.
For build run:
```
make
```

# Installation
Overal installation process requires next steps:
1. copy fake dll 'hantekdso.sys.fake' to 'drive_c/windows/syswow64/drivers/hantekdso.sys' in wine prefix directory;
2. copy shared lib 'hantekdso.sys.so' to 'lib/wine/' subdir in wine installation directory;
3. import registry from 'resources/hantekdso.reg' using 'regedit.exe';

First two steps can be done with 'make'.

There must be configured settings in Makefile:
```
WINEPREFIX		= .wine_hantek
USER					= alexandr
WINE_LIBS_PATH = /opt/wine-stable/lib/wine/
```
WINEPREFIX - 'wine prefix name' in which Hantek software was installed;
USER - 'user name';
WINE_LIBS_PATH - path to wine libs

For installing run:
```
make install
```

