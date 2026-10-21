"""Virtual pad daemon: keeps two virtual Xbox 360 pads plugged in and runs
commands appended to cmd.txt (same syntax as vpads.py, without the delay):
    <pad 1|2> tap <BUTTON> [hold_seconds]
    <pad 1|2> press <BUTTON> | release <BUTTON>
    <pad 1|2> stick <x> <y>
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
    pads = [vg.VX360Gamepad(), vg.VX360Gamepad()]
    open(CMD, "w").close()
    open(LOG, "w").close()
    done = 0
    log("daemon started")
    while True:
        with open(CMD) as f:
            lines = [l.split() for l in f.read().splitlines() if l.strip()]
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
