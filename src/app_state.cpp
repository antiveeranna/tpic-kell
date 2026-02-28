#include "app_state.h"

#include <cstring>

static byte keyToSegments(char key) {
  if (key >= '0' && key <= '9') {
    return segmentMap[key - '0'];
  }
  switch (key) {
    case 'A':
      return SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
    case 'B':
      return SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
    case 'C':
      return SEG_A | SEG_D | SEG_E | SEG_F;
    case 'D':
      return SEG_B | SEG_C | SEG_D | SEG_E | SEG_G;
    case '*':
      return SEG_G | SEG_DP;
    case '#':
      return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
    default:
      return 0;
  }
}

static void buildTimeSegments(int minutes, int seconds, bool colonOn, bool blankLead, byte out[kDigits]) {
  int mT = minutes / 10;
  int mO = minutes % 10;
  int sT = seconds / 10;
  int sO = seconds % 10;

  out[0] = blankLead && mT == 0 ? 0 : segmentMap[mT];
  out[1] = segmentMap[mO];
  out[2] = segmentMap[sT];
  out[3] = segmentMap[sO];
  if (colonOn) {
    out[1] |= SEG_DP;
    out[2] |= SEG_DP;
  }
}

void buildIdleSegments(const char *digits, int len, byte out[kDigits]) {
  for (int i = 0; i < kDigits; i++) {
    out[i] = SEG_D;
  }
  int offset = 2 - len;
  for (int i = 0; i < len && i < kDigits; i++) {
    out[offset + i] = keyToSegments(digits[i]);
  }
}

static void appendKeyLog(AppState &s, char key) {
  size_t len = strlen(s.keyLog);
  if (len >= sizeof(s.keyLog) - 1) {
    memmove(s.keyLog, s.keyLog + 1, sizeof(s.keyLog) - 1);
    len = sizeof(s.keyLog) - 2;
  }
  s.keyLog[len] = key;
  s.keyLog[len + 1] = '\0';
  s.oledDirty = true;
}

void initState(AppState &s) {
  s.mode = MODE_IDLE;
  for (int i = 0; i < kDigits; i++) s.segs[i] = SEG_D;
  s.keyLog[0] = '\0';
  s.lastOled = 0;
  s.lastTick = 0;
  s.lastPhase = 0;
  s.countdownMin = 0;
  s.countdownSec = 0;
  s.preStep = 3;
  s.postFlashSec = 0;
  s.flashOn = true;
  s.digitBuf[0] = 0;
  s.digitBuf[1] = 0;
  s.digitBuf[2] = 0;
  s.digitLen = 0;
  s.colonOn = false;
  s.prePos = 0;
  s.paused = false;
  s.countingUp = false;
  s.targetMin = 0;
  s.lastKey = 0;
  s.segsDirty = true;
  s.oledDirty = true;
}

