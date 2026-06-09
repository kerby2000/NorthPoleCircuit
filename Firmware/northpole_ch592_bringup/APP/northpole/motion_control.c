#include "motion_control.h"

#include "app_config.h"
#include "log.h"
#include "northpole_ch592_port.h"
#include "timebase.h"
#include "touch.h"

#include <stddef.h>

#ifndef APP_MOTION_TOUCH_DEBOUNCE_MS
#define APP_MOTION_TOUCH_DEBOUNCE_MS 150u
#endif

typedef struct {
    uint8_t initialized;
    uint8_t running;
    uint8_t last_run_raw;
    uint8_t last_spd_plus_raw;
    uint8_t last_spd_minus_raw;
    uint8_t guard_mode;
    uint16_t guard_duty_permille;
    int32_t speed_hz_x1000;
    uint32_t last_run_edge_ms;
    uint32_t last_spd_plus_edge_ms;
    uint32_t last_spd_minus_edge_ms;
    int last_start_rc;
} motion_control_runtime_t;

static motion_control_runtime_t motion;

static int32_t motion_abs_i32(int32_t value)
{
    return value < 0 ? -value : value;
}

static int32_t motion_clamp_speed(int32_t speed_hz_x1000)
{
    if (speed_hz_x1000 > APP_MOTION_MAX_SPEED_HZ_X1000) {
        return APP_MOTION_MAX_SPEED_HZ_X1000;
    }
    if (speed_hz_x1000 < -APP_MOTION_MAX_SPEED_HZ_X1000) {
        return -APP_MOTION_MAX_SPEED_HZ_X1000;
    }
    return speed_hz_x1000;
}

static uint8_t motion_touch_raw(touch_pad_id_t pad)
{
    return touch_get_state(pad).raw ? 1u : 0u;
}

static uint8_t motion_rising_edge(uint8_t raw, uint8_t *last_raw, uint32_t *last_edge_ms)
{
    uint32_t now_ms = timebase_ms();
    uint8_t rising = raw && !*last_raw;

    *last_raw = raw;
    if (!rising) {
        return 0u;
    }
    if ((uint32_t)(now_ms - *last_edge_ms) < APP_MOTION_TOUCH_DEBOUNCE_MS) {
        return 0u;
    }
    *last_edge_ms = now_ms;
    return 1u;
}

static const char *motion_direction_name(int32_t speed_hz_x1000)
{
    if (speed_hz_x1000 < 0) {
        return "reverse";
    }
    if (speed_hz_x1000 > 0) {
        return "forward";
    }
    return "stopped";
}

static int motion_apply_wave(void)
{
#if APP_MOTION_CONTROL_ENABLE
    uint32_t electrical_hz_x1000;
    int8_t direction;

    if (!motion.running || motion.speed_hz_x1000 == 0) {
        northpole_motor_wave_stop();
        motion.last_start_rc = 0;
        return 0;
    }

    electrical_hz_x1000 = (uint32_t)motion_abs_i32(motion.speed_hz_x1000);
    direction = motion.speed_hz_x1000 < 0 ? -1 : 1;
    motion.last_start_rc = northpole_motor_wave_start_ex(
        electrical_hz_x1000,
        APP_MOTION_AMPLITUDE_PERMILLE,
        NORTHPOLE_MOTOR_WAVE_TARGET_A |
            NORTHPOLE_MOTOR_WAVE_TARGET_B |
            NORTHPOLE_MOTOR_WAVE_TARGET_G,
        1u,
        direction,
        motion.guard_mode,
        motion.guard_duty_permille);
    return motion.last_start_rc;
#else
    motion.last_start_rc = -10;
    return -10;
#endif
}

void motion_control_init(void)
{
    if (motion.initialized) {
        return;
    }

    motion.running = 0u;
    motion.speed_hz_x1000 = motion_clamp_speed(APP_MOTION_DEFAULT_SPEED_HZ_X1000);
    if (motion.speed_hz_x1000 == 0) {
        motion.speed_hz_x1000 = APP_MOTION_SPEED_STEP_HZ_X1000;
    }
    motion.guard_mode = (uint8_t)APP_MOTION_GUARD_MODE;
    motion.guard_duty_permille = APP_MOTION_GUARD_DUTY_PERMILLE;
    motion.last_start_rc = 0;
    motion.initialized = 1u;
}

int motion_control_start(void)
{
    motion_control_init();
    if (motion.speed_hz_x1000 == 0) {
        motion.speed_hz_x1000 = APP_MOTION_DEFAULT_SPEED_HZ_X1000;
    }
    if (motion.speed_hz_x1000 == 0) {
        motion.speed_hz_x1000 = APP_MOTION_SPEED_STEP_HZ_X1000;
    }
    motion.running = 1u;
    return motion_apply_wave();
}

void motion_control_stop(void)
{
    motion_control_init();
    motion.running = 0u;
    northpole_motor_wave_stop();
}

int motion_control_set_speed(int32_t speed_hz_x1000)
{
    motion_control_init();
    motion.speed_hz_x1000 = motion_clamp_speed(speed_hz_x1000);
    if (motion.running) {
        return motion_apply_wave();
    }
    return 0;
}

