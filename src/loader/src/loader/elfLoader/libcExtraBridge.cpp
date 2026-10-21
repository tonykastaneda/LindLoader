#if defined(_WIN32) || defined(__MINGW32__)
/* glibc functions some games need that the other bridges don't provide
   (first needed by Virtua Fighter 5 Final Showdown). Where glibc and the
   Windows C runtime disagree, the glibc behaviour is reproduced: ldiv()
   returns its struct through a hidden pointer, clock() counts microseconds,
   strerror_r() is the GNU version returning a string. */

#include "symbolResolver.hpp"
#include "libcExtraBridge.hpp"
#include "libcBridge.hpp"
#include "../log/log.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

extern "C"
{
    /* glibc: -1 for -inf, 1 for +inf, 0 otherwise. */
    int ll_isinff(float x) { return isinf(x) ? (x < 0 ? -1 : 1) : 0; }
    double ll_nextafter(double x, double y) { return nextafter(x, y); }
    float ll_nextafterf(float x, float y) { return nextafterf(x, y); }
    double ll_round(double x) { return round(x); }
    float ll_roundf(float x) { return roundf(x); }
    void ll_sincosf(float x, float *s, float *c); /* intelBridge.cpp */
    double ll_difftime(int32_t end, int32_t start) { return (double)end - (double)start; }

    /* Linux CLOCKS_PER_SEC is 1000000, the Windows C runtime's is 1000. */
    long ll_clock(void) { return (long)(clock() * (1000000 / CLOCKS_PER_SEC)); }

    /* Linux time_t is 32 bits here. */
    char *ll_ctime_r(const int32_t *t, char *buf)
    {
        __time32_t v = *t;
        const char *s = _ctime32(&v);
        if (!s)
            return NULL;
        strcpy(buf, s);
        return buf;
    }

    /* The GNU strerror_r returns the message (maybe not in buf). */
    char *ll_strerror_r(int errnum, char *buf, size_t len)
    {
        const char *s = strerror(errnum);
        if (buf && len)
        {
            strncpy(buf, s ? s : "Unknown error", len - 1);
            buf[len - 1] = 0;
            return buf;
        }
        return (char *)s;
    }

    const char *ll_inet_ntop(int af, const void *src, char *dst, int size)
    {
        /* Linux AF_INET6 is 10, Windows' is 23. */
        return InetNtopA(af == 10 ? AF_INET6 : af, (void *)src, dst, size);
    }

    int ll_umask(int mask) { return _umask(mask); }

    void *ll_mmap64(void *addr, size_t length, int prot, int flags, int fd, long long offset)
    {
        return LibcBridge::bridgeMmap(addr, length, prot, flags, fd, (off_t)offset);
    }

    int ll_pthread_yield(void)
    {
        SwitchToThread();
        return 0;
    }

    /* Only gettid (224 on i386) has a meaning here. */
    long ll_syscall(long number, ...)
    {
        if (number == 224)
            return (long)GetCurrentThreadId();
        errno = 38; /* Linux ENOSYS */
        return -1;
    }

    /* No stack walking or malloc tracing for Linux code. */
    int ll_backtrace(void **buffer, int size)
    {
        (void)buffer;
        (void)size;
        return 0;
    }
    void ll_backtrace_symbols_fd(void *const *buffer, int size, int fd)
    {
        (void)buffer;
        (void)size;
        (void)fd;
    }
    void ll_mtrace(void) {}

    /* Nothing to mount on Windows: fail the way an unprivileged Linux
       process would (EPERM). */
    int ll_mount(const char *, const char *, const char *, unsigned long, const void *)
    {
        errno = 1;
        return -1;
    }
    int ll_umount(const char *)
    {
        errno = 1;
        return -1;
    }

    /* ldiv/div: i386 Linux returns structs through a hidden pointer that the
       callee pops (ret $4); MinGW would return them in EDX:EAX. */
    void ll_ldiv(void);
    void ll_div(void);
}

asm(".text\n"
    ".globl _ll_ldiv\n_ll_ldiv:\n"
    ".globl _ll_div\n_ll_div:\n"
    "    movl 8(%esp), %eax\n" /* numerator (after the hidden pointer) */
    "    cltd\n"
    "    idivl 12(%esp)\n"
    "    movl 4(%esp), %ecx\n" /* hidden result pointer */
    "    movl %eax, (%ecx)\n"  /* quot */
    "    movl %edx, 4(%ecx)\n" /* rem */
    "    movl %ecx, %eax\n"
    "    ret $4\n");

namespace LibcExtraBridge
{
    void initBridges()
    {
        MAP("__isinff", ll_isinff);
        MAP("nextafter", ll_nextafter);
        MAP("nextafterf", ll_nextafterf);
        MAP("round", ll_round);
        MAP("roundf", ll_roundf);
        MAP("sincosf", ll_sincosf);
        MAP("difftime", ll_difftime);
        MAP("clock", ll_clock);
        MAP("ctime_r", ll_ctime_r);
        MAP("strerror_r", ll_strerror_r);
        MAP("inet_ntop", ll_inet_ntop);
        MAP("umask", ll_umask);
        MAP("mmap64", ll_mmap64);
        MAP("pthread_yield", ll_pthread_yield);
        MAP("syscall", ll_syscall);
        MAP("backtrace", ll_backtrace);
        MAP("backtrace_symbols_fd", ll_backtrace_symbols_fd);
        MAP("mtrace", ll_mtrace);
        MAP("muntrace", ll_mtrace);
        MAP("mount", ll_mount);
        MAP("umount", ll_umount);
        MAP("ldiv", ll_ldiv);
        MAP("div", ll_div); // replaces MathBridge's div, which returned in registers
    }
}
#endif
