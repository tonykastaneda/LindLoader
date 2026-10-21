#include "segaapiBridge.hpp"
#include "symbolResolver.hpp"
#include "../log/log.h"
extern "C"
{
#include "../audio/lindbergh/libsegaapi/libsegaapi.h"
}

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

namespace SegaApiBridge
{
    void initBridges()
    {
        log_info("Initializing Sega API Bridges...");

        MAP("SEGAAPI_Play", SEGAAPI_Play);
        MAP("SEGAAPI_Pause", SEGAAPI_Pause);
        MAP("SEGAAPI_Stop", SEGAAPI_Stop);
        MAP("SEGAAPI_PlayWithSetup", SEGAAPI_PlayWithSetup);
        MAP("SEGAAPI_GetPlaybackStatus", SEGAAPI_GetPlaybackStatus);
        MAP("SEGAAPI_SetFormat", SEGAAPI_SetFormat);
        MAP("SEGAAPI_GetFormat", SEGAAPI_GetFormat);
        MAP("SEGAAPI_SetSampleRate", SEGAAPI_SetSampleRate);
        MAP("SEGAAPI_GetSampleRate", SEGAAPI_GetSampleRate);
        MAP("SEGAAPI_SetPriority", SEGAAPI_SetPriority);
        MAP("SEGAAPI_GetPriority", SEGAAPI_GetPriority);
        MAP("SEGAAPI_SetUserData", SEGAAPI_SetUserData);
        MAP("SEGAAPI_GetUserData", SEGAAPI_GetUserData);
        MAP("SEGAAPI_SetSendRouting", SEGAAPI_SetSendRouting);
        MAP("SEGAAPI_GetSendRouting", SEGAAPI_GetSendRouting);
        MAP("SEGAAPI_SetSendLevel", SEGAAPI_SetSendLevel);
        MAP("SEGAAPI_GetSendLevel", SEGAAPI_GetSendLevel);
        MAP("SEGAAPI_SetChannelVolume", SEGAAPI_SetChannelVolume);
        MAP("SEGAAPI_GetChannelVolume", SEGAAPI_GetChannelVolume);
        MAP("SEGAAPI_SetPlaybackPosition", SEGAAPI_SetPlaybackPosition);
        MAP("SEGAAPI_GetPlaybackPosition", SEGAAPI_GetPlaybackPosition);
        MAP("SEGAAPI_SetNotificationFrequency", SEGAAPI_SetNotificationFrequency);
        MAP("SEGAAPI_SetNotificationPoint", SEGAAPI_SetNotificationPoint);
        MAP("SEGAAPI_ClearNotificationPoint", SEGAAPI_ClearNotificationPoint);
        MAP("SEGAAPI_SetStartLoopOffset", SEGAAPI_SetStartLoopOffset);
        MAP("SEGAAPI_GetStartLoopOffset", SEGAAPI_GetStartLoopOffset);
        MAP("SEGAAPI_SetEndLoopOffset", SEGAAPI_SetEndLoopOffset);
        MAP("SEGAAPI_GetEndLoopOffset", SEGAAPI_GetEndLoopOffset);
        MAP("SEGAAPI_SetEndOffset", SEGAAPI_SetEndOffset);
        MAP("SEGAAPI_GetEndOffset", SEGAAPI_GetEndOffset);
        MAP("SEGAAPI_SetLoopState", SEGAAPI_SetLoopState);
        MAP("SEGAAPI_GetLoopState", SEGAAPI_GetLoopState);
        MAP("SEGAAPI_UpdateBuffer", SEGAAPI_UpdateBuffer);
        MAP("SEGAAPI_SetSynthParam", SEGAAPI_SetSynthParam);
        MAP("SEGAAPI_GetSynthParam", SEGAAPI_GetSynthParam);
        MAP("SEGAAPI_SetSynthParamMultiple", SEGAAPI_SetSynthParamMultiple);
        MAP("SEGAAPI_GetSynthParamMultiple", SEGAAPI_GetSynthParamMultiple);
        MAP("SEGAAPI_SetReleaseState", SEGAAPI_SetReleaseState);
        MAP("SEGAAPI_CreateBuffer", SEGAAPI_CreateBuffer);
        MAP("SEGAAPI_DestroyBuffer", SEGAAPI_DestroyBuffer);
        MAP("SEGAAPI_SetGlobalEAXProperty", SEGAAPI_SetGlobalEAXProperty);
        MAP("SEGAAPI_GetGlobalEAXProperty", SEGAAPI_GetGlobalEAXProperty);
        MAP("SEGAAPI_SetSPDIFOutChannelStatus", SEGAAPI_SetSPDIFOutChannelStatus);
        MAP("SEGAAPI_GetSPDIFOutChannelStatus", SEGAAPI_GetSPDIFOutChannelStatus);
        MAP("SEGAAPI_SetSPDIFOutSampleRate", SEGAAPI_SetSPDIFOutSampleRate);
        MAP("SEGAAPI_GetSPDIFOutSampleRate", SEGAAPI_GetSPDIFOutSampleRate);
        MAP("SEGAAPI_SetSPDIFOutChannelRouting", SEGAAPI_SetSPDIFOutChannelRouting);
        MAP("SEGAAPI_GetSPDIFOutChannelRouting", SEGAAPI_GetSPDIFOutChannelRouting);
        MAP("SEGAAPI_SetIOVolume", SEGAAPI_SetIOVolume);
        MAP("SEGAAPI_GetIOVolume", SEGAAPI_GetIOVolume);
        MAP("SEGAAPI_SetLastStatus", SEGAAPI_SetLastStatus);
        MAP("SEGAAPI_GetLastStatus", SEGAAPI_GetLastStatus);
        MAP("SEGAAPI_Reset", SEGAAPI_Reset);
        MAP("SEGAAPI_Init", SEGAAPI_Init);
        MAP("SEGAAPI_Exit", SEGAAPI_Exit);
    }
}