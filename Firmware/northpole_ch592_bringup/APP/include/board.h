#ifndef BOARD_H
#define BOARD_H

#include <stddef.h>
#include <stdint.h>

#define BOARD_MOTOR_SLEEP_ACTIVE_HIGH 1u

typedef enum {
    BOARD_PIN_SAFE_UNUSED = 0,
    BOARD_PIN_SAFE_INPUT,
    BOARD_PIN_SAFE_INPUT_PULLUP,
    BOARD_PIN_SAFE_INPUT_PULLDOWN,
    BOARD_PIN_SAFE_OUTPUT_LOW,
    BOARD_PIN_SAFE_OUTPUT_HIGH,
    BOARD_PIN_SAFE_ANALOG,
    BOARD_PIN_SAFE_ALTERNATE,
    BOARD_PIN_SAFE_POWER,
    BOARD_PIN_SAFE_RF,
} board_pin_safe_state_t;

typedef enum {
    BOARD_PIN_DIR_UNUSED = 0,
    BOARD_PIN_DIR_INPUT,
    BOARD_PIN_DIR_OUTPUT,
    BOARD_PIN_DIR_BIDIRECTIONAL,
    BOARD_PIN_DIR_ANALOG,
    BOARD_PIN_DIR_ALTERNATE,
    BOARD_PIN_DIR_POWER,
    BOARD_PIN_DIR_RF,
} board_pin_direction_t;

typedef enum {
    BOARD_PIN_ACTIVE_NONE = 0,
    BOARD_PIN_ACTIVE_HIGH,
    BOARD_PIN_ACTIVE_LOW,
    BOARD_PIN_ACTIVE_PWM_HIGH,
    BOARD_PIN_ACTIVE_ANALOG_DELTA,
} board_pin_active_t;

typedef enum {
    BOARD_OUTPUT_PWM_A1 = 0,
    BOARD_OUTPUT_PWM_A2,
    BOARD_OUTPUT_PWM_B1,
    BOARD_OUTPUT_PWM_B2,
    BOARD_OUTPUT_PWM_G1,
    BOARD_OUTPUT_PWM_G2,
    BOARD_OUTPUT_RGB_DATA,
    BOARD_OUTPUT_MOTOR_SLEEP,
    BOARD_OUTPUT_COUNT,
} board_output_id_t;

typedef enum {
    BOARD_INPUT_HALL1 = 0,
    BOARD_INPUT_HALL2,
    BOARD_INPUT_TOUCH_SPD_MINUS,
    BOARD_INPUT_TOUCH_RUN,
    BOARD_INPUT_TOUCH_SPD_PLUS,
    BOARD_INPUT_TOUCH_MUSIC,
    BOARD_INPUT_AUDIO_BUSY,
    BOARD_INPUT_IP5209_INT,
    BOARD_INPUT_COUNT,
} board_input_id_t;

typedef struct {
    uint8_t u2_pad;
    const char *pin_function;
    const char *net;
    const char *role;
    board_pin_safe_state_t safe_state;
    board_pin_direction_t direction;
    board_pin_active_t active;
} board_pin_t;

void board_init_safe_pins(void);
void board_print_pin_map(void);

const board_pin_t *const *board_all_pins(size_t *count);
const board_pin_t *board_output_pin(board_output_id_t output);
const board_pin_t *board_input_pin(board_input_id_t input);

void board_output_write(board_output_id_t output, uint8_t high);
uint8_t board_output_last_state(board_output_id_t output);
uint8_t board_output_read(board_output_id_t output);
uint8_t board_input_read(board_input_id_t input);

const char *board_safe_state_name(board_pin_safe_state_t state);
const char *board_direction_name(board_pin_direction_t direction);
const char *board_active_name(board_pin_active_t active);

#endif /* BOARD_H */
