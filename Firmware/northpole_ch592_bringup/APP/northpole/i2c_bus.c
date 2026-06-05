#include "i2c_bus.h"

#if defined(__GNUC__)
#define FW_WEAK __attribute__((weak))
#else
#define FW_WEAK
#endif

static i2c_bus_status_t bus_status;

static void ensure_init(void);

FW_WEAK int i2c_bus_platform_init(uint32_t bus_hz)
{
    (void)bus_hz;
    return -1;
}

FW_WEAK int i2c_bus_platform_probe(uint8_t addr7, uint32_t timeout_ms)
{
    (void)addr7;
    (void)timeout_ms;
    return -1;
}

FW_WEAK int i2c_bus_platform_read_reg8(uint8_t addr7, uint8_t reg, uint8_t *value, uint32_t timeout_ms)
{
    (void)addr7;
    (void)reg;
    (void)value;
    (void)timeout_ms;
    return -1;
}

FW_WEAK int i2c_bus_platform_write_reg8(uint8_t addr7, uint8_t reg, uint8_t value, uint32_t timeout_ms)
{
    (void)addr7;
    (void)reg;
    (void)value;
    (void)timeout_ms;
    return -1;
}

FW_WEAK int i2c_bus_platform_debug_snapshot(i2c_bus_debug_t *debug)
{
    (void)debug;
    return -1;
}

FW_WEAK int i2c_bus_platform_release_debug_pins(void)
{
    return -1;
}

void i2c_bus_init(uint32_t bus_hz)
{
    int rc;

    if (bus_hz == 0) {
        bus_hz = I2C_BUS_DEFAULT_HZ;
    }

    rc = i2c_bus_platform_init(bus_hz);
    bus_status.initialized = rc == 0 ? 1u : 0u;
    bus_status.bus_hz = bus_hz;
    bus_status.last_error = (uint16_t)(rc < 0 ? -rc : rc);
}

i2c_bus_status_t i2c_bus_status(void)
{
    return bus_status;
}

int i2c_bus_debug_snapshot(i2c_bus_debug_t *debug)
{
    if (!debug) {
        return -1;
    }
    ensure_init();
    return i2c_bus_platform_debug_snapshot(debug);
}

int i2c_bus_release_debug_pins(void)
{
    int rc = i2c_bus_platform_release_debug_pins();
    if (rc == 0) {
        i2c_bus_init(bus_status.bus_hz ? bus_status.bus_hz : I2C_BUS_DEFAULT_HZ);
    }
    bus_status.last_error = (uint16_t)(rc < 0 ? -rc : rc);
    return rc;
}

static void ensure_init(void)
{
    if (!bus_status.initialized) {
        i2c_bus_init(I2C_BUS_DEFAULT_HZ);
    }
}

int i2c_bus_probe(uint8_t addr7, uint32_t timeout_ms)
{
    int rc;

    ensure_init();
    if (!bus_status.initialized) {
        return -1;
    }
    rc = i2c_bus_platform_probe(addr7, timeout_ms);
    bus_status.last_error = (uint16_t)(rc < 0 ? -rc : rc);
    return rc;
}

int i2c_bus_scan(uint8_t *addresses, uint8_t max_addresses, uint32_t timeout_ms)
{
    uint8_t found = 0;

    if (!addresses || max_addresses == 0) {
        return -1;
    }

    ensure_init();
    if (!bus_status.initialized) {
        return -2;
    }

    for (uint8_t addr = 0x08u; addr <= 0x77u; ++addr) {
        if (i2c_bus_probe(addr, timeout_ms) == 0) {
            if (found < max_addresses) {
                addresses[found] = addr;
            }
            found++;
        }
    }

    return found;
}

int i2c_bus_read_reg8(uint8_t addr7, uint8_t reg, uint8_t *value, uint32_t timeout_ms)
{
    int rc;

    if (!value) {
        return -1;
    }
    ensure_init();
    if (!bus_status.initialized) {
        return -2;
    }
    rc = i2c_bus_platform_read_reg8(addr7, reg, value, timeout_ms);
    bus_status.last_error = (uint16_t)(rc < 0 ? -rc : rc);
    return rc;
}

int i2c_bus_write_reg8(uint8_t addr7, uint8_t reg, uint8_t value, uint32_t timeout_ms)
{
    int rc;

    ensure_init();
    if (!bus_status.initialized) {
        return -2;
    }
    rc = i2c_bus_platform_write_reg8(addr7, reg, value, timeout_ms);
    bus_status.last_error = (uint16_t)(rc < 0 ? -rc : rc);
    return rc;
}
