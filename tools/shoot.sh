#!/bin/sh
# shoot.sh <prefix> <count> <gap>: desktop screenshots via LL_Snap into VT3\shots
cd "$(dirname "$0")"; mkdir -p shots
for k in $(seq 1 $2); do rm -f latest.png; schtasks //Run //TN LL_Snap >/dev/null; for i in $(seq 1 30); do [ -f latest.png ] && break; sleep 0.5; done; sleep 1; cp latest.png shots/$1_$k.png; sleep $3; done
