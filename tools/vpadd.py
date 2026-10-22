"""Virtual pad daemon: keeps two virtual Xbox 360 pads (VPADS=n for more) plugged in and runs
commands appended to cmd.txt (same syntax as vpads.py, without the delay):
    <pad 1..n> tap <BUTTON> [hold_seconds]
    <pad 1..n> press <BUTTON> | release <BUTTON>
    <pad 1..n> stick <x> <y>
    sleep <seconds>
"""
import os
import time

import vgamepad as vg

from vpads import NAMES

HERE = os.path.dirname(os.path.abspath(__file__))
CMD = os.path.join(HERE, "cmd.txt")
LOG = os.path.join(HERE, "vpadd.log")


def log(msg):
    with open(LOG, "a") as f:
        f.write(f"{time.strftime('%H:%M:%S')} {msg}\n")


def run(pads, parts):
    if parts[0] == "sleep":
        time.sleep(float(parts[1]))
        return
    pad, action = pads[int(parts[0]) - 1], parts[1]
    if action == "tap":
        hold = float(parts[3]) if len(parts) > 3 else 0.12
        pad.press_button(NAMES[parts[2]])
        pad.update()
        time.sleep(hold)
        pad.release_button(NAMES[parts[2]])
    elif action == "press":
        pad.press_button(NAMES[parts[2]])
    elif action == "release":
        pad.release_button(NAMES[parts[2]])
    elif action == "stick":
        pad.left_joystick_float(float(parts[2]), float(parts[3]))
    elif action == "trigger":
        if parts[2] == "r":
            pad.right_trigger_float(float(parts[3]))
        else:
            pad.left_trigger_float(float(parts[3]))
    pad.update()


def main():
    pads = [vg.VX360Gamepad() for _ in range(int(os.environ.get("VPADS", "2")))]
    open(CMD, "w").close()
    open(LOG, "w").close()
    done = 0
    log("daemon started")
    while True:
        try:
            with open(CMD) as f:
                lines = [l.split() for l in f.read().splitlines() if l.strip()]
        except PermissionError:  # another process is appending to cmd.txt
            time.sleep(0.05)
            continue
        for parts in lines[done:]:
            try:
                run(pads, parts)
                log(" ".join(parts))
            except Exception as e:  # keep the pads alive on a bad line
                log(f"error on {' '.join(parts)}: {e}")
            time.sleep(0.15)
        done = len(lines)
        time.sleep(0.05)


if __name__ == "__main__":
    main()
