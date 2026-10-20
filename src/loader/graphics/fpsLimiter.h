#pragma once

#include <stdbool.h>

typedef struct
{
    long targetFrameTime;
    long frameStart;
    long frameEnd;
    long sleepTime;
    long frameOverhead;
} FpsLimit;

void initFpsLimiter();
long clockNow();
void fpsLimiter(FpsLimit *stats);
double calculateFps();
void frameTiming();
void setupVsync(float refreshHz);
void vsyncBeforeSwap(void);
int vsyncExtraPresents(void);
void vsyncAfterPresents(void);
long long fpsNowUs(void);
extern long long gStatBlitUs, gStatSwapUs, gStatGameUs, gStatFrameEndUs;

void fpsOverlayUpdateFps(double fps);
void fpsOverlayToggleVisibility(void);
bool fpsOverlayIsVisible(void);
void fpsOverlaySetPosition(int pos);
int fpsOverlayGetSlot(void);
