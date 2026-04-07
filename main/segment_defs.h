#pragma once

#include <stdint.h>

#define kDigits 4

// Q7..Q0 = F DP A B E D C G
#define SEG_F  (1 << 7)
#define SEG_DP (1 << 6)
#define SEG_A  (1 << 5)
#define SEG_B  (1 << 4)
#define SEG_E  (1 << 3)
#define SEG_D  (1 << 2)
#define SEG_C  (1 << 1)
#define SEG_G  (1 << 0)

extern const uint8_t segmentMap[10];
