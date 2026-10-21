#ifndef ALSA2SDL_H
#define ALSA2SDL_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct _snd_pcm snd_pcm_t;
    typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;
    typedef struct _snd_pcm_sw_params snd_pcm_sw_params_t;
    typedef long snd_pcm_sframes_t;
    typedef unsigned long snd_pcm_uframes_t;

    int shared_snd_pcm_open(snd_pcm_t **pcm, const char *name, int stream, int mode);
    int shared_snd_pcm_close(snd_pcm_t *pcm);

    int shared_snd_pcm_hw_params_malloc(snd_pcm_hw_params_t **ptr);
    void shared_snd_pcm_hw_params_free(snd_pcm_hw_params_t *obj);
    int shared_snd_pcm_hw_params_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
    int shared_snd_pcm_hw_params_set_access(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int access);
    int shared_snd_pcm_hw_params_set_format(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int val);
    int shared_snd_pcm_hw_params_set_channels(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int val);
    int shared_snd_pcm_hw_params_set_rate_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
    int shared_snd_pcm_hw_params_set_periods(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir);
    int shared_snd_pcm_hw_params_set_buffer_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val);
    int shared_snd_pcm_hw_params_set_buffer_size_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned long *val);
    int shared_snd_pcm_hw_params_set_period_size_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned long *val, int *dir);
    int shared_snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);

    int shared_snd_pcm_sw_params_malloc(snd_pcm_sw_params_t **ptr);
    void shared_snd_pcm_sw_params_free(snd_pcm_sw_params_t *obj);
    int shared_snd_pcm_sw_params_current(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
    int shared_snd_pcm_sw_params_set_avail_min(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
    int shared_snd_pcm_sw_params_set_start_threshold(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
    int shared_snd_pcm_sw_params_set_xfer_align(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
    int shared_snd_pcm_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);

    int shared_snd_pcm_prepare(snd_pcm_t *pcm);
    int shared_snd_pcm_start(snd_pcm_t *pcm);
    int shared_snd_pcm_drop(snd_pcm_t *pcm);
    int shared_snd_pcm_resume(snd_pcm_t *pcm);
    int shared_snd_pcm_drain(snd_pcm_t *pcm);
    int shared_snd_pcm_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp);
    snd_pcm_sframes_t shared_snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, unsigned long size);
    int shared_snd_pcm_avail_update(snd_pcm_t *pcm);
    int shared_snd_pcm_state(snd_pcm_t *pcm);
    int shared_snd_pcm_format_width(int format);
    const char *shared_snd_strerror(int errnum);

#ifdef __cplusplus
}
#endif

#endif // ALSA2SDL_H
