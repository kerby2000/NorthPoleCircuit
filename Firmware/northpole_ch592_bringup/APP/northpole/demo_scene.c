#include "demo_scene.h"

#include "app_config.h"
#include "audio_wt2003.h"
#include "fault.h"
#include "hall.h"
#include "log.h"
#include "motion_control.h"
#include "motor_drv8837.h"
#include "northpole_ch592_port.h"
#include "rgb_ws2812.h"
#include "timebase.h"
#include "touch.h"

#include <stddef.h>
#include <string.h>

#if APP_DEMO_SCENE_ENABLE

typedef struct {
    uint8_t raw;
    uint8_t long_reported;
    uint32_t pressed_ms;
    uint32_t last_edge_ms;
} demo_touch_runtime_t;

typedef struct {
    uint8_t initialized;
    demo_scene_state_t state;
    demo_stop_reason_t stop_reason;
    uint8_t audio_enabled;
    uint8_t rgb_enabled;
    uint8_t hall_enabled;
    uint8_t intensity_percent;
    uint8_t rgb_brightness;
    uint8_t guard_mode;
    uint8_t rgb_index;
    uint16_t rgb_lfsr;
    demo_rgb_effect_t rgb_effect;
    uint16_t audio_track;
    uint16_t audio_volume;
    uint16_t amplitude_permille;
    uint16_t guard_duty_permille;
    uint32_t duration_ms;
    uint32_t state_entered_ms;
    uint32_t started_ms;
    uint32_t stopping_ms;
    uint32_t last_rgb_ms;
    uint32_t hall_edges[HALL_SENSOR_COUNT];
    uint32_t last_hall_edge_ms[HALL_SENSOR_COUNT];
    uint32_t hall_flash_until_ms;
    int32_t speed_hz_x1000;
    int last_motor_rc;
    demo_touch_runtime_t touch[TOUCH_COUNT];
} demo_scene_runtime_t;

static demo_scene_runtime_t demo;

static uint16_t demo_clamp_permille_u16(uint16_t value)
{
    return value > 1000u ? 1000u : value;
}

static uint8_t demo_clamp_percent_u8(uint8_t value)
{
    return value > 100u ? 100u : value;
}

static uint8_t demo_clamp_rgb_brightness(uint8_t value)
{
    return value > APP_RGB_BRINGUP_BRIGHTNESS_LIMIT ?
        APP_RGB_BRINGUP_BRIGHTNESS_LIMIT :
        value;
}

static uint32_t demo_clamp_duration(uint32_t duration_ms)
{
    if (duration_ms == 0u) {
        return APP_DEMO_DEFAULT_DURATION_MS;
    }
    return duration_ms > APP_DEMO_MAX_RUNTIME_MS ? APP_DEMO_MAX_RUNTIME_MS : duration_ms;
}

static int32_t demo_clamp_speed(int32_t speed_hz_x1000)
{
    if (speed_hz_x1000 > APP_MOTION_MAX_SPEED_HZ_X1000) {
        return APP_MOTION_MAX_SPEED_HZ_X1000;
    }
    if (speed_hz_x1000 < -APP_MOTION_MAX_SPEED_HZ_X1000) {
        return -APP_MOTION_MAX_SPEED_HZ_X1000;
    }
    if (speed_hz_x1000 == 0) {
        return APP_MOTION_SPEED_STEP_HZ_X1000;
    }
    return speed_hz_x1000;
}

static uint16_t demo_intensity_to_amplitude(uint8_t percent)
{
    uint32_t value = ((uint32_t)APP_DEMO_DEFAULT_AMPLITUDE_PERMILLE * (uint32_t)demo_clamp_percent_u8(percent)) / 100u;
    if (value == 0u && percent != 0u) {
        value = 1u;
    }
    return demo_clamp_permille_u16((uint16_t)value);
}

static void demo_enter(demo_scene_state_t state)
{
    demo.state = state;
    demo.state_entered_ms = timebase_ms();
}

static uint8_t demo_rgb_scale(uint8_t value, uint8_t percent)
{
    return (uint8_t)(((uint16_t)value * (uint16_t)percent) / 100u);
}

static uint16_t demo_rgb_lfsr_next(uint16_t state)
{
    uint16_t lsb = (uint16_t)(state & 1u);
    state >>= 1;
    if (lsb) {
        state ^= 0xB400u;
    }
    return state == 0u ? 0xACE1u : state;
}

