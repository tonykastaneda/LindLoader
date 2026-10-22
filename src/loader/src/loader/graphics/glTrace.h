#pragma once
/* Diagnostics: with LL_GLTRACE=<file>, glTrace() appends each distinct line once (first 2000) to that file. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void glTrace(const char *fmt, ...)
{
    static int state = 0; /* 0 unknown, 1 on, -1 off */
    static FILE *out = NULL;
    static unsigned long seen[4096];
    static int lines = 0;
    if (state == 0)
    {
        const char *path = getenv("LL_GLTRACE");
        out = path && *path ? fopen(path, "a") : NULL;
        state = out ? 1 : -1;
    }
    if (state < 0 || lines >= 2000)
        return;
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    unsigned long h = 5381;
    for (const char *c = line; *c; c++)
        h = h * 33 + (unsigned char)*c;
    for (int i = 0; i < lines && i < 4096; i++)
        if (seen[i] == h)
            return;
    seen[lines++] = h;
    fprintf(out, "%s\n", line);
    fflush(out);
}
