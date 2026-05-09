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

void rgb_ws2812_init(void)
{
    board_output_write(BOARD_OUTPUT_RGB_DATA, 0);
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
    board_output_write(BOARD_OUTPUT_RGB_DATA, 0);
}

rgb_color_t rgb_ws2812_get(uint8_t index)
{
    rgb_color_t empty = {0, 0, 0};
    return index < APP_RGB_LED_COUNT ? pixels[index] : empty;
}
