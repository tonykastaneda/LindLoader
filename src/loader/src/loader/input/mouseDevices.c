#include "mouseDevices.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h>

int getMouseDevicePath(SDL_MouseID id, char *out, int size)
{
    // With raw input, SDL's mouse IDs are the raw input device handles.
    UINT length = (UINT)size;
    if (size <= 0 || GetRawInputDeviceInfoA((HANDLE)(uintptr_t)id, RIDI_DEVICENAME, out, &length) == (UINT)-1)
        return 0;
    out[size - 1] = '\0';
    return out[0] != '\0';
}
#else
int getMouseDevicePath(SDL_MouseID id, char *out, int size)
{
    (void)id;
    if (size > 0)
        out[0] = '\0';
    return 0;
}
#endif