static rgb_color_t demo_rgb_wheel(uint8_t pos)
{
    rgb_color_t color;

    if (pos < 85u) {
        color.r = (uint8_t)(255u - (uint16_t)pos * 3u);
        color.g = (uint8_t)((uint16_t)pos * 3u);
        color.b = 0u;
    } else if (pos < 170u) {
        pos = (uint8_t)(pos - 85u);
        color.r = 0u;
        color.g = (uint8_t)(255u - (uint16_t)pos * 3u);
        color.b = (uint8_t)((uint16_t)pos * 3u);
    } else {
        pos = (uint8_t)(pos - 170u);
        color.r = (uint8_t)((uint16_t)pos * 3u);
        color.g = 0u;
        color.b = (uint8_t)(255u - (uint16_t)pos * 3u);
    }
    return color;
}

static void demo_rgb_idle(void)
{
    if (demo.rgb_enabled) {
        rgb_ws2812_clear();
    }
}

static void demo_update_rgb(uint32_t now_ms)
{
    rgb_color_t off = {0u, 0u, 0u};

    if (!demo.rgb_enabled || demo.state != DEMO_SCENE_RUNNING) {
        return;
    }
    if ((uint32_t)(now_ms - demo.last_rgb_ms) < 120u) {
        return;
    }
    demo.last_rgb_ms = now_ms;

    if ((int32_t)(now_ms - demo.hall_flash_until_ms) < 0) {
        rgb_color_t white = {255u, 255u, 255u};
        for (uint8_t i = 0; i < APP_RGB_LED_COUNT; ++i) {
            rgb_ws2812_set(i, white);
        }
        rgb_ws2812_show();
        return;
    }

    switch (demo.rgb_effect) {
    case DEMO_RGB_EFFECT_STARS:
        demo.rgb_lfsr = demo_rgb_lfsr_next(demo.rgb_lfsr);
        for (uint8_t i = 0; i < APP_RGB_LED_COUNT; ++i) {
            rgb_ws2812_set(i, off);
        }
        for (uint8_t spark = 0u; spark < 2u; ++spark) {
            rgb_color_t color;
            uint8_t index;

            demo.rgb_lfsr = demo_rgb_lfsr_next(demo.rgb_lfsr);
            index = (uint8_t)(demo.rgb_lfsr % APP_RGB_LED_COUNT);
            color.r = (demo.rgb_lfsr & 0x0008u) ? 255u : 80u;
            color.g = (demo.rgb_lfsr & 0x0010u) ? 255u : 120u;
            color.b = 255u;
            rgb_ws2812_set(index, color);
        }
        break;

    case DEMO_RGB_EFFECT_RAINBOW:
        for (uint8_t i = 0; i < APP_RGB_LED_COUNT; ++i) {
            rgb_color_t color = demo_rgb_wheel((uint8_t)(demo.rgb_index + (uint8_t)(i * 42u)));
            rgb_ws2812_set(i, color);
        }
        demo.rgb_index = (uint8_t)(demo.rgb_index + 5u);
        break;

    case DEMO_RGB_EFFECT_BREATHE: {
        uint8_t phase = demo.rgb_index;
        uint8_t level = phase < 128u ? (uint8_t)((uint16_t)phase * 100u / 127u) :
                                      (uint8_t)((uint16_t)(255u - phase) * 100u / 127u);
        rgb_color_t color = {
            demo_rgb_scale(0u, level),
            demo_rgb_scale(80u, level),
            demo_rgb_scale(255u, level),
        };
        for (uint8_t i = 0; i < APP_RGB_LED_COUNT; ++i) {
            rgb_ws2812_set(i, color);
        }
        demo.rgb_index = (uint8_t)(demo.rgb_index + 10u);
        break;
    }

    case DEMO_RGB_EFFECT_STROBE: {
        rgb_color_t white = {255u, 255u, 255u};
        rgb_color_t color = (demo.rgb_index & 1u) ? off : white;
        for (uint8_t i = 0; i < APP_RGB_LED_COUNT; ++i) {
            rgb_ws2812_set(i, color);
        }
        demo.rgb_index++;
        break;
    }

    case DEMO_RGB_EFFECT_CHRISTMAS:
        for (uint8_t i = 0; i < APP_RGB_LED_COUNT; ++i) {
            rgb_color_t color;
            if (((uint8_t)(i + demo.rgb_index) & 1u) == 0u) {
                color.r = 255u;
                color.g = 0u;
                color.b = 0u;
            } else {
                color.r = 0u;
                color.g = 255u;
                color.b = 0u;
            }
            rgb_ws2812_set(i, color);
        }
        demo.rgb_index++;
        break;

    case DEMO_RGB_EFFECT_CHASE:
    default: {
        rgb_color_t active = {0u, 0u, 0u};
        active.r = (uint8_t)((24u * demo.intensity_percent) / 100u);
        active.g = 0u;
        active.b = (uint8_t)(24u - active.r);
        for (uint8_t i = 0; i < APP_RGB_LED_COUNT; ++i) {
            rgb_ws2812_set(i, off);
        }
        rgb_ws2812_set(demo.rgb_index, active);
        demo.rgb_index++;
        if (demo.rgb_index >= APP_RGB_LED_COUNT) {
            demo.rgb_index = 0u;
        }
        break;
    }
    }
    rgb_ws2812_show();
}

