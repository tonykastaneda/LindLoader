#pragma once

#include <regex>
#include <stdint.h>

    // Lindbergh standard values Linux x86
    const int L_REG_EXTENDED = 1;
    const int L_REG_ICASE    = 2;
    const int L_REG_NOSUB    = 4;
    // const int L_REG_NEWLINE  = 8; // Not directly supported by std::regex in this setup

    const int L_REG_NOTBOL   = 1;
    const int L_REG_NOTEOL   = 2;

    const int L_REG_NOMATCH  = 1;
    const int L_REG_BADPAT   = 2;

    struct LinuxRegmatch {
        int rm_so;
        int rm_eo;
    };

    struct RegexData {
        std::regex re;
        bool nosub;
    };

namespace RegexBridge
{

    void initBridges();

    extern "C" int __attribute__((cdecl)) bridgeRegcomp(void *preg, const char *pattern, int cflags);
    extern "C" int __attribute__((cdecl)) bridgeRegexec(void *preg, const char *string, size_t nmatch, LinuxRegmatch *pmatch, int eflags);
    extern "C" void __attribute__((cdecl)) bridgeRegfree(void *preg);

} // namespace RegexBridge
