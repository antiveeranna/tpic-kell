#pragma once

#include <Arduino.h>
#include "segment_defs.h"

enum Mode {
  MODE_IDLE,
  MODE_PRECOUNTDOWN,
  MODE_COUNTDOWN,
  MODE_COUNTUP,
  MODE_FLASH_ZERO
};

struct AppState {
  Mode mode;
  byte segs[kDigits];
  char keyLog[9];
  unsigned long lastOled;
  unsigned long lastTick;
  unsigned long lastPhase;
  int displayMin;
  int displaySec;
  int postFlashSec;
  bool flashOn;
  char digitBuf[3];
  int digitLen;
  bool colonOn;
  int prePos;
  bool paused;
  bool countingUp;
  int targetMin;
  char lastKey;
  bool segsDirty;
  bool oledDirty;
};

void initState(AppState &s);
void updateMode(AppState &s, unsigned long now);
void handleKey(AppState &s, char key, unsigned long now);