static void demo_hall_poll(uint32_t now_ms)
{
    if (!demo.hall_enabled) {
        return;
    }
    for (uint8_t i = 0u; i < HALL_SENSOR_COUNT; ++i) {
        hall_state_t state = hall_get_state((hall_sensor_id_t)i);
        if (state.edge_count != demo.hall_edges[i]) {
            demo.hall_edges[i] = state.edge_count;
            demo.last_hall_edge_ms[i] = state.last_edge_ms;
            demo.hall_flash_until_ms = now_ms + APP_DEMO_HALL_RGB_FLASH_MS;
        }
    }
}

static uint32_t demo_abs_speed_hz_x1000(void)
{
    return demo.speed_hz_x1000 < 0 ?
        (uint32_t)(-demo.speed_hz_x1000) :
        (uint32_t)demo.speed_hz_x1000;
}

static int8_t demo_motion_direction(void)
{
    return demo.speed_hz_x1000 < 0 ? -1 : 1;
}

static uint8_t demo_motion_targets(void)
{
    return (uint8_t)(NORTHPOLE_MOTOR_WAVE_TARGET_A |
                     NORTHPOLE_MOTOR_WAVE_TARGET_B |
                     NORTHPOLE_MOTOR_WAVE_TARGET_G);
}

static int demo_apply_motion_tune(void)
{
    int rc = northpole_motor_wave_set_carrier_hz(APP_MOTOR_PWM_DEFAULT_HZ);
    if (rc != 0) {
        return rc;
    }
    return northpole_motor_wave_set_control_update_hz(APP_MOTOR_CONTROL_UPDATE_HZ);
}

static int demo_start_motor_wave(void)
{
    int rc = demo_apply_motion_tune();
    if (rc != 0) {
        return rc;
    }
    return northpole_motor_wave_start_smooth_ex(demo_abs_speed_hz_x1000(),
                                                demo.amplitude_permille,
                                                demo_motion_targets(),
                                                1u,
                                                demo_motion_direction(),
                                                demo.guard_mode,
                                                demo.guard_duty_permille);
}

static int demo_update_motor_wave(void)
{
    return northpole_motor_wave_update(demo_abs_speed_hz_x1000(),
                                       demo.amplitude_permille,
                                       demo_motion_direction(),
                                       demo.guard_mode,
                                       demo.guard_duty_permille);
}

static void demo_start_audio(void)
{
    audio_status_t volume_status;
    audio_status_t play_status;

    if (!demo.audio_enabled) {
        LOG_INFO("demo audio start skipped reason=audio-disabled\r\n");
        return;
    }
    volume_status = audio_wt2003_set_volume((uint8_t)demo.audio_volume);
    play_status = audio_wt2003_play(demo.audio_track);
    LOG_INFO("demo audio start volume=%u volume_status=%s track=%u play_status=%s expected=WT2003 playback after module latency\r\n",
             (unsigned)demo.audio_volume,
             audio_wt2003_status_name(volume_status),
             (unsigned)demo.audio_track,
             audio_wt2003_status_name(play_status));
}

static void demo_stop_audio(void)
{
    if (demo.audio_enabled) {
        (void)audio_wt2003_stop();
    }
}

static void demo_stop_outputs(demo_stop_reason_t reason)
{
    demo.stop_reason = reason;
    northpole_motor_wave_stop();
    motion_control_stop_immediate();
    motor_drv8837_off();
    demo_stop_audio();
    demo_rgb_idle();
}