void updateMode(AppState &s, unsigned long now) {
  if (s.paused) return;
  if (s.mode == MODE_PRECOUNTDOWN) {
    const unsigned long stepMs = 1000;
    if (now - s.lastPhase >= stepMs) {
      if (s.prePos < kDigits) {
        for (int i = 0; i < kDigits; i++) s.segs[i] = 0;
        s.segs[s.prePos] = segmentMap[3 - s.prePos];
        s.segsDirty = true;
        s.prePos++;
        s.lastPhase = now;
      } else {
        s.lastTick = now;
        s.colonOn = false;
        s.mode = s.countingUp ? MODE_COUNTUP : MODE_COUNTDOWN;
      }
    }
  } else if (s.mode == MODE_COUNTDOWN) {
    if (now - s.lastTick >= 1000) {
      s.lastTick += 1000;
      s.colonOn = !s.colonOn;
      if (s.countdownSec == 0) {
        if (s.countdownMin > 0) {
          s.countdownMin--;
          s.countdownSec = 59;
        }
      } else {
        s.countdownSec--;
      }
    }
    buildTimeSegments(s.countdownMin, s.countdownSec, s.colonOn, true, s.segs);
    s.segsDirty = true;

    int totalSec = s.countdownMin * 60 + s.countdownSec;
    if (totalSec <= 10 && s.mode != MODE_FLASH_ZERO) {
      s.mode = MODE_FLASH_ZERO;
      s.postFlashSec = 10;
      s.flashOn = true;
      s.lastPhase = now;
    }
  } else if (s.mode == MODE_COUNTUP) {
    if (now - s.lastTick >= 1000) {
      s.lastTick += 1000;
      s.colonOn = !s.colonOn;
      s.countdownSec++;
      if (s.countdownSec >= 60) {
        s.countdownSec = 0;
        s.countdownMin++;
      }
    }
    buildTimeSegments(s.countdownMin, s.countdownSec, s.colonOn, true, s.segs);
    s.segsDirty = true;

    if (s.countdownMin >= s.targetMin && s.countdownSec == 0 && s.countdownMin > 0) {
      s.mode = MODE_FLASH_ZERO;
      s.postFlashSec = 10;
      s.flashOn = true;
      s.lastPhase = now;
    }
  } else if (s.mode == MODE_FLASH_ZERO) {
    if (now - s.lastPhase >= 250) {
      s.lastPhase = now;
      s.flashOn = !s.flashOn;
      if (s.flashOn) {
        buildTimeSegments(s.countdownMin, s.countdownSec, s.colonOn, true, s.segs);
      } else {
        for (int i = 0; i < kDigits; i++) s.segs[i] = 0;
      }
      s.segsDirty = true;
    }

    if (now - s.lastTick >= 1000) {
      s.lastTick += 1000;
      s.colonOn = !s.colonOn;
      if (!s.countingUp) {
        if (s.countdownMin > 0 || s.countdownSec > 0) {
          if (s.countdownSec == 0) {
            s.countdownMin--;
            s.countdownSec = 59;
          } else {
            s.countdownSec--;
          }
        }
      }
      if (s.postFlashSec > 0) s.postFlashSec--;
    }

    if (s.postFlashSec == 0) {
      s.mode = MODE_IDLE;
      buildIdleSegments(s.digitBuf, s.digitLen, s.segs);
      s.segsDirty = true;
    }
  }
}

void handleKey(AppState &s, char key, unsigned long now) {
  if (key == 0) return;
  appendKeyLog(s, key);

#if KEY_TEST_MODE
  for (int i = 0; i < kDigits; i++) s.segs[i] = 0;
  s.segs[kDigits - 1] = keyToSegments(key);
  s.digitBuf[0] = key;
  s.digitBuf[1] = 0;
  s.digitBuf[2] = 0;
  s.digitLen = 1;
  s.segsDirty = true;
  return;
#endif

  if (s.mode == MODE_COUNTDOWN || s.mode == MODE_COUNTUP || s.mode == MODE_FLASH_ZERO) {
    if (key == '*' && s.lastKey == '*') {
      s.mode = MODE_IDLE;
      s.paused = false;
      s.digitLen = 0;
      buildIdleSegments(s.digitBuf, s.digitLen, s.segs);
      s.segsDirty = true;
    } else {
      s.paused = !s.paused;
      if (!s.paused) {
        s.lastTick = now;
        s.lastPhase = now;
      }
    }
    s.lastKey = key;
    return;
  }

  if (s.mode == MODE_IDLE) {
    if (key >= '0' && key <= '9') {
      if (s.digitLen < 2) {
        s.digitBuf[s.digitLen++] = key;
      } else {
        s.digitBuf[0] = s.digitBuf[1];
        s.digitBuf[1] = key;
      }
    } else if (key == 'A' && s.digitLen >= 1) {
      if (s.digitLen == 1) {
        s.countdownMin = s.digitBuf[0] - '0';
      } else {
        s.countdownMin = (s.digitBuf[0] - '0') * 10 + (s.digitBuf[1] - '0');
      }
      s.countdownSec = 0;
      s.countingUp = false;
      s.paused = false;
      s.prePos = 0;
      s.lastPhase = now - (1000 / kDigits);
      s.mode = MODE_PRECOUNTDOWN;
      s.postFlashSec = 0;
      s.digitLen = 0;
      s.keyLog[0] = '\0';
    } else if (key == 'B' && s.digitLen >= 1) {
      if (s.digitLen == 1) {
        s.targetMin = s.digitBuf[0] - '0';
      } else {
        s.targetMin = (s.digitBuf[0] - '0') * 10 + (s.digitBuf[1] - '0');
      }
      s.countdownMin = 0;
      s.countdownSec = 0;
      s.countingUp = true;
      s.paused = false;
      s.prePos = 0;
      s.lastPhase = now - (1000 / kDigits);
      s.mode = MODE_PRECOUNTDOWN;
      s.postFlashSec = 0;
      s.digitLen = 0;
      s.keyLog[0] = '\0';
    } else {
      s.digitLen = 0;
    }
  }
}
