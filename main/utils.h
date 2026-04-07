#pragma once

#include <stdint.h>
#include "esp_timer.h"

static inline uint32_t millis_now(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}
