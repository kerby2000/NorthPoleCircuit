#include "battery.h"

static battery_state_t current_state = BATTERY_STATE_UNKNOWN;
static uint16_t current_mv;

void battery_init(void)
{
    current_state = BATTERY_STATE_UNKNOWN;
    current_mv = 0;
}

battery_state_t battery_state(void)
{
    return current_state;
}

uint16_t battery_mv(void)
{
    return current_mv;
}

uint8_t battery_allows_motor_start(void)
{
    return current_state != BATTERY_STATE_LOW && current_state != BATTERY_STATE_UNKNOWN;
}

const char *battery_state_name(battery_state_t state)
{
    switch (state) {
    case BATTERY_STATE_UNKNOWN: return "unknown";
    case BATTERY_STATE_USB_ONLY: return "usb_only";
    case BATTERY_STATE_CHARGING: return "charging";
    case BATTERY_STATE_DISCHARGING: return "discharging";
    case BATTERY_STATE_LOW: return "low";
    default: return "?";
    }
}