static void demo_handle_run_short(void)
{
    if (demo.state == DEMO_SCENE_RUNNING ||
        demo.state == DEMO_SCENE_ARMING ||
        demo.state == DEMO_SCENE_START_AUDIO ||
        demo.state == DEMO_SCENE_START_RGB ||
        demo.state == DEMO_SCENE_START_MOTOR) {
        LOG_INFO("demo touch RUN action=stop expected=motor/audio/rgb stop state=%s\r\n",
                 demo_scene_state_name(demo.state));
        demo_scene_stop(DEMO_STOP_RUN_BUTTON);
    } else if (demo.state == DEMO_SCENE_FAULT) {
        LOG_INFO("demo touch RUN action=clear-fault-state expected=IDLE; run demo clear-faults if faults remain\r\n");
        demo_stop_outputs(DEMO_STOP_COMMAND);
        demo_enter(DEMO_SCENE_IDLE);
    } else {
        LOG_INFO("demo touch RUN action=start expected=audio track %u, RGB %s, motor speed=%ld guard=%s duty=%u\r\n",
                 (unsigned)demo.audio_track,
                 demo_scene_rgb_effect_name(demo.rgb_effect),
                 (long)demo.speed_hz_x1000,
                 northpole_motor_guard_mode_name(demo.guard_mode),
                 (unsigned)demo.guard_duty_permille);
        (void)demo_scene_start();
    }
}

static void demo_handle_touch_edges(uint32_t now_ms)
{
    for (uint8_t i = 0u; i < TOUCH_COUNT; ++i) {
        demo_touch_runtime_t *touch = &demo.touch[i];
        uint8_t raw = touch_get_state((touch_pad_id_t)i).raw ? 1u : 0u;

        if (raw != touch->raw) {
            if ((uint32_t)(now_ms - touch->last_edge_ms) < APP_DEMO_TOUCH_DEBOUNCE_MS) {
                LOG_INFO("demo touch %-6s edge raw=%u ignored=debounce dt_ms=%lu\r\n",
                         touch_name((touch_pad_id_t)i),
                         (unsigned)raw,
                         (unsigned long)(now_ms - touch->last_edge_ms));
                continue;
            }
            touch->last_edge_ms = now_ms;
            touch->raw = raw;
            if (raw) {
                touch->pressed_ms = now_ms;
                touch->long_reported = 0u;
                LOG_INFO("demo touch %-6s press state=%s\r\n",
                         touch_name((touch_pad_id_t)i),
                         demo_scene_state_name(demo.state));
            } else if (!touch->long_reported) {
                LOG_INFO("demo touch %-6s release duration_ms=%lu\r\n",
                         touch_name((touch_pad_id_t)i),
                         (unsigned long)(now_ms - touch->pressed_ms));
                switch ((touch_pad_id_t)i) {
                case TOUCH_RUN:
                    demo_handle_run_short();
                    break;
                case TOUCH_SPD_PLUS:
                    (void)demo_scene_adjust_speed(APP_MOTION_SPEED_STEP_HZ_X1000);
                    LOG_INFO("demo touch SPD+ action=speed-step delta=+%ld expected=faster-forward-or-slower-reverse speed_hz_x1000=%ld\r\n",
                             (long)APP_MOTION_SPEED_STEP_HZ_X1000,
                             (long)demo.speed_hz_x1000);
                    break;
                case TOUCH_SPD_MINUS:
                    (void)demo_scene_adjust_speed(-APP_MOTION_SPEED_STEP_HZ_X1000);
                    LOG_INFO("demo touch SPD- action=speed-step delta=-%ld expected=slower-forward-or-faster-reverse speed_hz_x1000=%ld\r\n",
                             (long)APP_MOTION_SPEED_STEP_HZ_X1000,
                             (long)demo.speed_hz_x1000);
                    break;
                case TOUCH_MUSIC:
                    if (demo.audio_enabled) {
                        if (demo.state == DEMO_SCENE_RUNNING) {
                            audio_status_t status = wt2003_next();
                            LOG_INFO("demo touch MUSIC action=audio-next enqueue_status=%s expected=WT2003 advances to next file after module latency\r\n",
                                     audio_wt2003_status_name(status));
                        } else {
                            audio_status_t status = audio_wt2003_play(demo.audio_track);
                            LOG_INFO("demo touch MUSIC action=audio-play-index track=%u enqueue_status=%s expected=WT2003 starts playback after module latency\r\n",
                                     (unsigned)demo.audio_track,
                                     audio_wt2003_status_name(status));
                        }
                    } else {
                        LOG_INFO("demo touch MUSIC action=ignored reason=audio-disabled\r\n");
                    }
                    break;
                default:
                    break;
                }
            }
        }

        if (raw && i == TOUCH_RUN && !touch->long_reported &&
            (uint32_t)(now_ms - touch->pressed_ms) >= APP_DEMO_RUN_LONG_PRESS_MS) {
            touch->long_reported = 1u;
            LOG_WARN("demo touch RUN long-press action=emergency-stop expected=FAULT motor/audio/rgb off\r\n");
            demo_scene_emergency_stop();
        }
    }
}

