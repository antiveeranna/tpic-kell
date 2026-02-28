#pragma once

#include <Arduino.h>
#include "segment_defs.h"

enum Mode : uint8_t {
  MODE_IDLE,
  MODE_PRECOUNTDOWN,
  MODE_COUNTDOWN,
  MODE_COUNTUP,
  MODE_FLASH_ZERO
};

struct AppState {
  // --- Core state ---
  Mode mode = MODE_IDLE;

  // --- Timing ---
  int  totalSeconds  = 0;
  int  targetMin     = 0;
  bool countingUp    = false;
  bool paused        = false;
  bool colonOn       = false;
  bool flashOn       = true;
  int  postFlashSec  = 0;
  int  prePos        = 0;
  unsigned long lastTick  = 0;
  unsigned long lastPhase = 0;

  // --- Display ---
  byte segs[kDigits] = {};
  bool segsDirty     = true;
  bool oledDirty     = true;
  unsigned long lastOled = 0;

  // --- Input ---
  char keyLog[9]     = {};
  char digitBuf[3]   = {};
  int  digitLen      = 0;
  char lastKey       = 0;

  // --- Derived accessors ---
  int displayMin() const { return totalSeconds / 60; }
  int displaySec() const { return totalSeconds % 60; }
};

void initState(AppState &s);
void updateMode(AppState &s, unsigned long now);
void handleKey(AppState &s, char key, unsigned long now);
