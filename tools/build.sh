#!/bin/sh
# Build the loader (loader/ submodule, MSYS2 MinGW32) and install it into the loader\ folder of
# every game in games\ (each game folder is self-contained).
# Usage: tools/build.sh
set -e
L=/c/Users/tonyk/Documents/GitHub/LindLoader
B=$L/build/loader
mkdir -p $B
[ -f $B/Makefile ] || MSYSTEM=MINGW32 C:/msys64/usr/bin/bash.exe -lc "cd $B && cmake $L/loader -G 'MinGW Makefiles'" > $B/cmake.log 2>&1
MSYSTEM=MINGW32 C:/msys64/usr/bin/bash.exe -lc "cd $B && cmake --build . -j16" > $B/build.log 2>&1 || { grep -m5 error $B/build.log; exit 1; }
taskkill //IM linuxloader.exe //F >/dev/null 2>&1 || true
sleep 1
for g in $L/games/*/; do
    cp $B/linuxloader.exe "$g/loader/"
    echo "installed into $(basename "$g")"
done

