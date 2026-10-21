/* Test helper: play a key script into the foreground window with SendInput.
   Script lines: "<vk hex> <hold ms> <pause ms>" or "wait <ms>". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

/* Keep the game window in front: input goes to the foreground window. */
static void focus_game(void)
{
    HWND w = FindWindowA(NULL, NULL);
    for (w = GetTopWindow(NULL); w; w = GetNextWindow(w, GW_HWNDNEXT)) {
        char t[256];
        if (IsWindowVisible(w) && GetWindowTextA(w, t, sizeof(t)) && strstr(t, "Race TV")) {
            if (GetForegroundWindow() != w) {
                DWORD fgThread = GetWindowThreadProcessId(GetForegroundWindow(), NULL), me = GetCurrentThreadId();
                AttachThreadInput(fgThread, me, TRUE);
                keybd_event(VK_MENU, 0, 0, 0);
                SetForegroundWindow(w);
                BringWindowToTop(w);
                keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
            AttachThreadInput(fgThread, me, FALSE);
            }
            return;
        }
    }
}

static void key(WORD vk, DWORD flags)
{
    INPUT in = { 0 };
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = (WORD)MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    in.ki.dwFlags = KEYEVENTF_SCANCODE | flags;
    if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN)
        in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    SendInput(1, &in, sizeof(in));
}

int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR c, int d)
{
    char line[128];
    FILE *f;
    (void)a; (void)b; (void)d;
    f = fopen(c && *c ? c : "keys.txt", "r");
    if (!f)
        return 1;
    while (fgets(line, sizeof(line), f)) {
        unsigned vk, hold, pause;
        if (sscanf(line, "wait %u", &pause) == 1) {
            Sleep(pause);
        } else if (sscanf(line, "%x %u %u", &vk, &hold, &pause) == 3) {
            focus_game();
            key((WORD)vk, 0);
            Sleep(hold);
            key((WORD)vk, KEYEVENTF_KEYUP);
            Sleep(pause);
        }
    }
    return 0;
}
