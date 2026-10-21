/* Test helper: write every top-level window with a title to winlist.txt
   (title, visible, minimised, rect, owning pid). */
#include <windows.h>
#include <stdio.h>

static FILE *out;

static BOOL CALLBACK each(HWND w, LPARAM l)
{
    char t[256];
    RECT r;
    DWORD pid;
    (void)l;
    if (!GetWindowTextA(w, t, sizeof(t)))
        return TRUE;
    GetWindowRect(w, &r);
    GetWindowThreadProcessId(w, &pid);
    fprintf(out, "%-6lu vis=%d min=%d fg=%d %ld,%ld,%ld,%ld %s\n", pid, IsWindowVisible(w) != 0, IsIconic(w) != 0,
            GetForegroundWindow() == w, r.left, r.top, r.right, r.bottom, t);
    return TRUE;
}

int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR c, int d)
{
    (void)a; (void)b; (void)d;
    out = fopen(c && *c ? c : "winlist.txt", "w");
    if (!out)
        return 1;
    EnumWindows(each, 0);
    fclose(out);
    return 0;
}
