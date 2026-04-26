#include "app_state.h"
#include <string.h>

void app_state_init(app_state_t *s) {
    memset(s, 0, sizeof(*s));
    s->mode       = MODE_IDLE;
    s->flashOn    = true;
    s->segsDirty  = true;
    s->targetDuty = DUTY_NORMAL_VAL;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void clearSegs(app_state_t *s) {
    memset(s->segs, 0, sizeof(s->segs));
    s->segsDirty = true;
}

static void tickTime(app_state_t *s, int delta) {
    s->totalSeconds += delta;
    if (s->totalSeconds < 0) s->totalSeconds = 0;
}

static void buildTimeSegments(int totalSec, bool colonOn,
                              bool blankLead, uint8_t out[kDigits]) {
    int minTens = totalSec / 600;
    int minOnes = (totalSec / 60) % 10;
    int secTens = (totalSec % 60) / 10;
    int secOnes = totalSec % 10;

    out[0] = blankLead && minTens == 0 ? 0 : segmentMap[minTens];
    out[1] = segmentMap[minOnes];
    out[2] = segmentMap[secTens];
    out[3] = segmentMap[secOnes];
    if (colonOn) {
        out[1] |= SEG_DP;
        out[2] |= SEG_DP;
    }
}

static void updateSegsFromTime(app_state_t *s) {
    buildTimeSegments(s->totalSeconds, s->colonOn, true, s->segs);
    s->segsDirty = true;
}

static int parseBuf(const char *buf, int len) {
    if (len == 1) return buf[0] - '0';
    if (len == 2) return (buf[0] - '0') * 10 + (buf[1] - '0');
    return 0;
}

static int parseEntrySec(const app_state_t *s) {
    return parseBuf(s->digitBuf, s->digitLen) * 60 +
           parseBuf(s->secBuf,   s->secLen);
}

static void startTimerWithSec(app_state_t *s, bool up, int total, uint32_t now) {
    s->totalSeconds = up ? 0 : total;
    s->targetSec    = up ? total : 0;
    s->countingUp   = up;
    s->paused       = false;
    s->prePos       = 0;
    s->lastPhase    = now - 1000;
    s->mode         = MODE_PRECOUNTDOWN;
    s->digitLen     = 0;
    s->secLen       = 0;
    s->enteringSeconds = false;
    s->overrun      = false;
    s->lastEntrySec = total;
}

static void startTimer(app_state_t *s, bool up, uint32_t now) {
    startTimerWithSec(s, up, parseEntrySec(s), now);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void updateMode(app_state_t *s, uint32_t now) {
    if (s->paused) return;

    s->targetDuty = DUTY_NORMAL_VAL;

    switch (s->mode) {

    case MODE_PRECOUNTDOWN: {
        if (now - s->lastPhase < 1000) break;
        if (s->prePos < kDigits) {
            clearSegs(s);
            s->segs[s->prePos] = segmentMap[3 - s->prePos];
            s->segsDirty = true;
            s->prePos++;
            s->lastPhase = now;
        } else {
            s->lastTick = now;
            s->colonOn  = true;
            s->mode     = s->countingUp ? MODE_COUNTUP : MODE_COUNTDOWN;
            buildTimeSegments(s->totalSeconds, s->colonOn, true, s->segs);
            s->segsDirty = true;
        }
        break;
    }

    case MODE_COUNTDOWN: {
        bool tick = (now - s->lastTick) >= 1000;
        if (tick && !s->overrun) {
            s->lastTick += 1000;
            s->colonOn = !s->colonOn;
            tickTime(s, -1);
        } else if (tick) {
            s->lastTick += 1000;
        }
        if (!s->overrun && s->totalSeconds <= 0) {
            s->totalSeconds = 0;
            s->overrun   = true;
            s->overrunAt = now;
            s->flashOn   = true;
        }
        bool alerting = s->overrun && (now - s->overrunAt) < 2000;
        if (alerting) {
            bool show = (((now - s->overrunAt) / 250) % 2) == 0;
            if (show != s->flashOn || tick) {
                s->flashOn = show;
                if (show) {
                    buildTimeSegments(s->totalSeconds, s->colonOn, true, s->segs);
                    s->segs[3] |= SEG_DP;
                } else {
                    memset(s->segs, 0, sizeof(s->segs));
                }
                s->segsDirty = true;
            }
        } else if (tick || (s->overrun && !s->flashOn)) {
            buildTimeSegments(s->totalSeconds, s->colonOn, true, s->segs);
            if (s->overrun) {
                s->segs[3] |= SEG_DP;
            } else if (s->totalSeconds > 0 && s->totalSeconds <= 10) {
                s->segs[3] |= SEG_DP;
            } else if (s->totalSeconds <= 30 && s->colonOn) {
                s->segs[3] |= SEG_DP;
            }
            s->flashOn = true;
            s->segsDirty = true;
        }
        if (s->overrun && !alerting) {
            uint32_t phase = (now - s->overrunAt - 2000u) % 3000u;
            uint32_t t1024 = (phase < 1500u)
                ? (phase * 1024u / 1500u)
                : ((3000u - phase) * 1024u / 1500u);
            int diff = (int)DUTY_DIMMED_VAL - (int)DUTY_NORMAL_VAL;
            s->targetDuty = (uint8_t)((int)DUTY_NORMAL_VAL +
                                      diff * (int)t1024 / 1024);
        }
        break;
    }

    case MODE_COUNTUP: {
        bool tick = (now - s->lastTick) >= 1000;
        if (tick) {
            s->lastTick += 1000;
            s->colonOn = !s->colonOn;
            tickTime(s, +1);
        }
        if (!s->overrun && s->targetSec > 0 &&
            s->totalSeconds >= s->targetSec) {
            s->overrun   = true;
            s->overrunAt = now;
            s->flashOn   = true;
        }
        bool alerting = s->overrun && (now - s->overrunAt) < 2000;
        if (alerting) {
            bool show = (((now - s->overrunAt) / 250) % 2) == 0;
            if (show != s->flashOn || tick) {
                s->flashOn = show;
                if (show) {
                    buildTimeSegments(s->totalSeconds, s->colonOn, true, s->segs);
                    s->segs[3] |= SEG_DP;
                } else {
                    memset(s->segs, 0, sizeof(s->segs));
                }
                s->segsDirty = true;
            }
        } else if (tick || (s->overrun && !s->flashOn)) {
            buildTimeSegments(s->totalSeconds, s->colonOn, true, s->segs);
            if (s->overrun) {
                s->segs[3] |= SEG_DP;
            } else if (s->targetSec > 0) {
                int remain = s->targetSec - s->totalSeconds;
                if (remain > 0 && remain <= 10) {
                    s->segs[3] |= SEG_DP;
                } else if (remain <= 30 && s->colonOn) {
                    s->segs[3] |= SEG_DP;
                }
            }
            s->flashOn = true;
            s->segsDirty = true;
        }
        if (s->overrun && !alerting) {
            uint32_t phase = (now - s->overrunAt - 2000u) % 3000u;
            uint32_t t1024 = (phase < 1500u)
                ? (phase * 1024u / 1500u)
                : ((3000u - phase) * 1024u / 1500u);
            int diff = (int)DUTY_DIMMED_VAL - (int)DUTY_NORMAL_VAL;
            s->targetDuty = (uint8_t)((int)DUTY_NORMAL_VAL +
                                      diff * (int)t1024 / 1024);
        }
        break;
    }

    case MODE_IDLE: {
        if (s->segsDirty) s->blinkBase = now;
        uint32_t idleElapsed = now - s->lastActivityTime;
        bool hasInput = s->digitLen > 0 || s->secLen > 0 || s->enteringSeconds;
        bool sleeping = !hasInput && idleElapsed >= IDLE_SLEEP_MS;
        bool ghost = !hasInput && !sleeping && s->lastEntrySec > 0;
        bool quiet = !hasInput && !ghost && idleElapsed >= IDLE_DIM_MS;

        if (sleeping) {
            uint32_t phase = (idleElapsed - IDLE_SLEEP_MS) % 4000u;
            uint32_t t1024 = (phase < 2000u)
                ? (phase * 1024u / 2000u)
                : ((4000u - phase) * 1024u / 2000u);
            int diff = (int)DUTY_FAINT_VAL - 255;
            s->targetDuty = (uint8_t)(255 + diff * (int)t1024 / 1024);
            if (s->lastBlink || s->segsDirty) {
                s->lastBlink = false;
                memset(s->segs, 0, sizeof(s->segs));
                s->segs[1] = SEG_D;
                s->segsDirty = true;
            }
        } else if (ghost) {
            s->targetDuty = DUTY_DIMMED_VAL;
            if (s->lastBlink || s->segsDirty) {
                s->lastBlink = false;
                buildTimeSegments(s->lastEntrySec, true, true, s->segs);
                s->segsDirty = true;
            }
        } else if (quiet) {
            s->targetDuty = DUTY_DIMMED_VAL;
            if (s->lastBlink || s->segsDirty) {
                s->lastBlink = false;
                memset(s->segs, 0, sizeof(s->segs));
                s->segs[1] = SEG_D;
                s->segsDirty = true;
            }
        } else {
            bool blinkOn = ((now - s->blinkBase) / 500) % 2 == 0;
            if (blinkOn != s->lastBlink || s->segsDirty) {
                s->lastBlink = blinkOn;
                memset(s->segs, 0, sizeof(s->segs));
                if (!hasInput) {
                    if (blinkOn) s->segs[1] = SEG_D;
                } else if (blinkOn) {
                    int offset = 2 - s->digitLen;
                    for (int i = 0; i < s->digitLen; i++) {
                        s->segs[offset + i] = segmentMap[s->digitBuf[i] - '0'];
                    }
                    for (int i = 0; i < s->secLen; i++) {
                        s->segs[2 + i] = segmentMap[s->secBuf[i] - '0'];
                    }
                    if (s->enteringSeconds) {
                        s->segs[1] |= SEG_DP;
                        s->segs[2] |= SEG_DP;
                    }
                }
                s->segsDirty = true;
            }
        }
        break;
    }

    default:
        break;
    }
}

void handleKey(app_state_t *s, char key, uint32_t now) {
    if (key == 0) return;
    s->lastActivityTime = now;
    s->segsDirty = true;

    switch (s->mode) {

    case MODE_PRECOUNTDOWN:
        s->mode     = MODE_IDLE;
        s->digitLen = 0;
        s->secLen   = 0;
        s->enteringSeconds = false;
        clearSegs(s);
        s->lastKey = key;
        return;

    case MODE_COUNTDOWN:
    case MODE_COUNTUP:
        if (key == '*' && s->lastKey == '*') {
            s->mode     = MODE_IDLE;
            s->paused   = false;
            s->digitLen = 0;
            s->secLen   = 0;
            s->enteringSeconds = false;
            s->overrun  = false;
            clearSegs(s);
        } else {
            s->paused = !s->paused;
            if (!s->paused) {
                s->lastTick  = now;
                s->lastPhase = now;
            }
        }
        s->lastKey = key;
        return;

    case MODE_IDLE:
        if (key >= '0' && key <= '9') {
            char *buf = s->enteringSeconds ? s->secBuf  : s->digitBuf;
            int  *len = s->enteringSeconds ? &s->secLen : &s->digitLen;
            if (*len < 2) {
                buf[(*len)++] = key;
            } else {
                buf[0] = buf[1];
                buf[1] = key;
            }
            s->segsDirty = true;
        } else if (key == '#') {
            s->enteringSeconds = true;
            s->segsDirty = true;
        } else if ((key == 'A' || key == 'B') &&
                   (s->digitLen >= 1 || s->secLen >= 1)) {
            startTimer(s, key == 'B', now);
        } else if ((key == 'A' || key == 'B') &&
                   !s->enteringSeconds && s->lastEntrySec > 0) {
            startTimerWithSec(s, key == 'B', s->lastEntrySec, now);
        } else {
            s->digitLen  = 0;
            s->secLen    = 0;
            s->enteringSeconds = false;
            s->lastEntrySec = 0;
            s->segsDirty = true;
        }
        break;
    }
}