void demo_scene_init(void)
{
    if (demo.initialized) {
        return;
    }
    demo.initialized = 1u;
    demo.state = DEMO_SCENE_IDLE;
    demo.stop_reason = DEMO_STOP_NONE;
    demo.audio_enabled = 1u;
    demo.rgb_enabled = 1u;
    demo.hall_enabled = APP_DEMO_DEFAULT_HALL_ENABLE ? 1u : 0u;
    demo.intensity_percent = demo_clamp_percent_u8(APP_DEMO_DEFAULT_INTENSITY_PERCENT);
    demo.amplitude_permille = demo_intensity_to_amplitude(demo.intensity_percent);
    demo.guard_mode = (uint8_t)APP_DEMO_DEFAULT_GUARD_MODE;
    demo.guard_duty_permille = demo_clamp_permille_u16(APP_DEMO_DEFAULT_GUARD_DUTY_PERMILLE);
    demo.duration_ms = demo_clamp_duration(APP_DEMO_DEFAULT_DURATION_MS);
    demo.speed_hz_x1000 = demo_clamp_speed(APP_DEMO_DEFAULT_SPEED_HZ_X1000);
    demo.audio_track = APP_DEMO_DEFAULT_AUDIO_TRACK;
    demo.audio_volume = APP_DEMO_DEFAULT_AUDIO_VOLUME;
    if (demo.audio_volume > 31u) {
        demo.audio_volume = 31u;
    }
    demo.rgb_brightness = demo_clamp_rgb_brightness(APP_DEMO_DEFAULT_RGB_BRIGHTNESS);
    demo.rgb_effect = (demo_rgb_effect_t)APP_DEMO_DEFAULT_RGB_EFFECT;
    if (demo.rgb_effect > DEMO_RGB_EFFECT_CHRISTMAS) {
        demo.rgb_effect = DEMO_RGB_EFFECT_STARS;
    }
    demo.rgb_lfsr = 0xACE1u;
    rgb_ws2812_set_brightness(demo.rgb_brightness);
    demo_scene_reset_hall_counters();
}

int demo_scene_start(void)
{
    demo_scene_init();
    if (demo.state == DEMO_SCENE_FAULT) {
        LOG_WARN("demo start refused; clear FAULT with `demo clear-faults`\r\n");
        return -2;
    }
    if (fault_snapshot() != 0u) {
        demo.stop_reason = DEMO_STOP_FAULT;
        demo_enter(DEMO_SCENE_FAULT);
        LOG_WARN("demo start refused; faults=0x%08lx\r\n", (unsigned long)fault_snapshot());
        return -3;
    }
    demo.started_ms = 0u;
    demo.stopping_ms = 0u;
    demo.last_motor_rc = 0;
    demo.stop_reason = DEMO_STOP_NONE;
    demo_scene_reset_hall_counters();
    demo.rgb_index = 0u;
    demo.rgb_lfsr = 0xACE1u;
    demo_enter(DEMO_SCENE_ARMING);
    return 0;
}

void demo_scene_stop(demo_stop_reason_t reason)
{
    demo_scene_init();
    if (demo.state == DEMO_SCENE_IDLE) {
        demo_stop_outputs(reason);
        return;
    }
    if (demo.state == DEMO_SCENE_FAULT) {
        demo_stop_outputs(reason);
        demo_enter(DEMO_SCENE_IDLE);
        return;
    }
    demo.stop_reason = reason == DEMO_STOP_NONE ? DEMO_STOP_COMMAND : reason;
    demo.stopping_ms = timebase_ms();
    demo_stop_outputs(demo.stop_reason);
    demo_enter(DEMO_SCENE_IDLE);
    LOG_INFO("demo stopped reason=%s\r\n", demo_scene_stop_reason_name(demo.stop_reason));
}

