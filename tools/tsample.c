/* Test helper: sample the instruction pointer of every thread in a 32-bit
   process several times. Usage: tsample <pid> [samples] */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    DWORD pid = strtoul(argv[1], NULL, 10);
    int samples = argc > 2 ? atoi(argv[2]) : 10;
    for (int s = 0; s < samples; s++) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        THREADENTRY32 te = { sizeof(te) };
        for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
            if (te.th32OwnerProcessID != pid)
                continue;
            HANDLE t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID);
            if (!t)
                continue;
            if (SuspendThread(t) != (DWORD)-1) {
                CONTEXT c = { 0 };
                c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
                if (GetThreadContext(t, &c))
                    printf("%lu %08lx\n", te.th32ThreadID, c.Eip);
                ResumeThread(t);
            }
            CloseHandle(t);
        }
        CloseHandle(snap);
        Sleep(200);
    }
    return 0;
}