int motion_control_adjust_speed(int32_t delta_hz_x1000)
{
    int32_t old_speed;
    int32_t new_speed;

    motion_control_init();
    old_speed = motion.speed_hz_x1000;
    new_speed = old_speed + delta_hz_x1000;

    if (old_speed > 0 && delta_hz_x1000 < 0 && new_speed <= 0) {
        new_speed = -APP_MOTION_SPEED_STEP_HZ_X1000;
    } else if (old_speed < 0 && delta_hz_x1000 > 0 && new_speed >= 0) {
        new_speed = APP_MOTION_SPEED_STEP_HZ_X1000;
    } else if (old_speed == 0) {
        new_speed = delta_hz_x1000 >= 0 ?
            APP_MOTION_SPEED_STEP_HZ_X1000 :
            -APP_MOTION_SPEED_STEP_HZ_X1000;
    }

    motion.speed_hz_x1000 = motion_clamp_speed(new_speed);
    if (motion.running) {
        return motion_apply_wave();
    }
    return 0;
}

int motion_control_set_guard(uint8_t guard_mode, uint16_t duty_permille)
{
    motion_control_init();
    switch (guard_mode) {
    case NORTHPOLE_MOTOR_GUARD_OFF:
    case NORTHPOLE_MOTOR_GUARD_FORWARD:
    case NORTHPOLE_MOTOR_GUARD_REVERSE:
    case NORTHPOLE_MOTOR_GUARD_PHASE_A:
    case NORTHPOLE_MOTOR_GUARD_PHASE_B:
        break;
    default:
        return -1;
    }
    if (duty_permille > 1000u) {
        duty_permille = 1000u;
    }
    motion.guard_mode = guard_mode;
    motion.guard_duty_permille = duty_permille;
    if (motion.running) {
        return motion_apply_wave();
    }
    return 0;
}

void motion_control_poll(void)
{
    uint8_t run_raw;
    uint8_t spd_plus_raw;
    uint8_t spd_minus_raw;
    int rc;

    motion_control_init();
#if !APP_MOTION_CONTROL_ENABLE
    return;
#endif

    run_raw = motion_touch_raw(TOUCH_RUN);
    spd_plus_raw = motion_touch_raw(TOUCH_SPD_PLUS);
    spd_minus_raw = motion_touch_raw(TOUCH_SPD_MINUS);

    if (motion_rising_edge(run_raw, &motion.last_run_raw, &motion.last_run_edge_ms)) {
        if (motion.running) {
            motion_control_stop();
            LOG_INFO("motion run=0 speed_hz_x1000=%ld\r\n",
                     (long)motion.speed_hz_x1000);
        } else {
            rc = motion_control_start();
            LOG_INFO("motion run=1 speed_hz_x1000=%ld direction=%s guard=%s duty=%u rc=%d\r\n",
                     (long)motion.speed_hz_x1000,
                     motion_direction_name(motion.speed_hz_x1000),
                     northpole_motor_guard_mode_name(motion.guard_mode),
                     (unsigned)motion.guard_duty_permille,
                     rc);
        }
    }

    if (motion_rising_edge(spd_plus_raw, &motion.last_spd_plus_raw, &motion.last_spd_plus_edge_ms)) {
        rc = motion_control_adjust_speed(APP_MOTION_SPEED_STEP_HZ_X1000);
        LOG_INFO("motion speed+ speed_hz_x1000=%ld direction=%s rc=%d\r\n",
                 (long)motion.speed_hz_x1000,
                 motion_direction_name(motion.speed_hz_x1000),
                 rc);
    }

    if (motion_rising_edge(spd_minus_raw, &motion.last_spd_minus_raw, &motion.last_spd_minus_edge_ms)) {
        rc = motion_control_adjust_speed(-APP_MOTION_SPEED_STEP_HZ_X1000);
        LOG_INFO("motion speed- speed_hz_x1000=%ld direction=%s rc=%d\r\n",
                 (long)motion.speed_hz_x1000,
                 motion_direction_name(motion.speed_hz_x1000),
                 rc);
    }
}

void motion_control_status(motion_control_status_t *status)
{
    if (status == NULL) {
        return;
    }
    motion_control_init();
    status->enabled = APP_MOTION_CONTROL_ENABLE ? 1u : 0u;
    status->running = motion.running;
    status->target_flags = NORTHPOLE_MOTOR_WAVE_TARGET_A |
        NORTHPOLE_MOTOR_WAVE_TARGET_B |
        NORTHPOLE_MOTOR_WAVE_TARGET_G;
    status->sleep_high = motion.running ? 1u : 0u;
    status->guard_mode = motion.guard_mode;
    status->amplitude_permille = APP_MOTION_AMPLITUDE_PERMILLE;
    status->guard_duty_permille = motion.guard_duty_permille;
    status->speed_hz_x1000 = motion.speed_hz_x1000;
    status->speed_step_hz_x1000 = APP_MOTION_SPEED_STEP_HZ_X1000;
    status->max_speed_hz_x1000 = APP_MOTION_MAX_SPEED_HZ_X1000;
    status->last_start_rc = motion.last_start_rc;
}
