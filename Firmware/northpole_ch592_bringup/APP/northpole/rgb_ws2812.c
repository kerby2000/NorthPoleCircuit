#include "rgb_ws2812.h"

#include "app_config.h"
#include "board.h"

#if defined(__GNUC__)
#define FW_WEAK __attribute__((weak))
#else
#define FW_WEAK
#endif

static rgb_color_t pixels[APP_RGB_LED_COUNT];
static uint8_t brightness = APP_RGB_DEFAULT_BRIGHTNESS;

FW_WEAK int rgb_ws2812_platform_write(const rgb_color_t *colors, uint8_t count, uint8_t global_brightness)
{
    (void)colors;
    (void)count;
    (void)global_brightness;
    return 0;
}

FW_WEAK void rgb_ws2812_platform_idle_low(void)
{
}

FW_WEAK const char *rgb_ws2812_platform_backend_name(void)
{
    return "stub";
}

FW_WEAK int rgb_ws2812_platform_diag_pa14_level(uint8_t high, uint32_t duration_ms)
{
    (void)high;
    (void)duration_ms;
    return -1;
}

FW_WEAK int rgb_ws2812_platform_diag_pa14_square(uint32_t hz, uint32_t duration_ms)
{
    (void)hz;
    (void)duration_ms;
    return -1;
}

void rgb_ws2812_init(void)
{
    rgb_ws2812_platform_idle_low();
    rgb_ws2812_clear();
}

void rgb_ws2812_set_brightness(uint8_t value)
{
    brightness = value > APP_RGB_BRINGUP_BRIGHTNESS_LIMIT ? APP_RGB_BRINGUP_BRIGHTNESS_LIMIT : value;
}

uint8_t rgb_ws2812_get_brightness(void)
{
    return brightness;
}

void rgb_ws2812_set(uint8_t index, rgb_color_t color)
{
    if (index < APP_RGB_LED_COUNT) {
        pixels[index] = color;
    }
}

void rgb_ws2812_clear(void)
{
    for (uint8_t i = 0; i < APP_RGB_LED_COUNT; ++i) {
        pixels[i].r = 0;
        pixels[i].g = 0;
        pixels[i].b = 0;
    }
    rgb_ws2812_show();
}

void rgb_ws2812_show(void)
{
    (void)rgb_ws2812_platform_write(pixels, APP_RGB_LED_COUNT, brightness);
    rgb_ws2812_platform_idle_low();
}

void rgb_ws2812_force_idle_low(void)
{
    rgb_ws2812_platform_idle_low();
}

const char *rgb_ws2812_backend_name(void)
{
    return rgb_ws2812_platform_backend_name();
}

rgb_color_t rgb_ws2812_get(uint8_t index)
{
    rgb_color_t empty = {0, 0, 0};
    return index < APP_RGB_LED_COUNT ? pixels[index] : empty;
}

int rgb_ws2812_diag_pa14_level(uint8_t high, uint32_t duration_ms)
{
    return rgb_ws2812_platform_diag_pa14_level(high, duration_ms);
}

int rgb_ws2812_diag_pa14_square(uint32_t hz, uint32_t duration_ms)
{
    return rgb_ws2812_platform_diag_pa14_square(hz, duration_ms);
}