void demo_scene_emergency_stop(void)
{
    demo_scene_init();
    demo_stop_outputs(DEMO_STOP_EMERGENCY);
    demo_enter(DEMO_SCENE_FAULT);
    LOG_WARN("demo emergency stop\r\n");
}

void demo_scene_clear_faults(void)
{
    demo_scene_init();
    fault_clear_all();
    if (demo.state == DEMO_SCENE_FAULT) {
        demo.stop_reason = DEMO_STOP_NONE;
        demo.last_motor_rc = 0;
        demo_enter(DEMO_SCENE_IDLE);
    }
}

int demo_scene_set_speed(int32_t speed_hz_x1000)
{
    demo_scene_init();
    demo.speed_hz_x1000 = demo_clamp_speed(speed_hz_x1000);
    if (demo.state == DEMO_SCENE_RUNNING) {
        return demo_update_motor_wave();
    }
    return 0;
}

int demo_scene_adjust_speed(int32_t delta_hz_x1000)
{
    int32_t next_speed;

    demo_scene_init();
    next_speed = demo.speed_hz_x1000 + delta_hz_x1000;
    if (demo.speed_hz_x1000 > 0 && delta_hz_x1000 < 0 && next_speed <= 0) {
        next_speed = -APP_MOTION_SPEED_STEP_HZ_X1000;
    } else if (demo.speed_hz_x1000 < 0 && delta_hz_x1000 > 0 && next_speed >= 0) {
        next_speed = APP_MOTION_SPEED_STEP_HZ_X1000;
    }
    return demo_scene_set_speed(next_speed);
}

int demo_scene_set_duration(uint32_t duration_ms)
{
    demo_scene_init();
    demo.duration_ms = demo_clamp_duration(duration_ms);
    return 0;
}

int demo_scene_set_intensity(uint8_t percent)
{
    demo_scene_init();
    demo.intensity_percent = demo_clamp_percent_u8(percent);
    demo.amplitude_permille = demo_intensity_to_amplitude(demo.intensity_percent);
    if (demo.state == DEMO_SCENE_RUNNING) {
        return demo_update_motor_wave();
    }
    return 0;
}

int demo_scene_set_audio_enabled(uint8_t enabled)
{
    demo_scene_init();
    demo.audio_enabled = enabled ? 1u : 0u;
    if (!demo.audio_enabled) {
        demo_stop_audio();
    }
    return 0;
}

int demo_scene_set_audio_track(uint16_t track)
{
    demo_scene_init();
    demo.audio_track = track == 0u ? 1u : track;
    return 0;
}

int demo_scene_set_audio_volume(uint8_t volume)
{
    demo_scene_init();
    demo.audio_volume = volume > 31u ? 31u : volume;
    if (demo.audio_enabled) {
        return audio_wt2003_set_volume((uint8_t)demo.audio_volume);
    }
    return 0;
}

int demo_scene_set_rgb_enabled(uint8_t enabled)
{
    demo_scene_init();
    demo.rgb_enabled = enabled ? 1u : 0u;
    if (!demo.rgb_enabled) {
        rgb_ws2812_clear();
    }
    return 0;
}

int demo_scene_set_rgb_brightness(uint8_t brightness)
{
    demo_scene_init();
    demo.rgb_brightness = demo_clamp_rgb_brightness(brightness);
    rgb_ws2812_set_brightness(demo.rgb_brightness);
    return 0;
}

int demo_scene_set_rgb_effect(demo_rgb_effect_t effect)
{
    demo_scene_init();
    if (effect > DEMO_RGB_EFFECT_CHRISTMAS) {
        return -1;
    }
    demo.rgb_effect = effect;
    demo.rgb_index = 0u;
    demo.rgb_lfsr = 0xACE1u;
    return 0;
}

int demo_scene_set_hall_enabled(uint8_t enabled)
{
    demo_scene_init();
    demo.hall_enabled = enabled ? 1u : 0u;
    demo_scene_reset_hall_counters();
    return 0;
}

void demo_scene_reset_hall_counters(void)
{
    for (uint8_t i = 0u; i < HALL_SENSOR_COUNT; ++i) {
        hall_state_t state = hall_get_state((hall_sensor_id_t)i);
        demo.hall_edges[i] = state.edge_count;
        demo.last_hall_edge_ms[i] = state.last_edge_ms;
    }
}

