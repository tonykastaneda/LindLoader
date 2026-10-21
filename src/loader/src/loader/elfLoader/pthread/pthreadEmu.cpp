// ============================================================
// pthread_emu.cpp - Init/Shutdown and C API Exports
// ============================================================

#include "pthreadEmu.hpp"
#include "pthreadInternal.hpp"

// External declaration for TLS destructor support
extern void CallTlsDestructors();

// ============================================================
// Global State
// ============================================================

static volatile bool s_emu_initialized = false;
static CRITICAL_SECTION s_init_lock;
static volatile bool s_init_lock_ready = false;

// One-time initialization for the init lock itself
static void EnsureInitLock() {
    if (!s_init_lock_ready) {
        // This is a potential race, but only at very first initialization
        // In practice, Initialize() should be called from main thread first
        InitializeCriticalSection(&s_init_lock);
        s_init_lock_ready = true;
    }
}

// ============================================================
// Initialize / Shutdown
// ============================================================

void PthreadEmu::Initialize() {
    EnsureInitLock();

    EnterCriticalSection(&s_init_lock);

    if (s_emu_initialized) {
        LeaveCriticalSection(&s_init_lock);
        return;
    }

    // Initialize the mapper (handles all object tracking)
    PthreadMapper::Initialize();

    s_emu_initialized = true;

    LeaveCriticalSection(&s_init_lock);
}

void PthreadEmu::Shutdown() {
    EnsureInitLock();

    EnterCriticalSection(&s_init_lock);

    if (!s_emu_initialized) {
        LeaveCriticalSection(&s_init_lock);
        return;
    }

    // Call TLS destructors for main thread
    CallTlsDestructors();

    // Cleanup semaphores
    SemaphoreCleanup();

    // Shutdown the mapper (cleans up all objects)
    PthreadMapper::Shutdown();

    s_emu_initialized = false;

    LeaveCriticalSection(&s_init_lock);
}

// ============================================================
// C API Exports (optional, for direct hooking)
// ============================================================

