#include "power_ip5209.h"

#include "app_config.h"
#include "board.h"
#include "i2c_bus.h"

#if defined(__GNUC__)
#define FW_WEAK __attribute__((weak))
#else
#define FW_WEAK
#endif

FW_WEAK int power_ip5209_platform_read_register(uint8_t reg, uint8_t *value)
{
    (void)reg;
    (void)value;
    return -1;
}

FW_WEAK int power_ip5209_platform_write_register(uint8_t reg, uint8_t value)
{
    (void)reg;
    (void)value;
    return -1;
}

static power_ip5209_status_t cached_status;

void power_ip5209_init(void)
{
    i2c_bus_init(I2C_BUS_DEFAULT_HZ);
    cached_status.i2c_present = i2c_bus_probe(APP_IP5209_I2C_ADDR, I2C_BUS_DEFAULT_TIMEOUT_MS) == 0 ? 1u : 0u;
    cached_status.int_level = board_input_read(BOARD_INPUT_IP5209_INT);
    cached_status.charging = 0;
    cached_status.boost_enabled = 0;
}

power_ip5209_status_t power_ip5209_status(void)
{
    power_ip5209_status_t status;
    status = cached_status;
    status.int_level = board_input_read(BOARD_INPUT_IP5209_INT);
    return status;
}

int power_ip5209_read_register(uint8_t reg, uint8_t *value)
{
    return power_ip5209_platform_read_register(reg, value);
}

int power_ip5209_write_register(uint8_t reg, uint8_t value)
{
    return power_ip5209_platform_write_register(reg, value);
}
