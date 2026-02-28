#include "keypad.h"

static const char KEYMAP[4][4] = {
  {'A','3','2','1'},
  {'B','6','5','4'},
  {'C','9','8','7'},
  {'D','#','0','*'}
};

static volatile bool sPending = false;

static void IRAM_ATTR keypadISR() {
  sPending = true;
}

void Keypad::begin(uint8_t intPin) {
  _intPin = intPin;
  pinMode(intPin, INPUT_PULLUP);
  writePcf(0xFF);
  attachInterrupt(digitalPinToInterrupt(intPin), keypadISR, FALLING);
  arm();
  readPcf();
}

void Keypad::writePcf(uint8_t value) {
  Wire.beginTransmission(kPcfAddr);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t Keypad::readPcf() {
  Wire.requestFrom(kPcfAddr, (uint8_t)1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0xFF;
}

void Keypad::arm() {
  writePcf(0x0F);
}

char Keypad::scanOnce() {
  for (int col = 0; col < 4; col++) {
    uint8_t out = 0xFF;
    out &= ~(1 << (4 + col));
    writePcf(out);
    delayMicroseconds(20);
    uint8_t in = readPcf();
    uint8_t rows = in & kRowMask;
    for (int row = 0; row < 4; row++) {
      if ((rows & (1 << row)) == 0) {
        return KEYMAP[row][col];
      }
    }
  }
  return 0;
}

char Keypad::poll() {
  if (!sPending) return 0;

  char key = scanOnce();
  unsigned long now = millis();

  if (key != _lastRead) {
    _lastRead = key;
    _lastChange = now;
  }

  bool debouncing = (now - _lastChange) < kDebounceMs;
  char result = 0;

  if (key != 0 && !debouncing && key != _lastStable) {
    _lastStable = key;
    result = key;
  }
  if (key == 0 && !debouncing) {
    _lastStable = 0;
  }

  arm();
  readPcf();

  if (key == 0 && !debouncing && digitalRead(_intPin) == HIGH) {
    sPending = false;
  }

  return result;
}
