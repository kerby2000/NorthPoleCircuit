#ifndef POWER_IP5209_H
#define POWER_IP5209_H

#include <stdint.h>

typedef struct {
    uint8_t i2c_present;
    uint8_t int_level;
    uint8_t charging;
    uint8_t boost_enabled;
} power_ip5209_status_t;

void power_ip5209_init(void);
power_ip5209_status_t power_ip5209_status(void);
int power_ip5209_read_register(uint8_t reg, uint8_t *value);
int power_ip5209_write_register(uint8_t reg, uint8_t value);

#endif /* POWER_IP5209_H */
