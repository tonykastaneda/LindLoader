#pragma once

#include <SDL3/SDL.h>

// The Windows raw input device path of a mouse ("\\?\HID#VID_046D&PID_C08B&MI_00#..."), which
// stays the same between runs while the mouse stays in the same port. Returns 0 when unknown
// (always on Linux).
int getMouseDevicePath(SDL_MouseID id, char *out, int size);
