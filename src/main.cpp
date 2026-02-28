#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "app_state.h"
#include "segment_defs.h"
#include "keypad.h"

// PIN MAPPING
#define TPIC_LATCH  1
#define TPIC_CLOCK  0
#define TPIC_G      4
#define TPIC_DATA   3
#define KEYPAD_INT  7

// OLED SETTINGS
#define OFFSET_X 28
#define OFFSET_Y 24
Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool gOledOk = false;
static AppState gState;
static Keypad keypad;

void showSegments(const byte segs[kDigits]);

// Brightness duty cycles for /G (active low: higher = dimmer)
const int kDutyNormal = 255 - 204; // ~80%
const int kDutyDimmed = 255 - 25;  // ~10%

// Digit at position 2 is physically mounted upside-down on the PCB.
// Swap top/bottom segment pairs (A<->D, B<->E, C<->F) to compensate.
const byte kFlipMask = 0b0100;

byte rotate180(byte v) {
  byte out = 0;
  if (v & SEG_A) out |= SEG_D;
  if (v & SEG_B) out |= SEG_E;
  if (v & SEG_C) out |= SEG_F;
  if (v & SEG_D) out |= SEG_A;
  if (v & SEG_E) out |= SEG_B;
  if (v & SEG_F) out |= SEG_C;
  if (v & SEG_G) out |= SEG_G;
  if (v & SEG_DP) out |= SEG_DP;
  return out;
}


void showSegments(const byte segs[kDigits]) {
  digitalWrite(TPIC_LATCH, LOW);
  for (int pos = kDigits - 1; pos >= 0; pos--) {
    byte out = segs[pos];
    if (kFlipMask & (1 << pos)) {
      out = rotate180(out);
    }
    shiftOut(TPIC_DATA, TPIC_CLOCK, MSBFIRST, out);
  }
  digitalWrite(TPIC_LATCH, HIGH);
}

void playSnakeAnimation() {
  const byte snakeSegs[] = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F};
  const int snakeLen = sizeof(snakeSegs) / sizeof(snakeSegs[0]);
  const int frames = 24;
  byte segs[kDigits];
  for (int f = 0; f < frames; f++) {
    for (int i = 0; i < kDigits; i++) {
      int idx = (f + i * 2) % snakeLen;
      segs[i] = snakeSegs[idx];
    }
    showSegments(segs);
    delay(60);
  }
}

void renderOled(const AppState &s) {
  if (!gOledOk) return;
  display.clearDisplay();
  display.setTextSize(1);

  const char *status;
  if (s.paused) {
    status = "Paused";
  } else {
    switch (s.mode) {
      case MODE_IDLE:         status = s.digitLen > 0 ? "Entry" : "Idle"; break;
      case MODE_PRECOUNTDOWN: status = "Starting"; break;
      case MODE_COUNTDOWN:    status = "Count DN"; break;
      case MODE_COUNTUP:      status = "Count UP"; break;
      case MODE_FLASH_ZERO:   status = "Done!"; break;
      default:                status = ""; break;
    }
  }
  display.setCursor(OFFSET_X + 4, OFFSET_Y + 4);
  display.print(status);

  if (s.mode == MODE_COUNTDOWN || s.mode == MODE_COUNTUP || s.mode == MODE_FLASH_ZERO) {
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", s.displayMin(), s.displaySec());
    display.setCursor(OFFSET_X + 4, OFFSET_Y + 16);
    display.print(timeBuf);
  }

  display.setCursor(OFFSET_X + 4, OFFSET_Y + 28);
  display.print(s.keyLog);

  display.display();
}

void setup() {
  // TPIC Setup
  pinMode(TPIC_DATA, OUTPUT);
  pinMode(TPIC_CLOCK, OUTPUT);
  pinMode(TPIC_LATCH, OUTPUT);
  pinMode(TPIC_G, OUTPUT);
  pinMode(KEYPAD_INT, INPUT_PULLUP);

  // 80% brightness via PWM on /G (active low).
  const int kPwmFreq = 5000;
  const int kPwmResolution = 8;
  ledcAttach(TPIC_G, kPwmFreq, kPwmResolution);
  ledcWrite(TPIC_G, kDutyNormal);

  // OLED Setup
  Wire.begin(5, 6);
  for (int attempt = 0; attempt < 5 && !gOledOk; attempt++) {
    gOledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    if (!gOledOk) delay(50);
  }
  if (gOledOk) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
  }

  keypad.begin(KEYPAD_INT);
  gState = AppState{};
  playSnakeAnimation();
}

void loop() {
  AppState &s = gState;
  unsigned long now = millis();

  char key = keypad.poll();
  if (key) handleKey(s, key, now);

  updateMode(s, now);

  ledcWrite(TPIC_G, s.paused ? kDutyDimmed : kDutyNormal);

  if (s.segsDirty) {
    showSegments(s.segs);
    s.segsDirty = false;
  }

  if (s.oledDirty || (now - s.lastOled) >= 200) {
    renderOled(s);
    s.lastOled = now;
    s.oledDirty = false;
  }
}
