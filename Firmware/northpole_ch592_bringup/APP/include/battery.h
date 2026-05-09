#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

typedef enum {
    BATTERY_STATE_UNKNOWN = 0,
    BATTERY_STATE_USB_ONLY,
    BATTERY_STATE_CHARGING,
    BATTERY_STATE_DISCHARGING,
    BATTERY_STATE_LOW,
} battery_state_t;

void battery_init(void);
battery_state_t battery_state(void);
uint16_t battery_mv(void);
uint8_t battery_allows_motor_start(void);
const char *battery_state_name(battery_state_t state);

#endif /* BATTERY_H */
