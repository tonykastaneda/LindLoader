# LindLoader

Play SEGA Lindbergh arcade games on Windows. Point LindLoader at a folder of your own game dumps. It recognizes the games and lets you set up graphics and controls, then plays them with one click.

LindLoader has two parts:

- **`linuxloader.exe`**, the emulator core: our version of [Linux Loader](https://github.com/lindbergh-loader/linuxloader), in `src/loader`.
- **`Lindy.exe`**, the Qt app you use to manage and launch games, in `src/ui`.

LindLoader does not include or download games, and it does not decrypt anything. Use clean dumps of games you own.

## What's different in our Linux Loader

- **Vsync and frame pacing:** an adaptive frame repeat, so games hold a steady 60 fps on high-refresh displays without tearing.
- **Virtua Tennis 3 Rev C:**
  - The original camera at any resolution.
  - A HUD that fits the screen.
  - The game's own 4:3 mode.
- **Virtua Fighter 5 Final Showdown Rev B:**
  - Runs on Windows.
  - Resolutions above 2048 wide (e.g. 4K) work.
  - The game's own 4:3 mode.
- **Sega Race TV:** runs on Windows, including its network setup.
- **Linked play on one PC** (`LOOPBACK_LINK`): several cabinets of Sega Race TV, one loader each, race each other over loopback. Lindy sets it up from **Game > Start Linked Play**.
- **One mouse per player** in lightgun games (`RAW_MOUSE_INPUT`, `MOUSE1_`/`MOUSE2_` bindings), with crosshairs you can pick.
- **Rebindable hotkeys:** Toggle Fullscreen and Toggle FPS Overlay, besides Test and Exit Game.
- **Audio output device and volume** (`AUDIO_DEVICE`, `AUDIO_VOLUME`).
- **Controller IDs** are saved to the controls file that was loaded.
- **`--identify <folder>`** lists the games in a folder as JSON, for frontends.
- **Lindy as a frontend's emulator:** `Lindy.exe "<game folder>"` starts a game with Lindy's settings without showing Lindy, and exits when it closes. `Lindy.exe --linked 4 "<game folder>"` starts linked play, and Exit Game in any cabinet closes them all.
- **Disk images (.bin)** are unpacked with **File > Unpack (.bin)**, never run directly. Lindy can then delete the image and move the game into a library folder.

## Layout

```
src/loader/       Linux Loader with our changes (the emulator core)
src/ui/           the LindLoader Qt app
tools/            build and test tools, see tools/README.md
linuxloader.exe   built core   ┐
ll-deps/          its DLLs     ├ build output (not in git)
Lindy.exe         built app    ┘
```

## Building (Windows)

You need [MSYS2](https://www.msys2.org/) with:

- **For the core:** the MinGW32 toolchain. Linux Loader is a 32-bit program.
- **For the app:** the UCRT64 toolchain with `mingw-w64-ucrt-x86_64-qt6-static`. That makes `Lindy.exe` a single file with no Qt DLLs.

```
tools/build.sh
```

## Credits

LindLoader is built on **[Linux Loader](https://github.com/lindbergh-loader/linuxloader)** by the **Linux Loader Development Team**. Their loader runs Lindbergh games outside the original hardware, and almost all of the emulation in LindLoader is their work. Thank you to everyone who contributed to it.

Linux Loader is licensed under [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International](https://creativecommons.org/licenses/by-nc-sa/4.0/). LindLoader, including our changes to the loader and the app, is shared under the same license (see [LICENSE](LICENSE)). It is free and non-commercial.
