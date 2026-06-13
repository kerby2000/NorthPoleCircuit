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
    uint8_t stopping;
    uint8_t last_run_raw;
    uint8_t last_spd_plus_raw;
    uint8_t last_spd_minus_raw;
    uint8_t guard_mode;
    uint16_t amplitude_current_permille;
    uint16_t amplitude_target_permille;
    uint16_t amplitude_ramp_from_permille;
    uint16_t amplitude_ramp_to_permille;
    uint16_t amplitude_ramp_ms;
    uint16_t guard_duty_permille;
    uint16_t ramp_start_ms;
    uint16_t ramp_stop_ms;
    uint16_t ramp_speed_ms;
    uint32_t carrier_hz;
    uint32_t control_update_hz;
    uint32_t amplitude_ramp_start_ms;
    uint32_t speed_ramp_start_ms;
    uint32_t last_run_edge_ms;
    uint32_t last_spd_plus_edge_ms;
    uint32_t last_spd_minus_edge_ms;
    int32_t speed_current_hz_x1000;
    int32_t speed_target_hz_x1000;
    int32_t speed_ramp_from_hz_x1000;
    int32_t speed_ramp_to_hz_x1000;
    int last_start_rc;
} motion_control_runtime_t;

static motion_control_runtime_t motion;

static int32_t motion_abs_i32(int32_t value)
{
    return value < 0 ? -value : value;
}

