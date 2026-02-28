#pragma once

#include <Arduino.h>

constexpr int kDigits = 4;

// Q7..Q0 = F DP A B E D C G
constexpr byte SEG_F  = 1 << 7;
constexpr byte SEG_DP = 1 << 6;
constexpr byte SEG_A  = 1 << 5;
constexpr byte SEG_B  = 1 << 4;
constexpr byte SEG_E  = 1 << 3;
constexpr byte SEG_D  = 1 << 2;
constexpr byte SEG_C  = 1 << 1;
constexpr byte SEG_G  = 1 << 0;

extern const byte segmentMap[10];
