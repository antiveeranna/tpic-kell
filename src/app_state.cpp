#include "app_state.h"

#include <cstring>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void clearSegs(AppState &s) {
  memset(s.segs, 0, sizeof(s.segs));
  s.segsDirty = true;
}

static void tickTime(AppState &s, int delta) {
  s.totalSeconds += delta;
  if (s.totalSeconds < 0) s.totalSeconds = 0;
}

static byte keyToSegments(char key) {
  if (key >= '0' && key <= '9') {
    return segmentMap[key - '0'];
  }
  switch (key) {
    case 'A': return SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
    case 'B': return SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
    case 'C': return SEG_A | SEG_D | SEG_E | SEG_F;
    case 'D': return SEG_B | SEG_C | SEG_D | SEG_E | SEG_G;
    case '*': return SEG_G | SEG_DP;
    case '#': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
    default:  return 0;
  }
}

static void buildTimeSegments(int minutes, int seconds, bool colonOn,
                               bool blankLead, byte out[kDigits]) {
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

static void updateSegsFromTime(AppState &s) {
  buildTimeSegments(s.displayMin(), s.displaySec(), s.colonOn, true, s.segs);
  s.segsDirty = true;
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

static int parseDigits(const AppState &s) {
  if (s.digitLen == 1) return s.digitBuf[0] - '0';
  return (s.digitBuf[0] - '0') * 10 + (s.digitBuf[1] - '0');
}

static void startTimer(AppState &s, bool up, unsigned long now) {
  int mins = parseDigits(s);
  s.totalSeconds = up ? 0 : mins * 60;
  s.targetMin = up ? mins : 0;
  s.countingUp = up;
  s.paused = false;
  s.prePos = 0;
  s.lastPhase = now - 1000;
  s.mode = MODE_PRECOUNTDOWN;
  s.postFlashSec = 0;
  s.digitLen = 0;
  s.keyLog[0] = '\0';
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void initState(AppState &s) {
  s = AppState{};
}

void updateMode(AppState &s, unsigned long now) {
  if (s.paused) return;

  switch (s.mode) {

  case MODE_PRECOUNTDOWN: {
    if (now - s.lastPhase < 1000) break;
    if (s.prePos < kDigits) {
      clearSegs(s);
      s.segs[s.prePos] = segmentMap[3 - s.prePos];
      s.segsDirty = true;
      s.prePos++;
      s.lastPhase = now;
    } else {
      s.lastTick = now;
      s.colonOn = false;
      s.mode = s.countingUp ? MODE_COUNTUP : MODE_COUNTDOWN;
    }
    break;
  }

  case MODE_COUNTDOWN: {
    if (now - s.lastTick >= 1000) {
      s.lastTick += 1000;
      s.colonOn = !s.colonOn;
      tickTime(s, -1);
      updateSegsFromTime(s);
    }
    if (s.totalSeconds <= 10) {
      s.mode = MODE_FLASH_ZERO;
      s.postFlashSec = 10;
      s.flashOn = true;
      s.lastPhase = now;
    }
    break;
  }

  case MODE_COUNTUP: {
    if (now - s.lastTick >= 1000) {
      s.lastTick += 1000;
      s.colonOn = !s.colonOn;
      tickTime(s, +1);
      updateSegsFromTime(s);
    }
    if (s.targetMin > 0 && s.totalSeconds >= s.targetMin * 60) {
      s.mode = MODE_FLASH_ZERO;
      s.postFlashSec = 10;
      s.flashOn = true;
      s.lastPhase = now;
    }
    break;
  }

  case MODE_FLASH_ZERO: {
    if (now - s.lastPhase >= 250) {
      s.lastPhase = now;
      s.flashOn = !s.flashOn;
      if (s.flashOn) {
        updateSegsFromTime(s);
      } else {
        clearSegs(s);
      }
    }
    if (now - s.lastTick >= 1000) {
      s.lastTick += 1000;
      s.colonOn = !s.colonOn;
      if (!s.countingUp) {
        tickTime(s, -1);
      }
      if (s.postFlashSec > 0) s.postFlashSec--;
    }
    if (s.postFlashSec == 0) {
      s.mode = MODE_IDLE;
      clearSegs(s);
    }
    break;
  }

  case MODE_IDLE:
  default:
    break;
  }
}

void handleKey(AppState &s, char key, unsigned long now) {
  if (key == 0) return;
  appendKeyLog(s, key);

  switch (s.mode) {

  case MODE_PRECOUNTDOWN:
    s.mode = MODE_IDLE;
    s.digitLen = 0;
    clearSegs(s);
    s.lastKey = key;
    return;

  case MODE_COUNTDOWN:
  case MODE_COUNTUP:
  case MODE_FLASH_ZERO:
    if (key == '*' && s.lastKey == '*') {
      s.mode = MODE_IDLE;
      s.paused = false;
      s.digitLen = 0;
      clearSegs(s);
    } else {
      s.paused = !s.paused;
      if (!s.paused) {
        s.lastTick = now;
        s.lastPhase = now;
      }
    }
    s.lastKey = key;
    return;

  case MODE_IDLE:
    if (key >= '0' && key <= '9') {
      if (s.digitLen < 2) {
        s.digitBuf[s.digitLen++] = key;
      } else {
        s.digitBuf[0] = s.digitBuf[1];
        s.digitBuf[1] = key;
      }
      s.segsDirty = true;
    } else if (key == 'A' && s.digitLen >= 1) {
      startTimer(s, false, now);
    } else if (key == 'B' && s.digitLen >= 1) {
      startTimer(s, true, now);
    } else {
      s.digitLen = 0;
      s.segsDirty = true;
    }
    break;
  }
}
