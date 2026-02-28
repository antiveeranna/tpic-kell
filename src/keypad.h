#pragma once

#include <Arduino.h>
#include <Wire.h>

struct Keypad {
  void begin(uint8_t intPin);
  char poll();   // returns debounced key, or 0

private:
  static constexpr uint8_t kPcfAddr = 0x20;
  static constexpr uint8_t kRowMask = 0x0F;
  static constexpr unsigned long kDebounceMs = 25;

  uint8_t _intPin = 0;
  char _lastStable = 0;
  char _lastRead = 0;
  unsigned long _lastChange = 0;

  void writePcf(uint8_t value);
  uint8_t readPcf();
  void arm();
  char scanOnce();
};
