#include <sys/time.h>
#include <unistd.h>
#include <glad/gl.h>
#include <SDL3/SDL.h>
#include "../log/log.h"
#include <string.h>
#include <stdio.h>

#include "../config/config.h"
#include "../graphics/fpsLimiter.h"
#include "../graphics/overlayMsgs.h"

static void statLog(const char *fmt, ...);

/* Per-frame CPU time of the loader's own work, for FRAME_STATS. */
long long gStatBlitUs, gStatSwapUs, gStatGameUs, gStatFrameEndUs;
static long long sBlitSum, sBlitMax, sSwapSum, sSwapMax, sGameSum, sGameMax, sGameMin = 1LL << 40;

FpsLimit fpsLimit;
double lastTime = 0.0;
int frameCount = 0;
double fps = 0.0;

static int gFpsSlot = -1;
static int gFpsPosition = 0;
static bool gFpsVisible = false;

/* Frame pacing.
   The upstream limiter slept with usleep(), which on Windows only wakes on
   the 1-15 ms scheduler tick, so frames came out unevenly. This one keeps an
   absolute schedule (next = previous deadline + period) on a microsecond
   clock, sleeps until just before the deadline and spins the rest. When
   vsync alone gives FPS_TARGET (display refresh a multiple of it) the
   limiter stands aside and the swap does the pacing. */
static bool gVsyncPaces, gWantVsyncPaces;
static int gInterval = -1;
static int gPresents = 1, gWantPresents = 1; /* times each game frame is presented */
static long long gRefreshUs, gLastShownUs;
static long long gNextDeadline;
static long long gPeriod;

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
static HANDLE gTimer;
static LARGE_INTEGER gQpcFreq;

static long long nowUs(void)
{
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (long long)(c.QuadPart / gQpcFreq.QuadPart) * 1000000LL +
           (long long)(c.QuadPart % gQpcFreq.QuadPart) * 1000000LL / gQpcFreq.QuadPart;
}

static void sleepUntil(long long deadline)
{
    long long left = deadline - nowUs();
    /* Coarse sleep to ~1 ms before the deadline, then spin. */
    if (left > 1500)
    {
        LARGE_INTEGER due;
        due.QuadPart = -(left - 1000) * 10; /* 100 ns units, relative */
        if (gTimer && SetWaitableTimer(gTimer, &due, 0, NULL, NULL, FALSE))
            WaitForSingleObject(gTimer, INFINITE);
        else
            Sleep((DWORD)((left - 1000) / 1000));
    }
    while (nowUs() < deadline)
        YieldProcessor();
}
#else
#include <time.h>
static long long nowUs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

static void sleepUntil(long long deadline)
{
    long long left = deadline - nowUs();
    if (left > 1500)
        usleep((useconds_t)(left - 1000));
    while (nowUs() < deadline)
        ;
}
#endif

void initFpsLimiter()
{
    /* Read overlay config: position from config, visibility from config */
    gFpsPosition = getConfig()->fpsOverlayPosition;
    if (gFpsPosition < 0) gFpsPosition = 0;
    if (gFpsPosition > 3) gFpsPosition = 3;

    gFpsVisible = (getConfig()->fpsOverlayEnabled != 0);

#ifdef _WIN32
    QueryPerformanceFrequency(&gQpcFreq);
    gTimer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!gTimer)
    {
        /* Older Windows: plain timer, with the scheduler tick at 1 ms. */
        gTimer = CreateWaitableTimerW(NULL, TRUE, NULL);
        timeBeginPeriod(1);
    }
#endif

    if (getConfig()->fpsLimiter == 1)
    {
        fpsLimit.targetFrameTime = 1000000 / getConfig()->fpsTarget;
        fpsLimit.frameEnd = clockNow();
        gPeriod = (long long)(1000000.0 / getConfig()->fpsTarget + 0.5);
    }
}

