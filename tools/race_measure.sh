#!/bin/sh
# Race TV pacing test: boot, coin + start with virtual pad 1, race ~2 min, print frame stats.
L=/c/Users/tonyk/Documents/GitHub/Lindy; P=$L/tools
taskkill //IM linuxloader.exe //F >/dev/null 2>&1; sleep 2
find $L/games/RACETV -name framestats.log -delete; rm -f $L/tools/logs/RACETV/*
schtasks //run //tn LL_Run_RTV >/dev/null; sleep 60
sh $P/pad.sh "1 tap BACK" "sleep 1" "1 tap START" "sleep 3"
for i in $(seq 8); do echo "1 tap START" >> $P/cmd.txt; echo "sleep 1.8" >> $P/cmd.txt; done; sleep 16
echo "1 trigger r 1.0" >> $P/cmd.txt
for i in $(seq 30); do echo "1 stick 0.35 0" >> $P/cmd.txt; echo "sleep 1.5" >> $P/cmd.txt; echo "1 stick -0.35 0" >> $P/cmd.txt; echo "sleep 1.5" >> $P/cmd.txt; done
sleep ${1:-100}
F=$(find $L/games/RACETV -name framestats.log | head -1); grep VSync "$F"; grep Frames "$F"
grep -n 'CRASH\|FATAL' $L/tools/logs/RACETV/run.log
