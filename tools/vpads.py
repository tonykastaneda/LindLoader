"""Drive two virtual Xbox 360 pads through ViGEmBus from a script.

Usage: python vpads.py <script.txt>
Script lines: "<seconds> <pad 1|2> <action>" where action is one of
  press <BUTTON> | release <BUTTON> | tap <BUTTON> | stick <x> <y> | wait
BUTTON names: A B X Y LB RB BACK START LS RS UP DOWN LEFT RIGHT
Lines run in order; <seconds> is a delay before the line runs.
"""
import sys
import time

import vgamepad as vg

B = vg.XUSB_BUTTON
NAMES = {
    "A": B.XUSB_GAMEPAD_A, "B": B.XUSB_GAMEPAD_B, "X": B.XUSB_GAMEPAD_X,
    "Y": B.XUSB_GAMEPAD_Y, "LB": B.XUSB_GAMEPAD_LEFT_SHOULDER,
    "RB": B.XUSB_GAMEPAD_RIGHT_SHOULDER, "BACK": B.XUSB_GAMEPAD_BACK,
    "START": B.XUSB_GAMEPAD_START, "LS": B.XUSB_GAMEPAD_LEFT_THUMB,
    "RS": B.XUSB_GAMEPAD_RIGHT_THUMB, "UP": B.XUSB_GAMEPAD_DPAD_UP,
    "DOWN": B.XUSB_GAMEPAD_DPAD_DOWN, "LEFT": B.XUSB_GAMEPAD_DPAD_LEFT,
    "RIGHT": B.XUSB_GAMEPAD_DPAD_RIGHT,
}


def main(path):
    pads = [vg.VX360Gamepad(), vg.VX360Gamepad()]
    time.sleep(1.0)
    with open(path) as f:
        lines = [l.split() for l in f if l.strip() and not l.startswith("#")]
    for parts in lines:
        delay, pad, action = float(parts[0]), pads[int(parts[1]) - 1], parts[2]
        time.sleep(delay)
        if action == "press":
            pad.press_button(NAMES[parts[3]])
        elif action == "release":
            pad.release_button(NAMES[parts[3]])
        elif action == "tap":
            pad.press_button(NAMES[parts[3]])
            pad.update()
            time.sleep(0.12)
            pad.release_button(NAMES[parts[3]])
        elif action == "stick":
            pad.left_joystick_float(float(parts[3]), float(parts[4]))
        pad.update()
        print(f"{time.strftime('%H:%M:%S')} pad {parts[1]} {' '.join(parts[2:])}", flush=True)


if __name__ == "__main__":
    main(sys.argv[1])
