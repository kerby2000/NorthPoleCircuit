#ifndef TOUCH_H
#define TOUCH_H

#include <stdint.h>

typedef enum {
    TOUCH_SPD_MINUS = 0,
    TOUCH_RUN,
    TOUCH_SPD_PLUS,
    TOUCH_MUSIC,
    TOUCH_COUNT,
} touch_pad_id_t;

typedef struct {
    uint16_t raw;
    uint16_t baseline;
    uint16_t threshold;
    uint8_t pressed;
} touch_state_t;

void touch_init(void);
void touch_poll(void);
uint16_t touch_read_raw(touch_pad_id_t pad);
touch_state_t touch_get_state(touch_pad_id_t pad);
const char *touch_name(touch_pad_id_t pad);

#endif /* TOUCH_H */