extern "C" {

    // --- Initialization ---
    void emuPthreadInit() {
        PthreadEmu::Initialize();
    }

    void emuPthreadShutdown() {
        PthreadEmu::Shutdown();
    }

    // --- Thread Functions ---
    int emuPthreadCreate(void* thread, const void* attr,
        void* (*start_routine)(void*), void* arg) {
        return PthreadEmu::pthreadCreate(thread, attr, start_routine, arg);
    }

    int emuPthreadJoin(uint32_t thread, void** retval) {
        return PthreadEmu::pthreadJoin(thread, retval);
    }

    int emuPthreadDetach(uint32_t thread) {
        return PthreadEmu::pthreadDetach(thread);
    }

    void emuPthreadExit(void* retval) {
        PthreadEmu::pthreadExit(retval);
    }

    uint32_t emuPthreadSelf() {
        return PthreadEmu::pthreadSelf();
    }

    int emuPthreadEqual(uint32_t t1, uint32_t t2) {
        return PthreadEmu::pthreadEqual(t1, t2);
    }

    // --- Mutex Functions ---
    int emuPthreadMutexInit(void* mutex, const void* attr) {
        return PthreadEmu::pthreadMutexInit(mutex, attr);
    }

    int emuPthreadMutexDestroy(void* mutex) {
        return PthreadEmu::pthreadMutexDestroy(mutex);
    }

    int emuPthreadMutexLock(void* mutex) {
        return PthreadEmu::pthreadMutexLock(mutex);
    }

    int emuPthreadMutexTrylock(void* mutex) {
        return PthreadEmu::pthreadMutexTrylock(mutex);
    }

    int emuPthreadMutexTimedlock(void* mutex, const struct timespec* abstime) {
        return PthreadEmu::pthreadMutexTimedlock(mutex, abstime);
    }

    int emuPthreadMutexUnlock(void* mutex) {
        return PthreadEmu::pthreadMutexUnlock(mutex);
    }

    // --- Mutex Attribute Functions ---
    int emuPthreadMutexattrInit(void* attr) {
        return PthreadEmu::pthreadMutexattrInit(attr);
    }

    int emuPthreadMutexattrDestroy(void* attr) {
        return PthreadEmu::pthreadMutexattrDestroy(attr);
    }

    int emuPthreadMutexattrSettype(void* attr, int type) {
        return PthreadEmu::pthreadMutexattrSettype(attr, type);
    }

    int emuPthreadMutexattrGettype(const void* attr, int* type) {
        return PthreadEmu::pthreadMutexattrGettype(attr, type);
    }

    // --- Condition Variable Functions ---
    int emuPthreadCondInit(void* cond, const void* attr) {
        return PthreadEmu::pthreadCondInit(cond, attr);
    }

    int emuPthreadCondDestroy(void* cond) {
        return PthreadEmu::pthreadCondDestroy(cond);
    }

    int emuPthreadCondWait(void* cond, void* mutex) {
        return PthreadEmu::pthreadCondWait(cond, mutex);
    }

    int emuPthreadCondTimedwait(void* cond, void* mutex, const struct timespec* abstime) {
        return PthreadEmu::pthreadCondTimedwait(cond, mutex, abstime);
    }

    int emuPthreadCondSignal(void* cond) {
        return PthreadEmu::pthreadCondSignal(cond);
    }

    int emuPthreadCondBroadcast(void* cond) {
        return PthreadEmu::pthreadCondBroadcast(cond);
    }

    // --- Read-Write Lock Functions ---
    int emuPthreadRwlockInit(void* rwlock, const void* attr) {
        return PthreadEmu::pthreadRwlockInit(rwlock, attr);
    }

    int emuPthreadRwlockDestroy(void* rwlock) {
        return PthreadEmu::pthreadRwlockDestroy(rwlock);
    }

    int emuPthreadRwlockRdlock(void* rwlock) {
        return PthreadEmu::pthreadRwlockRdlock(rwlock);
    }

    int emuPthreadRwlockTryrdlock(void* rwlock) {
        return PthreadEmu::pthreadRwlockTryrdlock(rwlock);
    }

    int emuPthreadRwlockWrlock(void* rwlock) {
        return PthreadEmu::pthreadRwlockWrlock(rwlock);
    }

    int emuPthreadRwlockTrywrlock(void* rwlock) {
        return PthreadEmu::pthreadRwlockTrywrlock(rwlock);
    }

    int emuPthreadRwlockUnlock(void* rwlock) {
        return PthreadEmu::pthreadRwlockUnlock(rwlock);
    }

    // --- Barrier Functions ---
    int emuPthreadBarrierInit(void* barrier, const void* attr, unsigned int count) {
        return PthreadEmu::pthreadBarrierInit(barrier, attr, count);
    }

    int emuPthreadBarrierDestroy(void* barrier) {
        return PthreadEmu::pthreadBarrierDestroy(barrier);
    }

    int emuPthreadBarrierWait(void* barrier) {
        return PthreadEmu::pthreadBarrierWait(barrier);
    }

    // --- Spinlock Functions ---
    int emuPthreadSpinInit(void* lock, int pshared) {
        return PthreadEmu::pthreadSpinInit(lock, pshared);
    }

    int emuPthreadSpinDestroy(void* lock) {
        return PthreadEmu::pthreadSpinDestroy(lock);
    }

    int emuPthreadSpinLock(void* lock) {
        return PthreadEmu::pthreadSpinLock(lock);
    }

    int emuPthreadSpinTrylock(void* lock) {
        return PthreadEmu::pthreadSpinTrylock(lock);
    }

    int emuPthreadSpinUnlock(void* lock) {
        return PthreadEmu::pthreadSpinUnlock(lock);
    }

    // --- Once Function ---
    int emuPthreadOnce(void* once_control, void (*init_routine)(void)) {
        return PthreadEmu::pthreadOnce(once_control, init_routine);
    }

    // --- TLS Functions ---
    int emuPthreadKeyCreate(void* key, void (*destructor)(void*)) {
        return PthreadEmu::pthreadKeyCreate(key, destructor);
    }

    int emuPthreadKeyDelete(uint32_t key) {
        return PthreadEmu::pthreadKeyDelete(key);
    }

    int emuPthreadSetspecific(uint32_t key, const void* value) {
        return PthreadEmu::pthreadSetSpecific(key, value);
    }

    void* emuPthreadGetspecific(uint32_t key) {
        return PthreadEmu::pthreadGetSpecific(key);
    }

    // --- Scheduling ---
    int emuPthreadSchedYield() {
        return PthreadEmu::schedYield();
    }

} // extern "C"
