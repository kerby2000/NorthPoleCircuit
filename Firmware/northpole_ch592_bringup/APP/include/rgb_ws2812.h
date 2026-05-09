#ifndef RGB_WS2812_H
#define RGB_WS2812_H

#include <stdint.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

void rgb_ws2812_init(void);
void rgb_ws2812_set_brightness(uint8_t brightness);
uint8_t rgb_ws2812_get_brightness(void);
void rgb_ws2812_set(uint8_t index, rgb_color_t color);
void rgb_ws2812_clear(void);
void rgb_ws2812_show(void);
rgb_color_t rgb_ws2812_get(uint8_t index);

#endif /* RGB_WS2812_H */
