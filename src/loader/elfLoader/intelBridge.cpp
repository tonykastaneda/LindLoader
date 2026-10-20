#if defined(_WIN32) || defined(__MINGW32__)
/* Intel compiler runtime (libirc.so, libimf.so, libsvml.so) for games built
   with icc, such as Sega Race TV. The loader treats these libraries as
   INTERNAL, so their functions have to be provided here.

   __libm_sse2_* do not use the cdecl convention: the argument is in xmm0
   (and xmm1 for atan2), the result comes back in xmm0 (sincos: sine in
   xmm0, cosine in xmm1), and callers keep live values in the other xmm
   registers across the call. The wrappers below save every general and
   xmm register they don't return in, call the C math function, and move
   its x87 result into xmm0. */

#include "symbolResolver.hpp"
#include "intelBridge.hpp"

#include <math.h>
#include <string.h>

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

extern "C"
{
    void *ll_intel_memcpy(void *dst, const void *src, size_t n) { return memcpy(dst, src, n); }
    void *ll_intel_memset(void *dst, int c, size_t n) { return memset(dst, c, n); }
    int ll_intel_memcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }
    void ll_intel_new_proc_init(int) {}
    void ll_sincosf(float x, float *s, float *c) { *s = sinf(x); *c = cosf(x); }
    void ll_sincos(double x, double *s, double *c) { *s = sin(x); *c = cos(x); }

    /* Defined in the assembly below. */
    void ll_sse2_acos(void), ll_sse2_acosf(void), ll_sse2_asinf(void), ll_sse2_atan(void), ll_sse2_atan2(void),
        ll_sse2_atanf(void), ll_sse2_cosf(void), ll_sse2_log10f(void), ll_sse2_logf(void), ll_sse2_sin(void),
        ll_sse2_sincos(void), ll_sse2_sincosf(void), ll_sse2_sinf(void), ll_sse2_tanf(void);
}

/* Frame: push ebp/eax/ecx/edx, align to 16, 144 bytes:
   [esp+0..15] call arguments, [esp+16..31] sincos results,
   [esp+32..143] saved xmm1-xmm7. */
#define SSE2_ENTER                                                                                                      \
    "push ebp\n mov ebp, esp\n push eax\n push ecx\n push edx\n and esp, -16\n sub esp, 144\n"                       \
    "movups [esp+32], xmm1\n movups [esp+48], xmm2\n movups [esp+64], xmm3\n movups [esp+80], xmm4\n"                \
    "movups [esp+96], xmm5\n movups [esp+112], xmm6\n movups [esp+128], xmm7\n"
#define SSE2_LEAVE_KEEP_XMM1                                                                                            \
    "movups xmm2, [esp+48]\n movups xmm3, [esp+64]\n movups xmm4, [esp+80]\n movups xmm5, [esp+96]\n"                \
    "movups xmm6, [esp+112]\n movups xmm7, [esp+128]\n lea esp, [ebp-12]\n pop edx\n pop ecx\n pop eax\n pop ebp\n ret\n"
#define SSE2_LEAVE "movups xmm1, [esp+32]\n" SSE2_LEAVE_KEEP_XMM1

/* float f(float) */
#define SSE2_F1(name, cfunc)                                                                                            \
    ".globl _" #name "\n_" #name ":\n" SSE2_ENTER "movss [esp], xmm0\n call _" #cfunc "\n fstp dword ptr [esp]\n"    \
    "movss xmm0, [esp]\n" SSE2_LEAVE
/* double f(double) */
#define SSE2_D1(name, cfunc)                                                                                            \
    ".globl _" #name "\n_" #name ":\n" SSE2_ENTER "movsd [esp], xmm0\n call _" #cfunc "\n fstp qword ptr [esp]\n"    \
    "movsd xmm0, [esp]\n" SSE2_LEAVE

asm(".intel_syntax noprefix\n"
    ".text\n"
    SSE2_F1(ll_sse2_acosf, acosf)
    SSE2_F1(ll_sse2_asinf, asinf)
    SSE2_F1(ll_sse2_atanf, atanf)
    SSE2_F1(ll_sse2_cosf, cosf)
    SSE2_F1(ll_sse2_log10f, log10f)
    SSE2_F1(ll_sse2_logf, logf)
    SSE2_F1(ll_sse2_sinf, sinf)
    SSE2_F1(ll_sse2_tanf, tanf)
    SSE2_D1(ll_sse2_acos, acos)
    SSE2_D1(ll_sse2_atan, atan)
    SSE2_D1(ll_sse2_sin, sin)
    /* double atan2(double y in xmm0, double x in xmm1) */
    ".globl _ll_sse2_atan2\n_ll_sse2_atan2:\n" SSE2_ENTER
    "movsd [esp], xmm0\n movsd [esp+8], xmm1\n call _atan2\n fstp qword ptr [esp]\n movsd xmm0, [esp]\n" SSE2_LEAVE
    /* sincosf(float in xmm0) -> sin in xmm0, cos in xmm1 */
    ".globl _ll_sse2_sincosf\n_ll_sse2_sincosf:\n" SSE2_ENTER
    "movss [esp], xmm0\n lea eax, [esp+16]\n mov [esp+4], eax\n lea eax, [esp+20]\n mov [esp+8], eax\n"
    "call _ll_sincosf\n movss xmm0, [esp+16]\n movss xmm1, [esp+20]\n" SSE2_LEAVE_KEEP_XMM1
    /* sincos(double in xmm0) -> sin in xmm0, cos in xmm1 */
    ".globl _ll_sse2_sincos\n_ll_sse2_sincos:\n" SSE2_ENTER
    "movsd [esp], xmm0\n lea eax, [esp+16]\n mov [esp+8], eax\n lea eax, [esp+24]\n mov [esp+12], eax\n"
    "call _ll_sincos\n movsd xmm0, [esp+16]\n movsd xmm1, [esp+24]\n" SSE2_LEAVE_KEEP_XMM1
    ".att_syntax prefix\n");

namespace IntelBridge
{
    void initBridges()
    {
        // libirc
        MAP("_intel_fast_memcpy", ll_intel_memcpy);
        MAP("_intel_fast_memset", ll_intel_memset);
        MAP("_intel_fast_memcmp", ll_intel_memcmp);
        MAP("__intel_VEC_memcpy", ll_intel_memcpy);
        MAP("__intel_VEC_memzero", ll_intel_memset); // called as (dst, 0, n)
        MAP("__intel_new_proc_init_P", ll_intel_new_proc_init);
        // libimf, SSE2 register convention
        MAP("__libm_sse2_acos", ll_sse2_acos);
        MAP("__libm_sse2_acosf", ll_sse2_acosf);
        MAP("__libm_sse2_asinf", ll_sse2_asinf);
        MAP("__libm_sse2_atan", ll_sse2_atan);
        MAP("__libm_sse2_atan2", ll_sse2_atan2);
        MAP("__libm_sse2_atanf", ll_sse2_atanf);
        MAP("__libm_sse2_cosf", ll_sse2_cosf);
        MAP("__libm_sse2_log10f", ll_sse2_log10f);
        MAP("__libm_sse2_logf", ll_sse2_logf);
        MAP("__libm_sse2_sin", ll_sse2_sin);
        MAP("__libm_sse2_sincos", ll_sse2_sincos);
        MAP("__libm_sse2_sincosf", ll_sse2_sincosf);
        MAP("__libm_sse2_sinf", ll_sse2_sinf);
        MAP("__libm_sse2_tanf", ll_sse2_tanf);
    }
}
#endif
