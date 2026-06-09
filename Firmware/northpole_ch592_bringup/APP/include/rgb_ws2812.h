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
void rgb_ws2812_force_idle_low(void);
const char *rgb_ws2812_backend_name(void);
rgb_color_t rgb_ws2812_get(uint8_t index);
int rgb_ws2812_diag_pa14_level(uint8_t high, uint32_t duration_ms);
int rgb_ws2812_diag_pa14_square(uint32_t hz, uint32_t duration_ms);

#endif /* RGB_WS2812_H */
