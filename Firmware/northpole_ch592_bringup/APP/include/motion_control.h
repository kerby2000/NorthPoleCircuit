#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

#include <stdint.h>

typedef struct {
    uint8_t enabled;
    uint8_t running;
    uint8_t target_flags;
    uint8_t sleep_high;
    uint8_t guard_mode;
    uint16_t amplitude_permille;
    uint16_t guard_duty_permille;
    int32_t speed_hz_x1000;
    int32_t speed_step_hz_x1000;
    int32_t max_speed_hz_x1000;
    int last_start_rc;
} motion_control_status_t;

void motion_control_init(void);
void motion_control_poll(void);
int motion_control_start(void);
void motion_control_stop(void);
int motion_control_set_speed(int32_t speed_hz_x1000);
int motion_control_adjust_speed(int32_t delta_hz_x1000);
int motion_control_set_guard(uint8_t guard_mode, uint16_t duty_permille);
void motion_control_status(motion_control_status_t *status);

#endif /* MOTION_CONTROL_H */
