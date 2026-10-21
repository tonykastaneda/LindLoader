/* Test helper: bring the first window whose title contains the argument to
   the foreground and give it keyboard focus (SDL ignores gamepads while its
   window is unfocused). Windows refuses focus to background programs, so it
   raises the window above everything and clicks it, which always works.
   Logs to fg.log next to the exe. Usage: fg <title text> */
#include <windows.h>
#include <string.h>
#include <stdio.h>

static void logf(const char *msg, const char *title, int fg)
{
    char path[MAX_PATH], *slash;
    FILE *f;
    GetModuleFileNameA(NULL, path, sizeof(path));
    slash = strrchr(path, '\\');
    strcpy(slash ? slash + 1 : path, "fg.log");
    f = fopen(path, "a");
    if (!f)
        return;
    fprintf(f, "%s '%s' foreground=%d\n", msg, title, fg);
    fclose(f);
}

int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR text, int d)
{
    (void)a; (void)b; (void)d;
    SetProcessDPIAware();
    for (HWND w = GetTopWindow(NULL); w; w = GetNextWindow(w, GW_HWNDNEXT)) {
        char t[256];
        RECT r;
        INPUT in[2] = { 0 };
        if (!IsWindowVisible(w) || !GetWindowTextA(w, t, sizeof(t)) || !strstr(t, text))
            continue;
        logf("found", t, GetForegroundWindow() == w);
        ShowWindow(w, SW_RESTORE);
        SetWindowPos(w, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        GetWindowRect(w, &r);
        SetCursorPos((r.left + r.right) / 2, (r.top + r.bottom) / 2);
        in[0].type = in[1].type = INPUT_MOUSE;
        in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(2, in, sizeof(INPUT));
        Sleep(300);
        SetForegroundWindow(w);
        SetWindowPos(w, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        Sleep(200);
        logf("after click", t, GetForegroundWindow() == w);
        return 0;
    }
    logf("not found", text, 0);
    return 1;
}
