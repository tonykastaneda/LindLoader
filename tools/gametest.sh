#!/bin/sh
# Boots a game fullscreen, drives it with virtual pad 1 into a match, then screenshots the
# screen and texts it.
# Usage: gametest.sh <name> <game dir> <width> <height> <boot seconds> "<caption>"
# Test settings live in logs\T\cfg\<name>\ (created on first use, separate from LindLoader's
# config\). Their controls add RS on pad 1 as a coin button. The game reads the pads while
# unfocused, so nothing depends on window focus. Needs vpadd.py running (started if not).
D=/c/Users/tonyk/Documents/GitHub/Lindy
T=$D/tools
C=$T/logs/T/cfg/$1
mkdir -p "$C" $T/shots
if [ ! -f "$C/controls.ini" ]; then
    $D/linuxloader.exe --create config "$(cygpath -w "$C")" linuxloader.ini >/dev/null
    $D/linuxloader.exe --create controls "$(cygpath -w "$C")" controls.ini >/dev/null
    sed -i 's/^P1_Coin = .*/&, GC0_BUTTON_RIGHTSTICK/' "$C/controls.ini"
fi
sed -i "s/^WIDTH = .*/WIDTH = $3/; s/^HEIGHT = .*/HEIGHT = $4/; s/^FULLSCREEN = .*/FULLSCREEN = true/" "$C/linuxloader.ini"

tasklist | grep -qi python || { (cd $T && python vpadd.py >/dev/null 2>&1 &); sleep 3; }

SHOT=$(cygpath -w "$T/shots/$1-$3x$4.png")
rm -f "$T/shots/$1-$3x$4.png"
printf '@echo off\r\ncd /d %s\r\nset LL_LOGLEVEL=info\r\nset SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1\r\nlinuxloader.exe -g "%s" -c "%s" -o "%s" > "%s" 2>&1\r\n' \
    "$(cygpath -w $D)" "$2" "$(cygpath -w "$C/linuxloader.ini")" "$(cygpath -w "$C/controls.ini")" \
    "$(cygpath -w $T/logs/T/run.log)" > $T/logs/T/run.cmd
printf 'CreateObject("WScript.Shell").Run "cmd /c %s", 0, False\r\n' "$(cygpath -w $T/logs/T/run.cmd)" > $T/logs/T/run.vbs
printf 'CreateObject("WScript.Shell").Run "powershell -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File %s -Out %s -Send ""%s""", 0, True\r\n' \
    "$(cygpath -w $T/snap.ps1)" "$SHOT" "$6" > $T/logs/T/snap.vbs
for t in run snap; do
    schtasks //create //tn LL_Test_$t //tr "wscript.exe //B $(cygpath -w $T/logs/T/$t.vbs)" //sc once //st 00:00 //it //f >/dev/null 2>&1
done

taskkill //IM linuxloader.exe //F >/dev/null 2>&1
schtasks //run //tn LL_Test_run >/dev/null
sleep "$5"
# Coin, start, then A through the menus to the first serve / round start.
sh $T/pad.sh "1 tap RS" "sleep 1.5" "1 tap RS" "sleep 1.5" "1 tap START" "sleep 3" \
    "1 tap A" "sleep 1.5" "1 tap A" "sleep 1.5" "1 tap A" "sleep 1.5" "1 tap A" "sleep 1.5" \
    "1 tap A" "sleep 1.5" "1 tap A" "sleep 1.5" "1 tap A" "sleep 1.5" "1 tap A" "sleep 1.5" \
    "1 tap A" "sleep 1.5" "1 tap A" "sleep 1.5" "1 tap A" "sleep 1.5" "1 tap A" "sleep 10"
schtasks //run //tn LL_Test_snap >/dev/null
until [ -f "$T/shots/$1-$3x$4.png" ]; do sleep 1; done
sleep 2
echo "$T/shots/$1-$3x$4.png"
