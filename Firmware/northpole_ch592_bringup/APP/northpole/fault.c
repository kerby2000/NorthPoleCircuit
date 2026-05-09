#include "fault.h"

#if defined(__GNUC__)
#define FW_WEAK __attribute__((weak))
#else
#define FW_WEAK
#endif

static uint32_t active_faults;

FW_WEAK void fault_platform_on_fault(fault_code_t fault)
{
    (void)fault;
}

void fault_init(void)
{
    active_faults = 0;
}

void fault_raise(fault_code_t fault)
{
    if (fault > FAULT_NONE && fault < FAULT_COUNT) {
        active_faults |= (1u << (uint32_t)fault);
        fault_platform_on_fault(fault);
    }
}

void fault_clear(fault_code_t fault)
{
    if (fault > FAULT_NONE && fault < FAULT_COUNT) {
        active_faults &= ~(1u << (uint32_t)fault);
    }
}

void fault_clear_all(void)
{
    active_faults = 0;
}

uint8_t fault_is_set(fault_code_t fault)
{
    if (fault <= FAULT_NONE || fault >= FAULT_COUNT) {
        return 0;
    }
    return (active_faults & (1u << (uint32_t)fault)) ? 1u : 0u;
}

uint32_t fault_snapshot(void)
{
    return active_faults;
}

const char *fault_name(fault_code_t fault)
{
    switch (fault) {
    case FAULT_NONE: return "none";
    case FAULT_AUDIO_HW_BLOCKED: return "audio_hw_blocked";
    case FAULT_AUDIO_TIMEOUT: return "audio_timeout";
    case FAULT_MOTOR_NOT_ARMED: return "motor_not_armed";
    case FAULT_MOTOR_TIMEOUT: return "motor_timeout";
    case FAULT_MOTOR_DUTY_LIMIT: return "motor_duty_limit";
    case FAULT_LOW_BATTERY: return "low_battery";
    case FAULT_SETTINGS_INVALID: return "settings_invalid";
    case FAULT_BLE_ERROR: return "ble_error";
    default: return "unknown";
    }
}
