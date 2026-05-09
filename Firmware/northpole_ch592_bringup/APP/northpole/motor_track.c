#include "motor_track.h"

#include "motor_drv8837.h"

static motor_track_state_t track_state;

static const uint16_t quarter_sine_permille[64] = {
    0, 25, 49, 74, 98, 122, 147, 171,
    195, 219, 243, 267, 290, 314, 337, 360,
    383, 405, 428, 450, 471, 493, 514, 535,
    556, 576, 596, 615, 634, 653, 672, 690,
    707, 724, 741, 757, 773, 788, 803, 818,
    831, 845, 858, 870, 882, 893, 904, 914,
    924, 933, 942, 949, 957, 964, 970, 976,
    981, 985, 989, 992, 995, 997, 999, 1000,
};

static int16_t sine_8bit(uint8_t phase)
{
    if (phase < 64) {
        return (int16_t)quarter_sine_permille[phase];
    }
    if (phase < 128) {
        return (int16_t)quarter_sine_permille[127u - phase];
    }
    if (phase < 192) {
        return -(int16_t)quarter_sine_permille[phase - 128u];
    }
    return -(int16_t)quarter_sine_permille[255u - phase];
}

static void command_signed(motor_driver_id_t driver, int16_t value, uint16_t duty_limit_permille)
{
    uint16_t duty = value < 0 ? (uint16_t)(-value) : (uint16_t)value;
    duty = (uint16_t)((uint32_t)duty * duty_limit_permille / 1000u);

    if (value > 0) {
        (void)motor_drv8837_command(driver, MOTOR_DRV_FORWARD, duty);
    } else if (value < 0) {
        (void)motor_drv8837_command(driver, MOTOR_DRV_REVERSE, duty);
    } else {
        (void)motor_drv8837_command(driver, MOTOR_DRV_COAST, 0);
    }
}

void motor_track_init(void)
{
    track_state.target_speed = 0;
    track_state.current_speed = 0;
    track_state.phase_index = 0;
    track_state.enabled = 0;
}

void motor_track_set_target_speed(int16_t speed)
{
    track_state.target_speed = speed;
    track_state.enabled = speed != 0;
}

void motor_track_stop(void)
{
    track_state.target_speed = 0;
    track_state.current_speed = 0;
    track_state.enabled = 0;
    motor_drv8837_all_coast();
}

void motor_track_tick(void)
{
    if (track_state.current_speed < track_state.target_speed) {
        track_state.current_speed++;
    } else if (track_state.current_speed > track_state.target_speed) {
        track_state.current_speed--;
    }
}

void motor_track_step(int8_t direction, uint16_t duty_limit_permille)
{
    int8_t step = direction < 0 ? -1 : 1;
    track_state.phase_index = (uint8_t)(track_state.phase_index + step);

    command_signed(MOTOR_DRV_A, sine_8bit(track_state.phase_index), duty_limit_permille);
    command_signed(MOTOR_DRV_B, sine_8bit((uint8_t)(track_state.phase_index + 64u)), duty_limit_permille);
    (void)motor_drv8837_command(MOTOR_DRV_G, MOTOR_DRV_COAST, 0);
}

motor_track_state_t motor_track_get_state(void)
{
    return track_state;
}
