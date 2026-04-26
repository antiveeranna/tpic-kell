#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"

#include "esp_rom_sys.h"
#include "esp_check.h"

#include "app_state.h"
#include "segment_defs.h"
#include "keypad.h"
#include "utils.h"

// Pin mapping
#define TPIC_DATA   GPIO_NUM_3
#define TPIC_CLOCK  GPIO_NUM_0 
#define TPIC_LATCH  GPIO_NUM_1
#define TPIC_G      GPIO_NUM_4 
#define I2C_SCL     GPIO_NUM_6
#define I2C_SDA     GPIO_NUM_5
#define KEYPAD_INT  GPIO_NUM_7

// Brightness duty cycles for /G (active low: higher = dimmer).
// Values shared with app_state.h so the state machine can drive PWM.
#define DUTY_NORMAL DUTY_NORMAL_VAL
#define DUTY_DIMMED DUTY_DIMMED_VAL

// Digit at position 2 is physically mounted upside-down on the PCB.
#define FLIP_MASK 0b0100

static app_state_t g_state;
static keypad_t    g_keypad;

static uint8_t rotate180(uint8_t v) {
    uint8_t out = 0;
    if (v & SEG_A)  out |= SEG_D;
    if (v & SEG_B)  out |= SEG_E;
    if (v & SEG_C)  out |= SEG_F;
    if (v & SEG_D)  out |= SEG_A;
    if (v & SEG_E)  out |= SEG_B;
    if (v & SEG_F)  out |= SEG_C;
    if (v & SEG_G)  out |= SEG_G;
    if (v & SEG_DP) out |= SEG_DP;
    return out;
}

static void shift_out(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        gpio_set_level(TPIC_DATA, (data >> i) & 1);
        esp_rom_delay_us(1);
        gpio_set_level(TPIC_CLOCK, 1);
        esp_rom_delay_us(1);
        gpio_set_level(TPIC_CLOCK, 0);
    }
}

static void show_segments(const uint8_t segs[kDigits]) {
    gpio_set_level(TPIC_LATCH, 0);
    for (int pos = kDigits - 1; pos >= 0; pos--) {
        uint8_t out = segs[pos];
        if (FLIP_MASK & (1 << pos)) {
            out = rotate180(out);
        }
        shift_out(out);
    }
    gpio_set_level(TPIC_LATCH, 1);
}

static void play_snake_animation(void) {
    const uint8_t snake_segs[] = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F};
    const int snake_len = sizeof(snake_segs) / sizeof(snake_segs[0]);
    const int frames = 24;
    uint8_t segs[kDigits];
    for (int f = 0; f < frames; f++) {
        for (int i = 0; i < kDigits; i++) {
            int idx = (f + i * 2) % snake_len;
            segs[i] = snake_segs[idx];
        }
        show_segments(segs);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void set_brightness(int duty) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void app_main(void) {
    // Configure TPIC shift register pins
    gpio_config_t tpic_cfg = {
        .pin_bit_mask = (1ULL << TPIC_DATA) | (1ULL << TPIC_CLOCK) | (1ULL << TPIC_LATCH),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&tpic_cfg));

    // PWM brightness on /G pin
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num = TPIC_G,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = DUTY_NORMAL,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    // I2C bus (keypad PCF8574)
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &i2c_bus));

    // Keypad init
    keypad_init(&g_keypad, i2c_bus, KEYPAD_INT);

    // App state init
    app_state_init(&g_state);

    // Startup animation
    play_snake_animation();

    // Main loop
    while (1) {
        uint32_t now = millis_now();

        char key = keypad_poll(&g_keypad);
        if (key) handleKey(&g_state, key, now);

        updateMode(&g_state, now);

        set_brightness(g_state.paused ? DUTY_DIMMED : g_state.targetDuty);

        if (g_state.segsDirty) {
            show_segments(g_state.segs);
            g_state.segsDirty = false;
        }

        vTaskDelay(1);
    }
}
