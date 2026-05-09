#ifndef MOTOR_TRACK_H
#define MOTOR_TRACK_H

#include <stdint.h>

typedef struct {
    int16_t target_speed;
    int16_t current_speed;
    uint8_t phase_index;
    uint8_t enabled;
} motor_track_state_t;

void motor_track_init(void);
void motor_track_set_target_speed(int16_t speed);
void motor_track_stop(void);
void motor_track_tick(void);
void motor_track_step(int8_t direction, uint16_t duty_limit_permille);
motor_track_state_t motor_track_get_state(void);

#endif /* MOTOR_TRACK_H */
