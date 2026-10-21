/* Test helper: capture one window's contents to a .bmp even when other
   windows cover it (PrintWindow with PW_RENDERFULLCONTENT).
   Usage: wincap <out.bmp> <title text> */
#include <windows.h>
#include <string.h>
#include <stdio.h>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x2
#endif

int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR cmd, int d)
{
    char out[MAX_PATH], *title;
    (void)a; (void)b; (void)d;
    SetProcessDPIAware();
    strncpy(out, cmd, sizeof(out) - 1);
    out[sizeof(out) - 1] = 0;
    title = strchr(out, ' ');
    if (!title)
        return 2;
    *title++ = 0;
    for (HWND w = GetTopWindow(NULL); w; w = GetNextWindow(w, GW_HWNDNEXT)) {
        char t[256];
        RECT r;
        if (!IsWindowVisible(w) || !GetWindowTextA(w, t, sizeof(t)) || !strstr(t, title))
            continue;
        GetClientRect(w, &r);
        int W = r.right, H = r.bottom;
        HDC wdc = GetDC(w), mdc = CreateCompatibleDC(wdc);
        HBITMAP bmp = CreateCompatibleBitmap(wdc, W, H);
        SelectObject(mdc, bmp);
        PrintWindow(w, mdc, PW_CLIENTONLY | PW_RENDERFULLCONTENT);
        BITMAPINFOHEADER bi = { sizeof(bi), W, H, 1, 24, BI_RGB };
        int stride = (W * 3 + 3) & ~3;
        char *px = malloc((size_t)stride * H);
        GetDIBits(mdc, bmp, 0, H, px, (BITMAPINFO *)&bi, DIB_RGB_COLORS);
        BITMAPFILEHEADER fh = { 0x4d42, sizeof(fh) + sizeof(bi) + stride * H, 0, 0, sizeof(fh) + sizeof(bi) };
        FILE *f = fopen(out, "wb");
        if (f) {
            fwrite(&fh, sizeof(fh), 1, f);
            fwrite(&bi, sizeof(bi), 1, f);
            fwrite(px, (size_t)stride * H, 1, f);
            fclose(f);
        }
        free(px);
        DeleteObject(bmp);
        DeleteDC(mdc);
        ReleaseDC(w, wdc);
        return f ? 0 : 1;
    }
    return 1;
}