void demo_scene_poll(void)
{
    uint32_t now_ms;

    demo_scene_init();
    now_ms = timebase_ms();
    demo_handle_touch_edges(now_ms);
    demo_hall_poll(now_ms);
    demo_update_rgb(now_ms);

    switch (demo.state) {
    case DEMO_SCENE_IDLE:
    case DEMO_SCENE_FAULT:
        return;

    case DEMO_SCENE_ARMING:
        motor_drv8837_arm(demo.duration_ms + APP_MOTION_RAMP_MS_STOP + 1000u);
        demo_enter(DEMO_SCENE_START_AUDIO);
        break;

    case DEMO_SCENE_START_AUDIO:
        demo_start_audio();
        demo_enter(DEMO_SCENE_START_RGB);
        break;

    case DEMO_SCENE_START_RGB:
        rgb_ws2812_set_brightness(demo.rgb_brightness);
        demo_rgb_idle();
        demo.rgb_index = 0u;
        demo.last_rgb_ms = now_ms - 120u;
        demo_enter(DEMO_SCENE_START_MOTOR);
        break;

    case DEMO_SCENE_START_MOTOR:
        demo.last_motor_rc = demo_start_motor_wave();
        if (demo.last_motor_rc != 0) {
            demo_stop_outputs(DEMO_STOP_FAULT);
            demo_enter(DEMO_SCENE_FAULT);
            LOG_WARN("demo motor start failed rc=%d\r\n", demo.last_motor_rc);
            return;
        }
        demo.started_ms = now_ms;
        demo_enter(DEMO_SCENE_RUNNING);
        LOG_INFO("demo running speed_hz_x1000=%ld amplitude=%u guard=%s duty=%u duration_ms=%lu\r\n",
                 (long)demo.speed_hz_x1000,
                 (unsigned)demo.amplitude_permille,
                 northpole_motor_guard_mode_name(demo.guard_mode),
                 (unsigned)demo.guard_duty_permille,
                 (unsigned long)demo.duration_ms);
        break;

    case DEMO_SCENE_RUNNING:
        if (demo.duration_ms != 0u && (uint32_t)(now_ms - demo.started_ms) >= demo.duration_ms) {
            demo_scene_stop(DEMO_STOP_TIMEOUT);
        }
        break;

    case DEMO_SCENE_STOPPING:
        if ((uint32_t)(now_ms - demo.state_entered_ms) >= (uint32_t)(APP_MOTION_RAMP_MS_STOP + 100u)) {
            demo_stop_outputs(demo.stop_reason);
            demo_enter(DEMO_SCENE_IDLE);
            LOG_INFO("demo stopped reason=%s\r\n", demo_scene_stop_reason_name(demo.stop_reason));
        }
        break;

    default:
        demo_stop_outputs(DEMO_STOP_FAULT);
        demo_enter(DEMO_SCENE_FAULT);
        break;
    }
}

void demo_scene_status(demo_scene_status_t *status)
{
    uint32_t now_ms;

    if (status == NULL) {
        return;
    }
    memset(status, 0, sizeof(*status));
    demo_scene_init();
    now_ms = timebase_ms();
    status->enabled = APP_DEMO_SCENE_ENABLE ? 1u : 0u;
    status->state = demo.state;
    status->stop_reason = demo.stop_reason;
    status->usb_power_only = APP_DEMO_USB_POWER_ONLY ? 1u : 0u;
    status->audio_enabled = demo.audio_enabled;
    status->rgb_enabled = demo.rgb_enabled;
    status->hall_enabled = demo.hall_enabled;
    status->intensity_percent = demo.intensity_percent;
    status->rgb_brightness = demo.rgb_brightness;
    status->guard_mode = demo.guard_mode;
    status->rgb_effect = demo.rgb_effect;
    status->audio_track = demo.audio_track;
    status->audio_volume = demo.audio_volume;
    status->amplitude_permille = demo.amplitude_permille;
    status->guard_duty_permille = demo.guard_duty_permille;
    status->duration_ms = demo.duration_ms;
    status->elapsed_ms = (demo.started_ms != 0u &&
                          (demo.state == DEMO_SCENE_RUNNING || demo.state == DEMO_SCENE_STOPPING)) ?
        (uint32_t)(now_ms - demo.started_ms) :
        0u;
    status->remaining_ms = status->elapsed_ms < demo.duration_ms ?
        demo.duration_ms - status->elapsed_ms :
        0u;
    status->fault_mask = fault_snapshot();
    for (uint8_t i = 0u; i < HALL_SENSOR_COUNT; ++i) {
        status->hall_edges[i] = demo.hall_edges[i];
        status->last_hall_edge_ms[i] = demo.last_hall_edge_ms[i];
    }
    status->speed_hz_x1000 = demo.speed_hz_x1000;
    status->last_motor_rc = demo.last_motor_rc;
}

