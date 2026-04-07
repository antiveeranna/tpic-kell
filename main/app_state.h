#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "segment_defs.h"

typedef enum {
    MODE_IDLE,
    MODE_PRECOUNTDOWN,
    MODE_COUNTDOWN,
    MODE_COUNTUP,
    MODE_FLASH_ZERO
} mode_t;

typedef struct {
    // Core state
    mode_t mode;

    // Timing
    int      totalSeconds;
    int      targetMin;
    bool     countingUp;
    bool     paused;
    bool     colonOn;
    bool     flashOn;
    int      postFlashSec;
    int      prePos;
    uint32_t lastTick;
    uint32_t lastPhase;

    // Display
    uint8_t  segs[kDigits];
    bool     segsDirty;
    uint32_t blinkBase;
    bool     lastBlink;

    // Input
    char     digitBuf[3];
    int      digitLen;
    char     lastKey;
} app_state_t;

void app_state_init(app_state_t *s);
void updateMode(app_state_t *s, uint32_t now);
void handleKey(app_state_t *s, char key, uint32_t now);
