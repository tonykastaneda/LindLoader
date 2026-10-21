#!/bin/sh
# Send commands to the virtual pad daemon and wait for them to run.
# Usage: pad.sh "1 tap START" "sleep 2" "2 tap A" ...
DIR="$(dirname "$0")"
for c in "$@"; do
    echo "$c" >> "$DIR/cmd.txt"
done
n=$(wc -l < "$DIR/cmd.txt")
# Each command takes at least 0.15 s plus any sleeps; poll the log until caught up.
for i in $(seq 1 600); do
    done_n=$(grep -cvE 'daemon started|error' "$DIR/vpadd.log" 2>/dev/null); done_n=${done_n:-0}
    [ "$done_n" -ge "$n" ] && exit 0
    sleep 0.2
done
echo "pad.sh: timed out" >&2
exit 1
