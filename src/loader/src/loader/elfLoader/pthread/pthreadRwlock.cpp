// ============================================================
// pthread_rwlock.cpp - Read-Write Lock Emulation
// ============================================================

#include "pthreadEmu.hpp"
#include "pthreadInternal.hpp"
#include <pthread.h>

// ============================================================
// TLS for tracking read/write lock state per thread
// ============================================================

static DWORD s_tls_rwlock_state = TLS_OUT_OF_INDEXES;

#define RWLOCK_STATE_NONE   0
#define RWLOCK_STATE_READ   1
#define RWLOCK_STATE_WRITE  2

struct RwlockStateMap {
    void* rwlock;
    int state;
    RwlockStateMap* next;
};

static void InitRwlockTLS() {
    if (s_tls_rwlock_state == TLS_OUT_OF_INDEXES) {
        s_tls_rwlock_state = TlsAlloc();
    }
}

static RwlockStateMap* GetRwlockStateList() {
    InitRwlockTLS();
    return (RwlockStateMap*)TlsGetValue(s_tls_rwlock_state);
}

static void SetRwlockState(void* rwlock, int state) {
    InitRwlockTLS();
    
    RwlockStateMap* list = GetRwlockStateList();
    
    // Find existing entry
    for (RwlockStateMap* p = list; p; p = p->next) {
        if (p->rwlock == rwlock) {
            p->state = state;
            return;
        }
    }
    
    // Add new entry
    RwlockStateMap* entry = new RwlockStateMap;
    entry->rwlock = rwlock;
    entry->state = state;
    entry->next = list;
    TlsSetValue(s_tls_rwlock_state, entry);
}

static int GetRwlockState(void* rwlock) {
    RwlockStateMap* list = GetRwlockStateList();
    
    for (RwlockStateMap* p = list; p; p = p->next) {
        if (p->rwlock == rwlock) {
            return p->state;
        }
    }
    
    return RWLOCK_STATE_NONE;
}

static void ClearRwlockState(void* rwlock) {
    SetRwlockState(rwlock, RWLOCK_STATE_NONE);
}

// ============================================================
// Rwlock Attribute Functions
// ============================================================

int PthreadEmu::pthreadRwlockattrInit(void* attr) {
    if (!attr) return LINUX_EINVAL;
    
    PthreadRwlockAttrInternal* a = new PthreadRwlockAttrInternal;
    a->pshared = LINUX_PTHREAD_PROCESS_PRIVATE;
    
    *(void**)attr = a;
    return 0;
}

int PthreadEmu::pthreadRwlockattrDestroy(void* attr) {
    if (!attr) return LINUX_EINVAL;
    
    PthreadRwlockAttrInternal* a = *(PthreadRwlockAttrInternal**)attr;
    if (a) {
        delete a;
        *(void**)attr = nullptr;
    }
    return 0;
}

int PthreadEmu::pthreadRwlockattrSetpshared(void* attr, int pshared) {
    if (!attr) return LINUX_EINVAL;
    
    PthreadRwlockAttrInternal* a = *(PthreadRwlockAttrInternal**)attr;
    if (!a) return LINUX_EINVAL;
    
    if (pshared != LINUX_PTHREAD_PROCESS_PRIVATE) {
        return LINUX_EINVAL;
    }
    
    a->pshared = pshared;
    return 0;
}

int PthreadEmu::pthreadRwlockattrGetpshared(const void* attr, int* pshared) {
    if (!attr || !pshared) return LINUX_EINVAL;
    
    PthreadRwlockAttrInternal* a = *(PthreadRwlockAttrInternal**)attr;
    if (!a) return LINUX_EINVAL;
    
    *pshared = a->pshared;
    return 0;
}

// ============================================================
// Rwlock Functions
// ============================================================

int PthreadEmu::pthreadRwlockInit(void* rwlock, const void* attr) {
    if (!rwlock) return LINUX_EINVAL;
    
    PthreadRwlockInternal* rw = PthreadMapper::GetOrCreateRwlock(rwlock);
    if (!rw) return LINUX_ENOMEM;
    
    // SRWLOCK is initialized in GetOrCreateRwlock
    return 0;
}

