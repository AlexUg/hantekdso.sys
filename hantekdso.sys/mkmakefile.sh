#!/bin/bash
winemaker --nosource-fix --dll --nomfc -I/opt/wine/include/wine/ -l:ntoskrnl.exe.so -lwine -lusb-1.0 -lpthread .