static uint16_t motion_clamp_permille(uint16_t value)
{
    return value > 1000u ? 1000u : value;
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

static uint8_t motion_guard_mode_valid(uint8_t guard_mode)
{
    switch (guard_mode) {
    case NORTHPOLE_MOTOR_GUARD_OFF:
    case NORTHPOLE_MOTOR_GUARD_FORWARD:
    case NORTHPOLE_MOTOR_GUARD_REVERSE:
    case NORTHPOLE_MOTOR_GUARD_PHASE_A:
    case NORTHPOLE_MOTOR_GUARD_PHASE_B:
        return 1u;
    default:
        return 0u;
    }
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

static int32_t motion_lerp_i32(int32_t from, int32_t to, uint32_t elapsed_ms, uint32_t duration_ms)
{
    int64_t delta;

    if (duration_ms == 0u || elapsed_ms >= duration_ms) {
        return to;
    }
    delta = (int64_t)to - (int64_t)from;
    return (int32_t)((int64_t)from + ((delta * (int64_t)elapsed_ms) / (int64_t)duration_ms));
}

static uint16_t motion_lerp_u16(uint16_t from, uint16_t to, uint32_t elapsed_ms, uint32_t duration_ms)
{
    int32_t value;

    value = motion_lerp_i32((int32_t)from, (int32_t)to, elapsed_ms, duration_ms);
    if (value < 0) {
        value = 0;
    }
    if (value > 1000) {
        value = 1000;
    }
    return (uint16_t)value;
}

static void motion_begin_speed_ramp(int32_t target_hz_x1000)
{
    motion.speed_ramp_from_hz_x1000 = motion.speed_current_hz_x1000;
    motion.speed_ramp_to_hz_x1000 = motion_clamp_speed(target_hz_x1000);
    motion.speed_target_hz_x1000 = motion.speed_ramp_to_hz_x1000;
    motion.speed_ramp_start_ms = timebase_ms();
    if (!motion.running) {
        motion.speed_current_hz_x1000 = motion.speed_target_hz_x1000;
    }
}

static void motion_begin_amplitude_ramp(uint16_t target_permille, uint16_t duration_ms)
{
    target_permille = motion_clamp_permille(target_permille);
    motion.amplitude_ramp_from_permille = motion.amplitude_current_permille;
    motion.amplitude_ramp_to_permille = target_permille;
    motion.amplitude_target_permille = target_permille;
    motion.amplitude_ramp_ms = duration_ms;
    motion.amplitude_ramp_start_ms = timebase_ms();
}

static void motion_begin_stop_ramp(uint16_t duration_ms)
{
    motion.amplitude_ramp_from_permille = motion.amplitude_current_permille;
    motion.amplitude_ramp_to_permille = 0u;
    motion.amplitude_ramp_ms = duration_ms;
    motion.amplitude_ramp_start_ms = timebase_ms();
}

static int8_t motion_current_direction(void)
{
    if (motion.speed_current_hz_x1000 < 0) {
        return -1;
    }
    if (motion.speed_current_hz_x1000 > 0) {
        return 1;
    }
    return motion.speed_target_hz_x1000 < 0 ? -1 : 1;
}

static int motion_apply_wave_update(void)
{
#if APP_MOTION_CONTROL_ENABLE
    uint32_t electrical_hz_x1000;
    int8_t direction;

    if (!motion.running && !motion.stopping) {
        motion.last_start_rc = 0;
        return 0;
    }

    electrical_hz_x1000 = (uint32_t)motion_abs_i32(motion.speed_current_hz_x1000);
    direction = motion_current_direction();
    motion.last_start_rc = northpole_motor_wave_update(electrical_hz_x1000,
                                                       motion.amplitude_current_permille,
                                                       direction,
                                                       motion.guard_mode,
                                                       motion.guard_duty_permille);
    return motion.last_start_rc;
#else
    motion.last_start_rc = -10;
    return -10;
#endif
}

static void motion_update_ramps(void)
{
    uint32_t now_ms;
    uint32_t elapsed_ms;

    if (!motion.running && !motion.stopping) {
        return;
    }

    now_ms = timebase_ms();
    elapsed_ms = now_ms - motion.speed_ramp_start_ms;
    motion.speed_current_hz_x1000 = motion_lerp_i32(motion.speed_ramp_from_hz_x1000,
                                                    motion.speed_ramp_to_hz_x1000,
                                                    elapsed_ms,
                                                    motion.ramp_speed_ms);
    elapsed_ms = now_ms - motion.amplitude_ramp_start_ms;
    motion.amplitude_current_permille = motion_lerp_u16(motion.amplitude_ramp_from_permille,
                                                        motion.amplitude_ramp_to_permille,
                                                        elapsed_ms,
                                                        motion.amplitude_ramp_ms);

    if (motion.stopping && motion.amplitude_current_permille == 0u) {
        motion_control_stop_immediate();
        return;
    }
    (void)motion_apply_wave_update();
}

void motion_control_init(void)
{
    if (motion.initialized) {
        return;
    }

    motion.running = 0u;
    motion.stopping = 0u;
    motion.speed_target_hz_x1000 = motion_clamp_speed(APP_MOTION_DEFAULT_SPEED_HZ_X1000);
    if (motion.speed_target_hz_x1000 == 0) {
        motion.speed_target_hz_x1000 = APP_MOTION_SPEED_STEP_HZ_X1000;
    }
    motion.speed_current_hz_x1000 = motion.speed_target_hz_x1000;
    motion.speed_ramp_from_hz_x1000 = motion.speed_current_hz_x1000;
    motion.speed_ramp_to_hz_x1000 = motion.speed_target_hz_x1000;
    motion.amplitude_current_permille = 0u;
    motion.amplitude_target_permille = motion_clamp_permille(APP_MOTION_AMPLITUDE_PERMILLE);
    motion.amplitude_ramp_from_permille = 0u;
    motion.amplitude_ramp_to_permille = motion.amplitude_target_permille;
    motion.guard_mode = (uint8_t)APP_MOTION_GUARD_MODE;
    motion.guard_duty_permille = motion_clamp_permille(APP_MOTION_GUARD_DUTY_PERMILLE);
    motion.ramp_start_ms = APP_MOTION_RAMP_MS_START;
    motion.ramp_stop_ms = APP_MOTION_RAMP_MS_STOP;
    motion.ramp_speed_ms = APP_MOTION_RAMP_MS_SPEED;
    motion.carrier_hz = APP_MOTOR_PWM_DEFAULT_HZ;
    motion.control_update_hz = APP_MOTOR_CONTROL_UPDATE_HZ;
    motion.last_start_rc = 0;
    motion.initialized = 1u;
}

int motion_control_start(void)
{
    uint32_t electrical_hz_x1000;
    int8_t direction;

    motion_control_init();
    if (motion.speed_target_hz_x1000 == 0) {
        motion.speed_target_hz_x1000 = APP_MOTION_DEFAULT_SPEED_HZ_X1000;
    }
    if (motion.speed_target_hz_x1000 == 0) {
        motion.speed_target_hz_x1000 = APP_MOTION_SPEED_STEP_HZ_X1000;
    }

    motion.running = 1u;
    motion.stopping = 0u;
    motion.speed_current_hz_x1000 = 0;
    motion.speed_ramp_from_hz_x1000 = 0;
    motion.speed_ramp_to_hz_x1000 = motion.speed_target_hz_x1000;
    motion.speed_ramp_start_ms = timebase_ms();
    motion.amplitude_current_permille = 0u;

    /*
     * Start the timer engine with a non-zero phase increment, then let the
     * regular ramp update pull the effective speed down to zero on the first
     * poll. Starting with speed_current == 0 would make the low-level wave
     * starter reject the command before the ramp can run.
     */
    electrical_hz_x1000 = (uint32_t)motion_abs_i32(motion.speed_target_hz_x1000);
    direction = motion.speed_target_hz_x1000 < 0 ? -1 : 1;
    motion.last_start_rc = northpole_motor_wave_start_smooth_ex(
        electrical_hz_x1000,
        0u,
        NORTHPOLE_MOTOR_WAVE_TARGET_A |
            NORTHPOLE_MOTOR_WAVE_TARGET_B |
            NORTHPOLE_MOTOR_WAVE_TARGET_G,
        1u,
        direction,
        motion.guard_mode,
        motion.guard_duty_permille);
    if (motion.last_start_rc != 0) {
        motion.running = 0u;
        return motion.last_start_rc;
    }
    motion_begin_amplitude_ramp(motion.amplitude_target_permille, motion.ramp_start_ms);
    motion_update_ramps();
    return motion.last_start_rc;
}

void motion_control_stop(void)
{
    motion_control_init();
    if (!motion.running) {
        northpole_motor_wave_stop();
        return;
    }
    motion.stopping = 1u;
    motion_begin_stop_ramp(motion.ramp_stop_ms);
    motion_update_ramps();
}

void motion_control_stop_immediate(void)
{
    motion_control_init();
    motion.running = 0u;
    motion.stopping = 0u;
    motion.amplitude_current_permille = 0u;
    motion.amplitude_ramp_from_permille = 0u;
    motion.amplitude_ramp_to_permille = motion.amplitude_target_permille;
    northpole_motor_wave_stop();
}

int motion_control_set_speed(int32_t speed_hz_x1000)
{
    motion_control_init();
    motion_begin_speed_ramp(speed_hz_x1000);
    if (motion.running) {
        motion_update_ramps();
    }
    return 0;
}

int motion_control_adjust_speed(int32_t delta_hz_x1000)
{
    int32_t old_speed;
    int32_t new_speed;

    motion_control_init();
    old_speed = motion.speed_target_hz_x1000;
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

    return motion_control_set_speed(motion_clamp_speed(new_speed));
}

int motion_control_set_amplitude(uint16_t amplitude_permille)
{
    motion_control_init();
    amplitude_permille = motion_clamp_permille(amplitude_permille);
    if (motion.running) {
        motion_begin_amplitude_ramp(amplitude_permille, motion.ramp_start_ms);
        motion_update_ramps();
    } else {
        motion.amplitude_target_permille = amplitude_permille;
    }
    return 0;
}

int motion_control_set_guard(uint8_t guard_mode, uint16_t duty_permille)
{
    motion_control_init();
    if (!motion_guard_mode_valid(guard_mode)) {
        return -1;
    }
    motion.guard_mode = guard_mode;
    motion.guard_duty_permille = motion_clamp_permille(duty_permille);
    if (motion.running) {
        motion_update_ramps();
    }
    return 0;
}

int motion_control_set_carrier_hz(uint32_t carrier_hz)
{
    int rc;

    motion_control_init();
    if (motion.running) {
        return -2;
    }
    rc = northpole_motor_wave_set_carrier_hz(carrier_hz);
    if (rc == 0) {
        motion.carrier_hz = carrier_hz;
    }
    return rc;
}

int motion_control_set_update_hz(uint32_t update_hz)
{
    int rc;

    motion_control_init();
    if (motion.running) {
        return -2;
    }
    rc = northpole_motor_wave_set_control_update_hz(update_hz);
    if (rc == 0) {
        motion.control_update_hz = update_hz;
    }
    return rc;
}

int motion_control_set_ramp(uint16_t start_ms, uint16_t stop_ms, uint16_t speed_ms)
{
    motion_control_init();
    motion.ramp_start_ms = start_ms;
    motion.ramp_stop_ms = stop_ms;
    motion.ramp_speed_ms = speed_ms;
    return 0;
}

int motion_control_tune_original_like(void)
{
    int rc;

    motion_control_init();
    if (motion.running) {
        return -2;
    }
    rc = motion_control_set_carrier_hz(100000u);
    if (rc != 0) {
        return rc;
    }
    rc = motion_control_set_update_hz(4000u);
    if (rc != 0) {
        return rc;
    }
    motion.speed_target_hz_x1000 = 8000;
    motion.speed_current_hz_x1000 = motion.speed_target_hz_x1000;
    motion.speed_ramp_from_hz_x1000 = motion.speed_current_hz_x1000;
    motion.speed_ramp_to_hz_x1000 = motion.speed_target_hz_x1000;
    motion.amplitude_target_permille = 700u;
    motion.guard_mode = NORTHPOLE_MOTOR_GUARD_FORWARD;
    motion.guard_duty_permille = 600u;
    motion.ramp_start_ms = 500u;
    motion.ramp_stop_ms = 300u;
    motion.ramp_speed_ms = 500u;
    return 0;
}

int motion_control_tune_proven(void)
{
    int rc;

    motion_control_init();
    if (motion.running) {
        return -2;
    }
    rc = motion_control_set_carrier_hz(20000u);
    if (rc != 0) {
        return rc;
    }
    rc = motion_control_set_update_hz(4000u);
    if (rc != 0) {
        return rc;
    }
    motion.speed_target_hz_x1000 = 3000;
    motion.speed_current_hz_x1000 = motion.speed_target_hz_x1000;
    motion.speed_ramp_from_hz_x1000 = motion.speed_current_hz_x1000;
    motion.speed_ramp_to_hz_x1000 = motion.speed_target_hz_x1000;
    motion.amplitude_current_permille = 0u;
    motion.amplitude_target_permille = 1000u;
    motion.amplitude_ramp_from_permille = 0u;
    motion.amplitude_ramp_to_permille = motion.amplitude_target_permille;
    motion.guard_mode = NORTHPOLE_MOTOR_GUARD_FORWARD;
    motion.guard_duty_permille = 1000u;
    motion.ramp_start_ms = 0u;
    motion.ramp_stop_ms = 0u;
    motion.ramp_speed_ms = 0u;
    motion.last_start_rc = 0;
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

    motion_update_ramps();

#if !APP_MOTION_TOUCH_CONTROL_ENABLE
    return;
#endif

    run_raw = motion_touch_raw(TOUCH_RUN);
    spd_plus_raw = motion_touch_raw(TOUCH_SPD_PLUS);
    spd_minus_raw = motion_touch_raw(TOUCH_SPD_MINUS);

    if (motion_rising_edge(run_raw, &motion.last_run_raw, &motion.last_run_edge_ms)) {
        if (motion.running) {
            motion_control_stop();
            LOG_INFO("motion stopping speed_target_hz_x1000=%ld\r\n",
                     (long)motion.speed_target_hz_x1000);
        } else {
            rc = motion_control_start();
            LOG_INFO("motion run=1 speed_target_hz_x1000=%ld direction=%s guard=%s duty=%u rc=%d\r\n",
                     (long)motion.speed_target_hz_x1000,
                     motion_direction_name(motion.speed_target_hz_x1000),
                     northpole_motor_guard_mode_name(motion.guard_mode),
                     (unsigned)motion.guard_duty_permille,
                     rc);
        }
    }

    if (motion_rising_edge(spd_plus_raw, &motion.last_spd_plus_raw, &motion.last_spd_plus_edge_ms)) {
        rc = motion_control_adjust_speed(APP_MOTION_SPEED_STEP_HZ_X1000);
        LOG_INFO("motion speed+ speed_target_hz_x1000=%ld direction=%s rc=%d\r\n",
                 (long)motion.speed_target_hz_x1000,
                 motion_direction_name(motion.speed_target_hz_x1000),
                 rc);
    }

    if (motion_rising_edge(spd_minus_raw, &motion.last_spd_minus_raw, &motion.last_spd_minus_edge_ms)) {
        rc = motion_control_adjust_speed(-APP_MOTION_SPEED_STEP_HZ_X1000);
        LOG_INFO("motion speed- speed_target_hz_x1000=%ld direction=%s rc=%d\r\n",
                 (long)motion.speed_target_hz_x1000,
                 motion_direction_name(motion.speed_target_hz_x1000),
                 rc);
    }
}

void motion_control_status(motion_control_status_t *status)
{
    northpole_motor_wave_status_t wave;

    if (status == NULL) {
        return;
    }
    motion_control_init();
    motion_update_ramps();
    northpole_motor_wave_status(&wave);
    status->enabled = APP_MOTION_CONTROL_ENABLE ? 1u : 0u;
    status->running = motion.running;
    status->stopping = motion.stopping;
    status->target_flags = NORTHPOLE_MOTOR_WAVE_TARGET_A |
        NORTHPOLE_MOTOR_WAVE_TARGET_B |
        NORTHPOLE_MOTOR_WAVE_TARGET_G;
    status->sleep_high = motion.running ? 1u : 0u;
    status->guard_mode = motion.guard_mode;
    status->sine_table_size = wave.sine_table_size;
    status->amplitude_current_permille = motion.amplitude_current_permille;
    status->amplitude_target_permille = motion.amplitude_target_permille;
    status->guard_duty_permille = motion.guard_duty_permille;
    status->ramp_start_ms = motion.ramp_start_ms;
    status->ramp_stop_ms = motion.ramp_stop_ms;
    status->ramp_speed_ms = motion.ramp_speed_ms;
    status->carrier_hz = wave.carrier_hz;
    status->control_update_hz = wave.control_update_hz;
    status->phase_acc = wave.phase_acc;
    status->phase_inc = wave.phase_inc;
    status->update_tick_count = wave.tick_count;
    status->missed_update_count = wave.missed_update_count;
    status->speed_current_hz_x1000 = motion.speed_current_hz_x1000;
    status->speed_target_hz_x1000 = motion.speed_target_hz_x1000;
    status->speed_step_hz_x1000 = APP_MOTION_SPEED_STEP_HZ_X1000;
    status->max_speed_hz_x1000 = APP_MOTION_MAX_SPEED_HZ_X1000;
    status->last_start_rc = motion.last_start_rc;
}
