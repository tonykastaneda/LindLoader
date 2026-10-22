#!/bin/sh
# Build the loader (src/loader, MSYS2 MinGW32) and the app (src/ui) and put linuxloader.exe and
# Lindy.exe in the project root. Also updates the loader\ copy in every game folder under
# games\, if there is one.
# Usage: tools/build.sh
set -e
L=/c/Users/tonyk/Documents/GitHub/Lindy
B=$L/build/loader
mkdir -p $B
[ -f $B/Makefile ] || MSYSTEM=MINGW32 C:/msys64/usr/bin/bash.exe -lc "cd $B && cmake $L/src/loader -G 'MinGW Makefiles'" > $B/cmake.log 2>&1
MSYSTEM=MINGW32 C:/msys64/usr/bin/bash.exe -lc "cd $B && cmake --build . -j16" > $B/build.log 2>&1 || { grep -m5 error $B/build.log; exit 1; }
taskkill //IM linuxloader.exe //F >/dev/null 2>&1 || true
sleep 1
cp $B/linuxloader.exe $L/
echo "installed into the project root"
for g in $L/games/*/; do
    [ -d "$g/loader" ] || continue
    cp $B/linuxloader.exe "$g/loader/"
    echo "installed into $(basename "$g")"
done

# Lindy.exe, the Qt app (src/ui, MSYS2 UCRT64 with static Qt: a single exe).
MSYSTEM=UCRT64 C:/msys64/usr/bin/bash.exe -lc "cd $L && cmake -S src/ui -B build/ui -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/ucrt64/qt6-static > build/ui-cmake.log 2>&1 && cmake --build build/ui > build/ui-build.log 2>&1" || { grep -m5 -i error $L/build/ui-build.log; exit 1; }
taskkill //IM Lindy.exe //F >/dev/null 2>&1 || true
cp $L/build/ui/Lindy.exe $L/
echo "built Lindy.exe"