const char *demo_scene_state_name(demo_scene_state_t state)
{
    switch (state) {
    case DEMO_SCENE_IDLE: return "IDLE";
    case DEMO_SCENE_ARMING: return "ARMING";
    case DEMO_SCENE_START_AUDIO: return "START_AUDIO";
    case DEMO_SCENE_START_RGB: return "START_RGB";
    case DEMO_SCENE_START_MOTOR: return "START_MOTOR";
    case DEMO_SCENE_RUNNING: return "RUNNING";
    case DEMO_SCENE_STOPPING: return "STOPPING";
    case DEMO_SCENE_FAULT: return "FAULT";
    default: return "?";
    }
}

const char *demo_scene_stop_reason_name(demo_stop_reason_t reason)
{
    switch (reason) {
    case DEMO_STOP_NONE: return "none";
    case DEMO_STOP_COMMAND: return "command";
    case DEMO_STOP_TIMEOUT: return "timeout";
    case DEMO_STOP_RUN_BUTTON: return "run-button";
    case DEMO_STOP_EMERGENCY: return "emergency";
    case DEMO_STOP_FAULT: return "fault";
    default: return "?";
    }
}

const char *demo_scene_rgb_effect_name(demo_rgb_effect_t effect)
{
    switch (effect) {
    case DEMO_RGB_EFFECT_CHASE: return "chase";
    case DEMO_RGB_EFFECT_STARS: return "stars";
    case DEMO_RGB_EFFECT_RAINBOW: return "rainbow";
    case DEMO_RGB_EFFECT_BREATHE: return "breathe";
    case DEMO_RGB_EFFECT_STROBE: return "strobe";
    case DEMO_RGB_EFFECT_CHRISTMAS: return "christmas";
    default: return "?";
    }
}

#else

void demo_scene_init(void) {}
void demo_scene_poll(void) {}
int demo_scene_start(void) { return -1; }
void demo_scene_stop(demo_stop_reason_t reason) { (void)reason; }
void demo_scene_emergency_stop(void) {}
void demo_scene_clear_faults(void) {}
int demo_scene_set_speed(int32_t speed_hz_x1000) { (void)speed_hz_x1000; return -1; }
int demo_scene_adjust_speed(int32_t delta_hz_x1000) { (void)delta_hz_x1000; return -1; }
int demo_scene_set_duration(uint32_t duration_ms) { (void)duration_ms; return -1; }
int demo_scene_set_intensity(uint8_t percent) { (void)percent; return -1; }
int demo_scene_set_audio_enabled(uint8_t enabled) { (void)enabled; return -1; }
int demo_scene_set_audio_track(uint16_t track) { (void)track; return -1; }
int demo_scene_set_audio_volume(uint8_t volume) { (void)volume; return -1; }
int demo_scene_set_rgb_enabled(uint8_t enabled) { (void)enabled; return -1; }
int demo_scene_set_rgb_brightness(uint8_t brightness) { (void)brightness; return -1; }
int demo_scene_set_rgb_effect(demo_rgb_effect_t effect) { (void)effect; return -1; }
int demo_scene_set_hall_enabled(uint8_t enabled) { (void)enabled; return -1; }
void demo_scene_reset_hall_counters(void) {}
void demo_scene_status(demo_scene_status_t *status)
{
    if (status != NULL) {
        memset(status, 0, sizeof(*status));
        status->enabled = 0u;
    }
}
const char *demo_scene_state_name(demo_scene_state_t state) { (void)state; return "disabled"; }
const char *demo_scene_stop_reason_name(demo_stop_reason_t reason) { (void)reason; return "disabled"; }
const char *demo_scene_rgb_effect_name(demo_rgb_effect_t effect) { (void)effect; return "disabled"; }

#endif
