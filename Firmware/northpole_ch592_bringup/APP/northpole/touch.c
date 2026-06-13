#include "touch.h"

#include "board.h"

#if defined(__GNUC__)
#define FW_WEAK __attribute__((weak))
#else
#define FW_WEAK
#endif

static touch_state_t touch_states[TOUCH_COUNT];

static const board_input_id_t touch_inputs[TOUCH_COUNT] = {
    BOARD_INPUT_TOUCH_SPD_MINUS,
    BOARD_INPUT_TOUCH_RUN,
    BOARD_INPUT_TOUCH_SPD_PLUS,
    BOARD_INPUT_TOUCH_MUSIC,
};

FW_WEAK uint16_t touch_platform_measure(const board_pin_t *pin)
{
    (void)pin;
    return 0;
}

void touch_init(void)
{
    for (uint8_t i = 0; i < TOUCH_COUNT; ++i) {
        touch_states[i].raw = 0;
        touch_states[i].baseline = 0;
        touch_states[i].threshold = 0;
        touch_states[i].pressed = 0;
    }
}

void touch_poll(void)
{
    for (uint8_t i = 0; i < TOUCH_COUNT; ++i) {
        uint16_t raw = touch_read_raw((touch_pad_id_t)i);
        touch_states[i].raw = raw;
        if (touch_states[i].baseline == 0) {
            touch_states[i].baseline = raw;
        }
        touch_states[i].pressed = touch_states[i].threshold > 0 ?
            (uint8_t)(raw > touch_states[i].threshold) :
            (uint8_t)(raw != 0u);
    }
}

uint16_t touch_read_raw(touch_pad_id_t pad)
{
    if (pad >= TOUCH_COUNT) {
        return 0;
    }
    return touch_platform_measure(board_input_pin(touch_inputs[pad]));
}

touch_state_t touch_get_state(touch_pad_id_t pad)
{
    touch_state_t empty = {0, 0, 0, 0};
    return pad < TOUCH_COUNT ? touch_states[pad] : empty;
}

const char *touch_name(touch_pad_id_t pad)
{
    switch (pad) {
    case TOUCH_SPD_MINUS: return "spd-";
    case TOUCH_RUN: return "run";
    case TOUCH_SPD_PLUS: return "spd+";
    case TOUCH_MUSIC: return "music";
    default: return "?";
    }
}
