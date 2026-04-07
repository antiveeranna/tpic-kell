#include "keypad.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "utils.h"

#define PCF8574_ADDR   0x20
#define ROW_MASK       0x0F
#define DEBOUNCE_MS    25

static const char KEYMAP[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

static volatile bool s_pending = false;

static void IRAM_ATTR keypad_isr(void *arg) {
    (void)arg;
    __atomic_store_n(&s_pending, true, __ATOMIC_RELEASE);
}

static void pcf_write(keypad_t *kp, uint8_t value) {
    i2c_master_transmit(kp->dev, &value, 1, 100);
}

static uint8_t pcf_read(keypad_t *kp) {
    uint8_t val = 0xFF;
    i2c_master_receive(kp->dev, &val, 1, 100);
    return val;
}

static void keypad_arm(keypad_t *kp) {
    pcf_write(kp, 0x0F);
}

static char keypad_scan_once(keypad_t *kp) {
    for (int col = 0; col < 4; col++) {
        uint8_t out = 0xFF;
        out &= ~(1 << (4 + col));
        pcf_write(kp, out);
        esp_rom_delay_us(20);
        uint8_t in = pcf_read(kp);
        uint8_t rows = in & ROW_MASK;
        for (int row = 0; row < 4; row++) {
            if ((rows & (1 << row)) == 0) {
                return KEYMAP[row][col];
            }
        }
    }
    return 0;
}

void keypad_init(keypad_t *kp, i2c_master_bus_handle_t bus, uint8_t int_pin) {
    kp->int_pin     = int_pin;
    kp->last_stable = 0;
    kp->last_read   = 0;
    kp->last_change = 0;

    // Register PCF8574 on I2C bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF8574_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &kp->dev));

    // Configure interrupt pin
    gpio_config_t io_cfg = {
        .pin_bit_mask = 1ULL << int_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_cfg));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(int_pin, keypad_isr, NULL));

    pcf_write(kp, 0xFF);
    keypad_arm(kp);
    pcf_read(kp);
}

char keypad_poll(keypad_t *kp) {
    if (!__atomic_load_n(&s_pending, __ATOMIC_ACQUIRE)) return 0;

    char key = keypad_scan_once(kp);
    uint32_t now = millis_now();

    if (key != kp->last_read) {
        kp->last_read   = key;
        kp->last_change = now;
    }

    bool debouncing = (now - kp->last_change) < DEBOUNCE_MS;
    char result = 0;

    if (key != 0 && !debouncing && key != kp->last_stable) {
        kp->last_stable = key;
        result = key;
    }
    if (key == 0 && !debouncing) {
        kp->last_stable = 0;
    }

    keypad_arm(kp);
    pcf_read(kp);

    if (key == 0 && !debouncing && gpio_get_level(kp->int_pin) == 1) {
        __atomic_store_n(&s_pending, false, __ATOMIC_RELEASE);
    }

    return result;
}