int PthreadEmu::pthreadRwlockDestroy(void* rwlock) {
    if (!rwlock) return LINUX_EINVAL;
    
    // SRWLOCK doesn't need explicit destruction
    PthreadMapper::DestroyRwlock(rwlock);
    return 0;
}

int PthreadEmu::pthreadRwlockRdlock(void* rwlock) {
    if (!rwlock) return LINUX_EINVAL;
    
    PthreadRwlockInternal* rw = PthreadMapper::GetOrCreateRwlock(rwlock);
    if (!rw) return LINUX_EINVAL;
    
    AcquireSRWLockShared(&rw->srw);
    SetRwlockState(rwlock, RWLOCK_STATE_READ);
    
    return 0;
}

int PthreadEmu::pthreadRwlockTryrdlock(void* rwlock) {
    if (!rwlock) return LINUX_EINVAL;
    
    PthreadRwlockInternal* rw = PthreadMapper::GetOrCreateRwlock(rwlock);
    if (!rw) return LINUX_EINVAL;
    
    if (!TryAcquireSRWLockShared(&rw->srw)) {
        return LINUX_EBUSY;
    }
    
    SetRwlockState(rwlock, RWLOCK_STATE_READ);
    return 0;
}

int PthreadEmu::pthreadRwlockTimedrdlock(void* rwlock, const struct timespec* abstime) {
    if (!rwlock) return LINUX_EINVAL;
    if (!abstime) return pthreadRwlockRdlock(rwlock);
    
    // Windows SRWLOCK doesn't have timed acquire
    // Implement with spin + sleep
    DWORD timeout_ms = TimespecToMilliseconds(abstime);
    DWORD start = GetTickCount();
    
    while (true) {
        int result = pthreadRwlockTryrdlock(rwlock);
        if (result == 0) {
            return 0;
        }
        
        DWORD elapsed = GetTickCount() - start;
        if (elapsed >= timeout_ms) {
            return LINUX_ETIMEDOUT;
        }
        
        Sleep(1);
    }
}

int PthreadEmu::pthreadRwlockWrlock(void* rwlock) {
    if (!rwlock) return LINUX_EINVAL;
    
    PthreadRwlockInternal* rw = PthreadMapper::GetOrCreateRwlock(rwlock);
    if (!rw) return LINUX_EINVAL;
    
    AcquireSRWLockExclusive(&rw->srw);
    SetRwlockState(rwlock, RWLOCK_STATE_WRITE);
    
    return 0;
}

int PthreadEmu::pthreadRwlockTrywrlock(void* rwlock) {
    if (!rwlock) return LINUX_EINVAL;
    
    PthreadRwlockInternal* rw = PthreadMapper::GetOrCreateRwlock(rwlock);
    if (!rw) return LINUX_EINVAL;
    
    if (!TryAcquireSRWLockExclusive(&rw->srw)) {
        return LINUX_EBUSY;
    }
    
    SetRwlockState(rwlock, RWLOCK_STATE_WRITE);
    return 0;
}

int PthreadEmu::pthreadRwlockTimedwrlock(void* rwlock, const struct timespec* abstime) {
    if (!rwlock) return LINUX_EINVAL;
    if (!abstime) return pthreadRwlockWrlock(rwlock);
    
    DWORD timeout_ms = TimespecToMilliseconds(abstime);
    DWORD start = GetTickCount();
    
    while (true) {
        int result = pthreadRwlockTrywrlock(rwlock);
        if (result == 0) {
            return 0;
        }
        
        DWORD elapsed = GetTickCount() - start;
        if (elapsed >= timeout_ms) {
            return LINUX_ETIMEDOUT;
        }
        
        Sleep(1);
    }
}

int PthreadEmu::pthreadRwlockUnlock(void* rwlock) {
    if (!rwlock) return LINUX_EINVAL;
    
    PthreadRwlockInternal* rw = PthreadMapper::FindRwlock(rwlock);
    if (!rw) return LINUX_EINVAL;
    
    int state = GetRwlockState(rwlock);
    
    switch (state) {
        case RWLOCK_STATE_READ:
            ReleaseSRWLockShared(&rw->srw);
            break;
            
        case RWLOCK_STATE_WRITE:
            ReleaseSRWLockExclusive(&rw->srw);
            break;
            
        default:
            // Not locked by this thread
            return LINUX_EPERM;
    }
    
    ClearRwlockState(rwlock);
    return 0;
}