void setupVsync(float refreshHz)
{
    EmulatorConfig *cfg = getConfig();
    float target = cfg->fpsTarget > 0.0f ? cfg->fpsTarget : 60.0f;
    int interval;

    gVsyncPaces = false;
    if (cfg->vsync == 0)
        interval = 0;
    else if (cfg->vsync > 0)
        interval = cfg->vsync;
    else
    {
        /* AUTO: swap on every refresh. When the refresh rate is a multiple
           of the target (120 Hz for 60 FPS) each game frame is presented
           refresh/target times, so it stays on screen for exactly that many
           refreshes. (Swap intervals above 1 would do the same but NVIDIA
           ignores them in fullscreen.) */
        interval = 1;
        if (refreshHz > 0.0f)
        {
            int n = (int)(refreshHz / target + 0.5f);
            float off = refreshHz - n * target;
            if (n >= 1 && off > -1.0f && off < 1.0f)
            {
                gPresents = n;
                gVsyncPaces = true;
                gRefreshUs = (long long)(1000000.0 / refreshHz + 0.5);
            }
        }
    }
    if (!SDL_GL_SetSwapInterval(interval))
    {
        log_warn("VSync: could not set swap interval %d: %s\n", interval, SDL_GetError());
        gVsyncPaces = false;
    }
    gInterval = interval;
    gWantVsyncPaces = gVsyncPaces;
    gWantPresents = gPresents;
    statLog("VSync: display %.2f Hz, swap interval %d, %d present(s) per frame, %s\n", refreshHz, interval, gPresents,
             gVsyncPaces ? "vsync paces the game" : (cfg->fpsLimiter ? "frame limiter paces the game" : "unpaced"));
    fflush(stdout);
}

/* Called before every buffer swap. The swap interval belongs to the GL
   context and its window surface: the game creates its own context
   (glXCreateContext) and the window may go fullscreen after setupVsync(),
   both of which drop the interval. Re-apply it whenever either changes. */
void vsyncAfterPresents(void)
{
    if (gVsyncPaces)
    {
        glad_glFinish();
        gLastShownUs = nowUs();
    }
}

/* Extra presents of the current frame after its first swap, so each game
   frame stays on screen for gPresents refreshes (2 at 120 Hz for 60 FPS).
   Count the refreshes the frame has already taken since the previous one
   was shown and only add what is missing: a frame that was slow to render
   and already missed a refresh is not held for two more, which would drop
   a game that needs a little over one refresh per frame to 40 or 30 FPS. */
int vsyncExtraPresents(void)
{
    long long elapsed;
    int taken;

    if (!gVsyncPaces || gPresents <= 1)
        return 0;
    glad_glFinish(); /* wait until the first present is on screen */
    if (gLastShownUs == 0 || gRefreshUs <= 0)
        return gPresents - 1;
    elapsed = nowUs() - gLastShownUs;
    taken = (int)((elapsed + gRefreshUs / 2) / gRefreshUs);
    return taken >= gPresents ? 0 : gPresents - taken;
}

void vsyncBeforeSwap(void)
{
    static SDL_GLContext lastCtx;
    static SDL_WindowFlags lastFs;
    SDL_GLContext ctx;
    SDL_Window *w;
    SDL_WindowFlags fs;

    if (gInterval < 0)
        return;
    ctx = SDL_GL_GetCurrentContext();
    w = SDL_GL_GetCurrentWindow();
    fs = w ? (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) : 0;
    if (ctx == lastCtx && fs == lastFs)
        return;
    lastCtx = ctx;
    lastFs = fs;
    if (SDL_GL_SetSwapInterval(gInterval))
    {
        gVsyncPaces = gWantVsyncPaces;
        gPresents = gWantPresents;
        gNextDeadline = 0;
        statLog("VSync: swap interval %d applied (%s, context %p)\n", gInterval, fs ? "fullscreen" : "windowed", (void *)ctx);
    }
}

double getTimeInMilliseconds()
{
    return nowUs() / 1000.0;
}

double getTimeInSeconds()
{
    return nowUs() / 1000000.0;
}

/* Pacing measurements go to their own file (framestats.log next to the
   loader): the game takes over stdout once it starts. */
#include <stdarg.h>
static void statLog(const char *fmt, ...)
{
    FILE *f = fopen("framestats.log", "a");
    va_list ap;
    if (!f)
        return;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
}

/* Frame interval statistics, logged every 10 s with FRAME_STATS = true in
   linuxloader.ini ([System]); used to check pacing. */
