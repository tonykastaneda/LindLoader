#!/bin/sh
# Build the LindLoader fork (MSYS2 MinGW32) and install the exe into the loader\ folder of
# every game in games\ (each game folder is self-contained).
# Usage: tools/build.sh
set -e
L=/c/Users/tonyk/Documents/GitHub/LindLoader
B=$L/build-win32
mkdir -p $B
[ -f $B/Makefile ] || MSYSTEM=MINGW32 C:/msys64/usr/bin/bash.exe -lc "cd $B && cmake .. -G 'MinGW Makefiles'" > $B/cmake.log 2>&1
MSYSTEM=MINGW32 C:/msys64/usr/bin/bash.exe -lc "cd $B && cmake --build . -j16" > $B/build.log 2>&1 || { grep -m5 error $B/build.log; exit 1; }
taskkill //IM linuxloader.exe //F >/dev/null 2>&1 || true
sleep 1
for g in $L/games/*/; do
    cp $B/linuxloader.exe "$g/loader/"
    echo "installed into $(basename "$g")"
done

# <game>.exe launcher for each game (see src/launcher/play.c).
play() { # <game folder> <exe name> <game dir inside the folder>
    MSYSTEM=MINGW32 C:/msys64/usr/bin/bash.exe -lc "gcc -O2 -s -mwindows '-DGAME_DIR=\"$3\"' -o '$L/games/$1/$2' '$L/src/launcher/play.c'"
}
play VT3 "vt3.exe" 'game\\\\disk0\\\\vt3_Lindbergh'
play RACETV "racetv.exe" 'game\\\\disk0'
play VF5FS "vf5fs.exe" 'game\\\\disk0'
