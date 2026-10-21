# tools

Build and test helpers for this fork on Windows. None of this is needed to play a game.

## Layout these tools expect

```
LindLoader\                  the LindLoader repo
  loader\                    the linuxloader fork (git submodule)
  tools\                     this folder
  build\                     build output (gitignored)
  games\<GAME>\              one self-contained folder per game (gitignored)
    <game>.vbs               launcher (vt3.vbs, racetv.vbs, vf5fs.vbs)
    loader\                  linuxloader.exe + ll-deps
    game\                    the game files (clean dumps only)
    linuxloader.ini          that game's settings
    controls.ini             that game's controls (controller GUIDs are saved here too)
```

Games set up so far: `VT3` (Virtua Tennis 3 Rev C), `RACETV` (Sega Race TV), `VF5FS` (Virtua Fighter 5 Final Showdown Rev B).

## Building

`build.sh` builds the loader from `loader\` with MSYS2 MinGW32 into `build\loader\`. It then copies `linuxloader.exe` into every `games\*\loader\`. Run it from Git Bash.

## Running a test

`run-logged.vbs <VT3|RACETV|VF5FS>` starts a game like its launcher does, with three additions:

- Logging is turned on (`LL_LOGLEVEL=info`).
- Game output goes to `logs\<GAME>\run.log` and the loader's log to `logs\<GAME>\loader.log`.
- The pads are read even when the window isn't focused.

Extra environment variables can go in `testenv.txt`, one `NAME=VALUE` per line.

It is run through scheduled tasks created with `/IT`, so the game opens in the logged-in user's session. The task names are `LL_Run_VT3`, `LL_Run_RTV` and `LL_Run_VF5`. A shell running as a service can't show windows on the desktop.

## Virtual pads

These use ViGEmBus and the Python `vgamepad` package.

- **`vpadd.py`** is a daemon that keeps two virtual Xbox 360 pads plugged in. It runs the commands appended to `cmd.txt`.
- **`pad.sh "<pad> tap|press|release BUTTON" "<pad> stick x y" "<pad> trigger l|r amount" "sleep s"`** queues commands and waits until they've run.
- **`vpads.py`** is the one-shot version.

Start the daemon before the game: the loader assigns controllers when it starts. SDL numbers controllers in the order it finds them, so a real pad usually comes first.

## Game tests

**`gametest.sh <name> <game dir> <width> <height> <boot seconds> "<caption>"`** boots a game fullscreen at that resolution and coins in with virtual pad 1. It then presses Start and A through the menus into a match, and screenshots and texts the screen. Its settings live in `logs\T\cfg\<name>\`, apart from LindLoader's `config\`, and its controls add RS as a coin button. The game reads the pads while unfocused, so the test doesn't depend on window focus. It starts `vpadd.py` if it isn't running. A game's first boot sets up its save data for a minute or two and ignores input, so give that one a longer boot time.

A fullscreen game can show an older frame in desktop screenshots: Windows only refreshes them when something else on screen changes.

## LindLoader UI tests

`uitest\` drives LindLoader with Windows UI Automation, so no window needs keyboard focus, and uses the virtual pads (`vpadd.py` must be running). Run each script through a scheduled task in the user's session, like the others. Results go to a `.log` next to `$log`, and the bindings land in `config\<game>\controls.ini`. Delete that folder afterwards so the game starts from default controls.

- `layout.ps1 -Title <game> -Name <name>`: opens a game's Controls page with the Controllers button and captures the window to `shots\layout-<name>.bmp`.
- `bind.ps1`: VF5, binds player 1's Button 1 with pad X and Button 2 with the K key.
- `analog.ps1`: Race TV, binds steering, Steer Left and gas with the stick and trigger, and checks that an analog pedal ignores a button.
- `p2.ps1`: VF5, binds player 2's Button 1 with pad 2.

## Screenshots

- **`snap.ps1 -Out <png>`** captures the whole desktop. It writes `<png>` scaled to 1280 wide and `<name>.full.png` at full size. With `-Send "caption"` it also texts the image through the Mac's `imsg` CLI to the number in the `LL_IMSG_TO` environment variable.
- **`LL_Snap`** is the scheduled task that runs it in the user's session. It briefly flashes a PowerShell window, which also shows up in the screenshots. `snap.vbs` would run it with no window, but the task doesn't use it yet.
- **`shoot.sh <prefix> <count> <gap>`** takes a series of screenshots into `shots\`.
- **`wincap.c`** captures a single window with PrintWindow. It comes out black for some games, such as VF5.

## Other helpers

- `winlist.c`: lists the top-level windows with their titles. The window title shows the FPS.
- `fg.c`: tries to bring a window to the front.
- `keyseq.c`, `keyseq_rtv.c`: replay timed key presses from files like `race.txt`, `match.txt` and `focus.txt` (`<key> <hold ms> <pause ms>` per line).
- `tsample.c`: samples frame timing.
- `race_measure.sh`: Race TV pacing test. It boots the game, coins in, races, and prints the frame stats.
- `manifest.py`: prints the md5, size and path of every file in a folder, for comparing game dumps.
- `sub.py`: does an exact-text replace that keeps the file's line endings.

## Frame pacing

Set `FRAME_STATS = true` in a game's `linuxloader.ini`. `framestats.log` then appears in the game folder with 10-second summaries. At 60 fps a summary should show about 600 frames and 0 late frames.
