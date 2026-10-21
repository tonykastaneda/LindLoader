# LindLoader

Play SEGA Lindbergh arcade games on Windows. Point LindLoader at a folder of your own game dumps. It recognizes the games and sets up their controls and graphics, then launches them. You can use it from a GUI, or from the command line, so frontends like LaunchBox, Playnite and ES-DE can launch the games.

LindLoader runs games with [linuxloader](https://github.com/lindbergh-loader/linuxloader). It ships with its own fork of linuxloader ([tonykastaneda/linuxloader](https://github.com/tonykastaneda/linuxloader)), included as the `loader/` submodule. Fixes in the fork:

- **Vsync and frame pacing:** an adaptive frame repeat, so games hold a steady 60 fps on high-refresh displays.
- **Virtua Tennis 3 Rev C:**
  - The original camera at any resolution.
  - A HUD that fits the screen.
  - The game's own 4:3 mode.
- **Virtua Fighter 5 Final Showdown Rev B:**
  - Runs on Windows.
  - Resolutions above 2048 wide (e.g. 4K) work.
  - The game's own 4:3 mode.
- **Sega Race TV:** runs on Windows, including its network setup.
- **Controller IDs** are saved to the controls file that was loaded.

LindLoader does not include or download games, and it does not decrypt anything. Use clean dumps of games you own.

## Status

Early. Working now:

- The loader and the self-contained per-game folder layout.
- The per-game launcher.
- Build and test tools.

Next steps:

- [ ] `core`: scan a folder and recognize games; read and write `linuxloader.ini` and `controls.ini`; build the launch command.
- [ ] `lindloader-cli`: `scan`, `run`, `set`, `bind`, for frontends and scripts.
- [ ] `lindloader` GUI (SDL3 + Dear ImGui): works with a mouse on a desktop or a controller on a TV. Settings and press-to-bind controls.

## Layout

```
loader/      linuxloader fork (git submodule)
launcher/    per-game launcher (vt3.exe, racetv.exe, vf5fs.exe)
tools/       build and test tools, see tools/README.md
build/       build output (ignored)
games/       local self-contained game folders (ignored)
```

Each game folder is self-contained and can be moved anywhere:

```
games/VF5FS/
  vf5fs.exe          launcher
  loader/            linuxloader.exe + ll-deps
  game/              the game files
  linuxloader.ini    settings
  controls.ini       controls
```

## Building (Windows)

Requires MSYS2 with the MinGW32 toolchain.

```
git clone --recursive https://github.com/tonykastaneda/LindLoader.git
tools/build.sh
```
