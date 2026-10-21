#if defined(_WIN32) || defined(__MINGW32__)

#include "alsa2sdlBridge.hpp"
#include "symbolResolver.hpp"
#include "../audio/lindbergh/alsa2sdl/alsa2sdl.h"

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

namespace Alsa2SdlBridge
{
    void initBridges()
    {
        MAP("snd_pcm_open", shared_snd_pcm_open);
        MAP("snd_pcm_close", shared_snd_pcm_close);

        MAP("snd_pcm_hw_params_malloc", shared_snd_pcm_hw_params_malloc);
        MAP("snd_pcm_hw_params_free", shared_snd_pcm_hw_params_free);
        MAP("snd_pcm_hw_params_any", shared_snd_pcm_hw_params_any);
        MAP("snd_pcm_hw_params_set_access", shared_snd_pcm_hw_params_set_access);
        MAP("snd_pcm_hw_params_set_format", shared_snd_pcm_hw_params_set_format);
        MAP("snd_pcm_hw_params_set_channels", shared_snd_pcm_hw_params_set_channels);
        MAP("snd_pcm_hw_params_set_rate_near", shared_snd_pcm_hw_params_set_rate_near);
        MAP("snd_pcm_hw_params_set_periods", shared_snd_pcm_hw_params_set_periods);
        MAP("snd_pcm_hw_params_set_buffer_size", shared_snd_pcm_hw_params_set_buffer_size);
        MAP("snd_pcm_hw_params_set_buffer_size_near", shared_snd_pcm_hw_params_set_buffer_size_near);
        MAP("snd_pcm_hw_params_set_period_size_near", shared_snd_pcm_hw_params_set_period_size_near);
        MAP("snd_pcm_hw_params", shared_snd_pcm_hw_params);

        MAP("snd_pcm_sw_params_malloc", shared_snd_pcm_sw_params_malloc);
        MAP("snd_pcm_sw_params_free", shared_snd_pcm_sw_params_free);
        MAP("snd_pcm_sw_params_current", shared_snd_pcm_sw_params_current);
        MAP("snd_pcm_sw_params_set_avail_min", shared_snd_pcm_sw_params_set_avail_min);
        MAP("snd_pcm_sw_params_set_start_threshold", shared_snd_pcm_sw_params_set_start_threshold);
        MAP("snd_pcm_sw_params_set_xfer_align", shared_snd_pcm_sw_params_set_xfer_align);
        MAP("snd_pcm_sw_params", shared_snd_pcm_sw_params);

        MAP("snd_pcm_prepare", shared_snd_pcm_prepare);
        MAP("snd_pcm_start", shared_snd_pcm_start);
        MAP("snd_pcm_drop", shared_snd_pcm_drop);
        MAP("snd_pcm_resume", shared_snd_pcm_resume);
        MAP("snd_pcm_drain", shared_snd_pcm_drain);
        MAP("snd_pcm_delay", shared_snd_pcm_delay);
        MAP("snd_pcm_writei", shared_snd_pcm_writei);
        MAP("snd_pcm_avail_update", shared_snd_pcm_avail_update);
        MAP("snd_pcm_state", shared_snd_pcm_state);
        MAP("snd_pcm_format_width", shared_snd_pcm_format_width);
        MAP("snd_strerror", shared_snd_strerror);
    }
} // namespace Alsa2SdlBridge

#endif