#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>

typedef enum {
    FAULT_NONE = 0,
    FAULT_AUDIO_HW_BLOCKED,
    FAULT_AUDIO_TIMEOUT,
    FAULT_MOTOR_NOT_ARMED,
    FAULT_MOTOR_TIMEOUT,
    FAULT_MOTOR_DUTY_LIMIT,
    FAULT_LOW_BATTERY,
    FAULT_SETTINGS_INVALID,
    FAULT_BLE_ERROR,
    FAULT_COUNT,
} fault_code_t;

void fault_init(void);
void fault_raise(fault_code_t fault);
void fault_platform_on_fault(fault_code_t fault);
void fault_clear(fault_code_t fault);
void fault_clear_all(void);
uint8_t fault_is_set(fault_code_t fault);
uint32_t fault_snapshot(void);
const char *fault_name(fault_code_t fault);

#endif /* FAULT_H */