static void frameStats(void)
{
    static long long last, window, minDt = 1LL << 40, maxDt, sum;
    static int n, over;
    long long now = nowUs(), dt;

    if (!getConfig()->frameStats)
        return;
    if (last == 0)
    {
        last = window = now;
        return;
    }
    dt = now - last;
    last = now;
    if (dt < minDt) minDt = dt;
    if (dt > maxDt) maxDt = dt;
    if (gPeriod > 0 && dt > gPeriod * 3 / 2) over++;
    sum += dt;
    n++;
    sBlitSum += gStatBlitUs;
    if (gStatBlitUs > sBlitMax)
        sBlitMax = gStatBlitUs;
    sGameSum += gStatGameUs;
    if (gStatGameUs > sGameMax)
        sGameMax = gStatGameUs;
    if (gStatGameUs < sGameMin)
        sGameMin = gStatGameUs;
    sSwapSum += gStatSwapUs;
    if (gStatSwapUs > sSwapMax)
        sSwapMax = gStatSwapUs;
    if (now - window >= 10000000)
    {
        statLog("Frames: %d in %.1f s, interval avg %.2f ms, min %.2f, max %.2f, %d late (>1.5 frames)\n", n,
                 (now - window) / 1e6, sum / 1000.0 / n, minDt / 1000.0, maxDt / 1000.0, over);
        statLog("        blit avg %.3f ms max %.3f, swap avg %.3f ms max %.3f\n", sBlitSum / 1000.0 / n, sBlitMax / 1000.0,
                sSwapSum / 1000.0 / n, sSwapMax / 1000.0);
        statLog("        game avg %.3f ms min %.3f max %.3f\n", sGameSum / 1000.0 / n, sGameMin / 1000.0, sGameMax / 1000.0);
        sGameSum = sGameMax = 0;
        sGameMin = 1LL << 40;
        sBlitSum = sBlitMax = sSwapSum = sSwapMax = 0; fflush(stdout); fflush(stderr);
        window = now;
        minDt = 1LL << 40;
        maxDt = sum = 0;
        n = over = 0;
    }
}

double calculateFps()
{
    double currentTime = getTimeInSeconds();
    frameStats();
    double deltaTime = currentTime - lastTime;
    frameCount++;
    if (deltaTime >= 1.0)
    {
        fps = frameCount / deltaTime;
        frameCount = 0;
        lastTime = currentTime;
        /* If the driver overrides the swap interval (vsync forced off in
           the control panel) vsync can't pace the game: limit instead.
           Swaps don't wait for vsync while the window is still hidden
           (startup) or minimised, so only judge a visible window, and only
           after three seconds in a row. */
        if (gVsyncPaces)
        {
            static int fastSeconds;
            SDL_Window *w = SDL_GL_GetCurrentWindow();
            bool visible = w && !(SDL_GetWindowFlags(w) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED));
            if (visible && fps > getConfig()->fpsTarget * 1.1)
                fastSeconds++;
            else
                fastSeconds = 0;
            if (fastSeconds >= 3)
            {
                statLog("VSync: running at %.1f FPS, vsync is not pacing; using the frame limiter\n", fps);
                gVsyncPaces = false;
                gPresents = 1;
                gNextDeadline = 0;
            }
        }
    }
    return fps;
}

long long fpsNowUs(void)
{
    return nowUs();
}

long clockNow()
{
    return (long)nowUs();
}

void frameTiming()
{
    long long now;

    if (gVsyncPaces || gPeriod <= 0)
        return;
    now = nowUs();
    /* First frame, or more than two frames late: restart the schedule
       instead of rushing to catch up. */
    if (gNextDeadline == 0 || now - gNextDeadline > 2 * gPeriod)
        gNextDeadline = now + gPeriod;
    else
    {
        sleepUntil(gNextDeadline);
        gNextDeadline += gPeriod;
    }
}

void fpsLimiter(FpsLimit *stats)
{
    (void)stats;
    frameTiming();
}

void fpsOverlayUpdateFps(double fps)
{
    char fpsStr[64];
    snprintf(fpsStr, sizeof(fpsStr), "FPS: %.2f", fps);

    if (gFpsSlot < 0)
    {
        int pos = gFpsPosition;
        if (pos < 0) pos = 0;
        if (pos > 3) pos = 3;
        gFpsSlot = overlayShowMessage(fpsStr, 0, (OverlayPosition)pos);

        if (!gFpsVisible && gFpsSlot >= 0)
            overlaySetMessageVisible(gFpsSlot, false);
    }
    else
    {
        overlayUpdateMessageText(gFpsSlot, fpsStr);
    }
}

void fpsOverlayToggleVisibility(void)
{
    if (gFpsSlot < 0)
    {
        gFpsVisible = !gFpsVisible;
        return;
    }
    gFpsVisible = !gFpsVisible;
    overlaySetMessageVisible(gFpsSlot, gFpsVisible);
}

bool fpsOverlayIsVisible(void)
{
    return gFpsVisible;
}

void fpsOverlaySetPosition(int pos)
{
    gFpsPosition = pos;
}

int fpsOverlayGetSlot(void)
{
    return gFpsSlot;
}