#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdint.h>

#define I2C_BUS_MAX_SCAN_RESULTS 16u
#define I2C_BUS_DEFAULT_HZ 100000u
#define I2C_BUS_DEFAULT_TIMEOUT_MS 3u

typedef struct {
    uint8_t initialized;
    uint32_t bus_hz;
    uint16_t last_error;
} i2c_bus_status_t;

void i2c_bus_init(uint32_t bus_hz);
i2c_bus_status_t i2c_bus_status(void);
int i2c_bus_probe(uint8_t addr7, uint32_t timeout_ms);
int i2c_bus_scan(uint8_t *addresses, uint8_t max_addresses, uint32_t timeout_ms);
int i2c_bus_read_reg8(uint8_t addr7, uint8_t reg, uint8_t *value, uint32_t timeout_ms);
int i2c_bus_write_reg8(uint8_t addr7, uint8_t reg, uint8_t value, uint32_t timeout_ms);

#endif /* I2C_BUS_H */
