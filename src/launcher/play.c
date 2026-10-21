/* Play launcher for a self-contained game folder:
 *
 *   <folder>\<game>.exe        (this program, e.g. vt3.exe)
 *   <folder>\loader\linuxloader.exe
 *   <folder>\<GAME_DIR>        (the game files, GAME_DIR set at build time)
 *   <folder>\linuxloader.ini
 *   <folder>\controls.ini
 *
 * Starts the loader with that folder's game and settings, without a console window.
 * Build: gcc -mwindows -DGAME_DIR="\"game\\\\disk0\"" -o <game>.exe play.c */
#include <windows.h>
#include <stdio.h>

#ifndef GAME_DIR
#define GAME_DIR "game\\disk0"
#endif

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR args, int show)
{
    char here[MAX_PATH], loaderDir[MAX_PATH], cmd[4 * MAX_PATH + 64], *slash;
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    (void)inst; (void)prev; (void)args; (void)show;

    GetModuleFileNameA(NULL, here, MAX_PATH);
    slash = strrchr(here, '\\');
    if (slash)
        slash[1] = 0;

    snprintf(loaderDir, sizeof(loaderDir), "%sloader", here);
    snprintf(cmd, sizeof(cmd), "\"%sloader\\linuxloader.exe\" -g \"%s" GAME_DIR "\" -c \"%slinuxloader.ini\" -o \"%scontrols.ini\"",
             here, here, here, here);

    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, loaderDir, &si, &pi))
    {
        char msg[MAX_PATH + 64];
        snprintf(msg, sizeof(msg), "Could not start %sloader\\linuxloader.exe", here);
        MessageBoxA(NULL, msg, "Play", MB_ICONERROR);
        return 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
