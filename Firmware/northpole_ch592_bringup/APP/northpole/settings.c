#include "settings.h"

#include "app_config.h"
#include "fault.h"

#include <stddef.h>
#include <string.h>

#if defined(__GNUC__)
#define FW_WEAK __attribute__((weak))
#else
#define FW_WEAK
#endif

static settings_t active_settings;
static uint8_t active_settings_valid;

FW_WEAK int settings_platform_load(settings_t *settings)
{
    (void)settings;
    return -1;
}

FW_WEAK int settings_platform_save(const settings_t *settings)
{
    (void)settings;
    return -1;
}

uint16_t settings_crc16(const settings_t *settings)
{
    const uint8_t *bytes = (const uint8_t *)settings;
    size_t len = offsetof(settings_t, crc);
    uint16_t crc = 0xFFFFu;

    while (len--) {
        crc ^= (uint16_t)(*bytes++) << 8;
        for (uint8_t bit = 0; bit < 8u; ++bit) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static void finalize(settings_t *settings)
{
    settings->reserved = 0;
    settings->crc = settings_crc16(settings);
}

static uint8_t validate_ranges(const settings_t *settings)
{
    if (settings->version != APP_SETTINGS_VERSION) {
        return 0;
    }
    if (settings->volume > 30u) {
        return 0;
    }
    if (settings->brightness > APP_RGB_BRINGUP_BRIGHTNESS_LIMIT) {
        return 0;
    }
    if (settings->motor_intensity_limit > APP_BRINGUP_MOTOR_DUTY_LIMIT_PERMILLE) {
        return 0;
    }
    if (settings->demo_mode > 1u) {
        return 0;
    }
    return settings->crc == settings_crc16(settings);
}

void settings_init(void)
{
    settings_factory_reset();

#if APP_SETTINGS_FLASH_ENABLE
    {
        settings_t loaded;
        if (settings_platform_load(&loaded) == 0 && validate_ranges(&loaded)) {
            settings_set(&loaded);
            return;
        }
        fault_raise(FAULT_SETTINGS_INVALID);
    }
#endif
}

const settings_t *settings_get(void)
{
    return &active_settings;
}

void settings_set(const settings_t *settings)
{
    if (!settings || !validate_ranges(settings)) {
        fault_raise(FAULT_SETTINGS_INVALID);
        settings_factory_reset();
        return;
    }
    memcpy(&active_settings, settings, sizeof(active_settings));
    active_settings_valid = 1;
}

int settings_save(void)
{
    if (!settings_valid()) {
        fault_raise(FAULT_SETTINGS_INVALID);
        return -1;
    }

#if APP_SETTINGS_FLASH_ENABLE
    return settings_platform_save(&active_settings);
#else
    return 0;
#endif
}

void settings_factory_reset(void)
{
    active_settings.version = APP_SETTINGS_VERSION;
    active_settings.volume = APP_AUDIO_DEFAULT_VOLUME;
    active_settings.brightness = APP_RGB_DEFAULT_BRIGHTNESS;
    active_settings.default_scene = APP_DEFAULT_SCENE;
    active_settings.motor_intensity_limit = APP_MOTOR_INTENSITY_LIMIT_DEFAULT;
    active_settings.demo_mode = 0;
    finalize(&active_settings);
    active_settings_valid = 1;
}

void settings_corrupt_for_test(void)
{
    active_settings.crc ^= 0x5A5Au;
    active_settings_valid = 0;
    fault_raise(FAULT_SETTINGS_INVALID);
}

uint8_t settings_valid(void)
{
    return active_settings_valid && validate_ranges(&active_settings);
}
