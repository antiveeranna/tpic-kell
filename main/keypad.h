#pragma once

#include <stdint.h>
#include "driver/i2c_master.h"

typedef struct {
    i2c_master_dev_handle_t dev;
    uint8_t int_pin;
    char    last_stable;
    char    last_read;
    uint32_t last_change;
} keypad_t;

void keypad_init(keypad_t *kp, i2c_master_bus_handle_t bus, uint8_t int_pin);
char keypad_poll(keypad_t *kp);
