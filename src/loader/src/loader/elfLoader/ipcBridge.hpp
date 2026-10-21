#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <windows.h>
#include <map>
#include <stddef.h>

struct ShmInfo
{
    HANDLE hMap;
    void *pMem;
    size_t size;
    int key;
};

static std::map<int, ShmInfo> g_shmMap;

namespace IpcBridge 
{
    void initBridges();
}

extern "C" 
{
    int bridgeShmget(int key, size_t size, int shmflg);
    void *bridgeShmat(int shmid, const void *shmaddr, int shmflg);
    int bridgeShmctl(int shmid, int cmd, void *buf);
    int bridgeShmdt(const void *shmaddr);
};