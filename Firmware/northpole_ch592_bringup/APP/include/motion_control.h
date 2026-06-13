#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

#include <stdint.h>

typedef struct {
    uint8_t enabled;
    uint8_t running;
    uint8_t stopping;
    uint8_t target_flags;
    uint8_t sleep_high;
    uint8_t guard_mode;
    uint16_t sine_table_size;
    uint16_t amplitude_current_permille;
    uint16_t amplitude_target_permille;
    uint16_t guard_duty_permille;
    uint16_t ramp_start_ms;
    uint16_t ramp_stop_ms;
    uint16_t ramp_speed_ms;
    uint32_t carrier_hz;
    uint32_t control_update_hz;
    uint32_t phase_acc;
    uint32_t phase_inc;
    uint32_t update_tick_count;
    uint32_t missed_update_count;
    int32_t speed_current_hz_x1000;
    int32_t speed_target_hz_x1000;
    int32_t speed_step_hz_x1000;
    int32_t max_speed_hz_x1000;
    int last_start_rc;
} motion_control_status_t;

void motion_control_init(void);
void motion_control_poll(void);
int motion_control_start(void);
void motion_control_stop(void);
void motion_control_stop_immediate(void);
int motion_control_set_speed(int32_t speed_hz_x1000);
int motion_control_adjust_speed(int32_t delta_hz_x1000);
int motion_control_set_amplitude(uint16_t amplitude_permille);
int motion_control_set_guard(uint8_t guard_mode, uint16_t duty_permille);
int motion_control_set_carrier_hz(uint32_t carrier_hz);
int motion_control_set_update_hz(uint32_t update_hz);
int motion_control_set_ramp(uint16_t start_ms, uint16_t stop_ms, uint16_t speed_ms);
int motion_control_tune_original_like(void);
int motion_control_tune_proven(void);
void motion_control_status(motion_control_status_t *status);

#endif /* MOTION_CONTROL_H */
