#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "segment_defs.h"

// Brightness duty cycles for /G (active low: higher value = dimmer)
#define DUTY_NORMAL_VAL (uint8_t)(255 - 204)  // ~80%
#define DUTY_DIMMED_VAL (uint8_t)(255 - 25)   // ~10%
#define DUTY_FAINT_VAL  (uint8_t)(255 - 5)    // ~2%

// Idle quieting: dim after IDLE_DIM_MS, sleep (faint dot) after IDLE_SLEEP_MS.
#define IDLE_DIM_MS    30000u
#define IDLE_SLEEP_MS  300000u

typedef enum {
    MODE_IDLE,
    MODE_PRECOUNTDOWN,
    MODE_COUNTDOWN,
    MODE_COUNTUP
} mode_t;

typedef struct {
    // Core state
    mode_t mode;

    // Timing
    int      totalSeconds;
    int      targetSec;
    bool     countingUp;
    bool     paused;
    bool     colonOn;
    bool     flashOn;
    int      prePos;
    uint32_t lastTick;
    uint32_t lastPhase;

    // Display
    uint8_t  segs[kDigits];
    bool     segsDirty;
    uint32_t blinkBase;
    bool     lastBlink;
    uint8_t  targetDuty;
    uint32_t lastActivityTime;
    bool     overrun;
    uint32_t overrunAt;

    // Input
    char     digitBuf[3];
    int      digitLen;
    char     secBuf[3];
    int      secLen;
    bool     enteringSeconds;
    int      lastEntrySec;
    char     lastKey;
} app_state_t;

void app_state_init(app_state_t *s);
void updateMode(app_state_t *s, uint32_t now);
void handleKey(app_state_t *s, char key, uint32_t now);
