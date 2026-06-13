#ifndef DEMO_SCENE_H
#define DEMO_SCENE_H

#include <stdint.h>

typedef enum {
    DEMO_SCENE_IDLE = 0,
    DEMO_SCENE_ARMING,
    DEMO_SCENE_START_AUDIO,
    DEMO_SCENE_START_RGB,
    DEMO_SCENE_START_MOTOR,
    DEMO_SCENE_RUNNING,
    DEMO_SCENE_STOPPING,
    DEMO_SCENE_FAULT,
} demo_scene_state_t;

typedef enum {
    DEMO_STOP_NONE = 0,
    DEMO_STOP_COMMAND,
    DEMO_STOP_TIMEOUT,
    DEMO_STOP_RUN_BUTTON,
    DEMO_STOP_EMERGENCY,
    DEMO_STOP_FAULT,
} demo_stop_reason_t;

typedef enum {
    DEMO_RGB_EFFECT_CHASE = 0,
    DEMO_RGB_EFFECT_STARS,
    DEMO_RGB_EFFECT_RAINBOW,
    DEMO_RGB_EFFECT_BREATHE,
    DEMO_RGB_EFFECT_STROBE,
    DEMO_RGB_EFFECT_CHRISTMAS,
} demo_rgb_effect_t;

typedef struct {
    uint8_t enabled;
    demo_scene_state_t state;
    demo_stop_reason_t stop_reason;
    uint8_t usb_power_only;
    uint8_t audio_enabled;
    uint8_t rgb_enabled;
    uint8_t hall_enabled;
    uint8_t intensity_percent;
    uint8_t rgb_brightness;
    uint8_t guard_mode;
    demo_rgb_effect_t rgb_effect;
    uint16_t audio_track;
    uint16_t audio_volume;
    uint16_t amplitude_permille;
    uint16_t guard_duty_permille;
    uint32_t duration_ms;
    uint32_t elapsed_ms;
    uint32_t remaining_ms;
    uint32_t fault_mask;
    uint32_t hall_edges[2];
    uint32_t last_hall_edge_ms[2];
    int32_t speed_hz_x1000;
    int last_motor_rc;
} demo_scene_status_t;

void demo_scene_init(void);
void demo_scene_poll(void);
int demo_scene_start(void);
void demo_scene_stop(demo_stop_reason_t reason);
void demo_scene_emergency_stop(void);
void demo_scene_clear_faults(void);
int demo_scene_set_speed(int32_t speed_hz_x1000);
int demo_scene_adjust_speed(int32_t delta_hz_x1000);
int demo_scene_set_duration(uint32_t duration_ms);
int demo_scene_set_intensity(uint8_t percent);
int demo_scene_set_audio_enabled(uint8_t enabled);
int demo_scene_set_audio_track(uint16_t track);
int demo_scene_set_audio_volume(uint8_t volume);
int demo_scene_set_rgb_enabled(uint8_t enabled);
int demo_scene_set_rgb_brightness(uint8_t brightness);
int demo_scene_set_rgb_effect(demo_rgb_effect_t effect);
int demo_scene_set_hall_enabled(uint8_t enabled);
void demo_scene_reset_hall_counters(void);
void demo_scene_status(demo_scene_status_t *status);
const char *demo_scene_state_name(demo_scene_state_t state);
const char *demo_scene_stop_reason_name(demo_stop_reason_t reason);
const char *demo_scene_rgb_effect_name(demo_rgb_effect_t effect);

#endif /* DEMO_SCENE_H */
