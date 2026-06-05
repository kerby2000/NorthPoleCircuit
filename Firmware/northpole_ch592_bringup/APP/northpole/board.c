#include "board_pins.h"
#include "log.h"

#include <stddef.h>

#if defined(__GNUC__)
#define FW_WEAK __attribute__((weak))
#else
#define FW_WEAK
#endif

const board_pin_t BOARD_PIN_PWM_A1 = {32, BOARD_U2_PAD_32_FUNCTION, BOARD_U2_PAD_32_NET, "DRV8837 A IN1", BOARD_PIN_SAFE_OUTPUT_LOW, BOARD_PIN_DIR_OUTPUT, BOARD_PIN_ACTIVE_PWM_HIGH};
const board_pin_t BOARD_PIN_PWM_A2 = {1, BOARD_U2_PAD_1_FUNCTION, BOARD_U2_PAD_1_NET, "DRV8837 A IN2", BOARD_PIN_SAFE_OUTPUT_LOW, BOARD_PIN_DIR_OUTPUT, BOARD_PIN_ACTIVE_PWM_HIGH};
const board_pin_t BOARD_PIN_PWM_B1 = {17, BOARD_U2_PAD_17_FUNCTION, BOARD_U2_PAD_17_NET, "DRV8837 B IN1", BOARD_PIN_SAFE_OUTPUT_LOW, BOARD_PIN_DIR_OUTPUT, BOARD_PIN_ACTIVE_PWM_HIGH};
const board_pin_t BOARD_PIN_PWM_B2 = {18, BOARD_U2_PAD_18_FUNCTION, BOARD_U2_PAD_18_NET, "DRV8837 B IN2", BOARD_PIN_SAFE_OUTPUT_LOW, BOARD_PIN_DIR_OUTPUT, BOARD_PIN_ACTIVE_PWM_HIGH};
const board_pin_t BOARD_PIN_PWM_G1 = {30, BOARD_U2_PAD_30_FUNCTION, BOARD_U2_PAD_30_NET, "DRV8837 G IN1", BOARD_PIN_SAFE_OUTPUT_LOW, BOARD_PIN_DIR_OUTPUT, BOARD_PIN_ACTIVE_PWM_HIGH};
const board_pin_t BOARD_PIN_PWM_G2 = {31, BOARD_U2_PAD_31_FUNCTION, BOARD_U2_PAD_31_NET, "DRV8837 G IN2", BOARD_PIN_SAFE_OUTPUT_LOW, BOARD_PIN_DIR_OUTPUT, BOARD_PIN_ACTIVE_PWM_HIGH};
const board_pin_t BOARD_PIN_RGB_DATA = {28, BOARD_U2_PAD_28_FUNCTION, BOARD_U2_PAD_28_NET, "WS2812 data", BOARD_PIN_SAFE_OUTPUT_LOW, BOARD_PIN_DIR_OUTPUT, BOARD_PIN_ACTIVE_HIGH};
const board_pin_t BOARD_PIN_MOTOR_SLEEP = {3, BOARD_U2_PAD_3_FUNCTION, BOARD_U2_PAD_3_NET, "DRV8837 global nSLEEP", BOARD_PIN_SAFE_OUTPUT_LOW, BOARD_PIN_DIR_OUTPUT, BOARD_PIN_ACTIVE_HIGH};
const board_pin_t BOARD_PIN_AUDIO_BUSY = {29, BOARD_U2_PAD_29_FUNCTION, BOARD_U2_PAD_29_NET, "WT2003 BUSY input", BOARD_PIN_SAFE_INPUT, BOARD_PIN_DIR_INPUT, BOARD_PIN_ACTIVE_HIGH};
const board_pin_t BOARD_PIN_HALL1 = {2, BOARD_U2_PAD_2_FUNCTION, BOARD_U2_PAD_2_NET, "Hall 1 input", BOARD_PIN_SAFE_INPUT, BOARD_PIN_DIR_INPUT, BOARD_PIN_ACTIVE_LOW};
const board_pin_t BOARD_PIN_HALL2 = {26, BOARD_U2_PAD_26_FUNCTION, BOARD_U2_PAD_26_NET, "Hall 2 input", BOARD_PIN_SAFE_INPUT, BOARD_PIN_DIR_INPUT, BOARD_PIN_ACTIVE_LOW};
const board_pin_t BOARD_PIN_TOUCH_SPD_MINUS = {7, BOARD_U2_PAD_7_FUNCTION, BOARD_U2_PAD_7_NET, "Touch SPD-", BOARD_PIN_SAFE_ANALOG, BOARD_PIN_DIR_ANALOG, BOARD_PIN_ACTIVE_ANALOG_DELTA};
const board_pin_t BOARD_PIN_TOUCH_RUN = {8, BOARD_U2_PAD_8_FUNCTION, BOARD_U2_PAD_8_NET, "Touch RUN", BOARD_PIN_SAFE_ANALOG, BOARD_PIN_DIR_ANALOG, BOARD_PIN_ACTIVE_ANALOG_DELTA};
const board_pin_t BOARD_PIN_TOUCH_SPD_PLUS = {9, BOARD_U2_PAD_9_FUNCTION, BOARD_U2_PAD_9_NET, "Touch SPD+", BOARD_PIN_SAFE_ANALOG, BOARD_PIN_DIR_ANALOG, BOARD_PIN_ACTIVE_ANALOG_DELTA};
const board_pin_t BOARD_PIN_TOUCH_MUSIC = {27, BOARD_U2_PAD_27_FUNCTION, BOARD_U2_PAD_27_NET, "Touch MUSIC", BOARD_PIN_SAFE_ANALOG, BOARD_PIN_DIR_ANALOG, BOARD_PIN_ACTIVE_ANALOG_DELTA};
const board_pin_t BOARD_PIN_IP5209_INT = {10, BOARD_U2_PAD_10_FUNCTION, BOARD_U2_PAD_10_NET, "IP5209 interrupt", BOARD_PIN_SAFE_INPUT, BOARD_PIN_DIR_INPUT, BOARD_PIN_ACTIVE_LOW};
const board_pin_t BOARD_PIN_I2C_SCL = {11, BOARD_U2_PAD_11_FUNCTION, BOARD_U2_PAD_11_NET, "I2C SCL", BOARD_PIN_SAFE_INPUT_PULLUP, BOARD_PIN_DIR_BIDIRECTIONAL, BOARD_PIN_ACTIVE_NONE};
const board_pin_t BOARD_PIN_I2C_SDA = {12, BOARD_U2_PAD_12_FUNCTION, BOARD_U2_PAD_12_NET, "I2C SDA", BOARD_PIN_SAFE_INPUT_PULLUP, BOARD_PIN_DIR_BIDIRECTIONAL, BOARD_PIN_ACTIVE_NONE};
const board_pin_t BOARD_PIN_UART1_TX = {13, BOARD_U2_PAD_13_FUNCTION, BOARD_U2_PAD_13_NET, "UART1 TX to WT2003 RX via R12", BOARD_PIN_SAFE_INPUT, BOARD_PIN_DIR_ALTERNATE, BOARD_PIN_ACTIVE_HIGH};
const board_pin_t BOARD_PIN_UART1_RX = {14, BOARD_U2_PAD_14_FUNCTION, BOARD_U2_PAD_14_NET, "UART1 RX from WT2003 TX via R13", BOARD_PIN_SAFE_INPUT, BOARD_PIN_DIR_ALTERNATE, BOARD_PIN_ACTIVE_HIGH};
const board_pin_t BOARD_PIN_USB_DP = {15, BOARD_U2_PAD_15_FUNCTION, BOARD_U2_PAD_15_NET, "USB D+", BOARD_PIN_SAFE_ALTERNATE, BOARD_PIN_DIR_ALTERNATE, BOARD_PIN_ACTIVE_NONE};
const board_pin_t BOARD_PIN_USB_DN = {16, BOARD_U2_PAD_16_FUNCTION, BOARD_U2_PAD_16_NET, "USB D-", BOARD_PIN_SAFE_ALTERNATE, BOARD_PIN_DIR_ALTERNATE, BOARD_PIN_ACTIVE_NONE};
const board_pin_t BOARD_PIN_ANTENNA = {24, BOARD_U2_PAD_24_FUNCTION, BOARD_U2_PAD_24_NET, "BLE antenna", BOARD_PIN_SAFE_RF, BOARD_PIN_DIR_RF, BOARD_PIN_ACTIVE_NONE};

static const board_pin_t *const all_pins[] = {
    &BOARD_PIN_PWM_A1,
    &BOARD_PIN_PWM_A2,
    &BOARD_PIN_PWM_B1,
    &BOARD_PIN_PWM_B2,
    &BOARD_PIN_PWM_G1,
    &BOARD_PIN_PWM_G2,
    &BOARD_PIN_RGB_DATA,
    &BOARD_PIN_MOTOR_SLEEP,
    &BOARD_PIN_AUDIO_BUSY,
    &BOARD_PIN_HALL1,
    &BOARD_PIN_HALL2,
    &BOARD_PIN_TOUCH_SPD_MINUS,
    &BOARD_PIN_TOUCH_RUN,
    &BOARD_PIN_TOUCH_SPD_PLUS,
    &BOARD_PIN_TOUCH_MUSIC,
    &BOARD_PIN_IP5209_INT,
    &BOARD_PIN_I2C_SCL,
    &BOARD_PIN_I2C_SDA,
    &BOARD_PIN_UART1_TX,
    &BOARD_PIN_UART1_RX,
    &BOARD_PIN_USB_DP,
    &BOARD_PIN_USB_DN,
    &BOARD_PIN_ANTENNA,
};

static const board_pin_t *const output_pins[BOARD_OUTPUT_COUNT] = {
    &BOARD_PIN_PWM_A1,
    &BOARD_PIN_PWM_A2,
    &BOARD_PIN_PWM_B1,
    &BOARD_PIN_PWM_B2,
    &BOARD_PIN_PWM_G1,
    &BOARD_PIN_PWM_G2,
    &BOARD_PIN_RGB_DATA,
    &BOARD_PIN_MOTOR_SLEEP,
};

static const board_pin_t *const input_pins[BOARD_INPUT_COUNT] = {
    &BOARD_PIN_HALL1,
    &BOARD_PIN_HALL2,
    &BOARD_PIN_TOUCH_SPD_MINUS,
    &BOARD_PIN_TOUCH_RUN,
    &BOARD_PIN_TOUCH_SPD_PLUS,
    &BOARD_PIN_TOUCH_MUSIC,
    &BOARD_PIN_AUDIO_BUSY,
    &BOARD_PIN_IP5209_INT,
};

static uint8_t output_state[BOARD_OUTPUT_COUNT];

FW_WEAK void board_hal_apply_safe_state(const board_pin_t *pin)
{
    (void)pin;
}

FW_WEAK void board_hal_write_output(const board_pin_t *pin, uint8_t high)
{
    (void)pin;
    (void)high;
}

FW_WEAK uint8_t board_hal_read_input(const board_pin_t *pin)
{
    (void)pin;
    return 0;
}

static void apply_pin(const board_pin_t *pin)
{
    board_hal_apply_safe_state(pin);
}

void board_init_safe_pins(void)
{
    for (size_t i = 0; i < BOARD_OUTPUT_COUNT; ++i) {
        output_state[i] = 0;
        board_hal_write_output(output_pins[i], 0);
    }

    for (size_t i = 0; i < sizeof(all_pins) / sizeof(all_pins[0]); ++i) {
        apply_pin(all_pins[i]);
    }
}

void board_print_pin_map(void)
{
    size_t count = 0;
    const board_pin_t *const *pins = board_all_pins(&count);
    for (size_t i = 0; i < count; ++i) {
        LOG_INFO("U2 pad %u %-10s %-24s %-32s dir=%s active=%s safe=%s\r\n",
                 pins[i]->u2_pad,
                 pins[i]->pin_function,
                 pins[i]->net,
                 pins[i]->role,
                 board_direction_name(pins[i]->direction),
                 board_active_name(pins[i]->active),
                 board_safe_state_name(pins[i]->safe_state));
    }
}

const board_pin_t *const *board_all_pins(size_t *count)
{
    if (count) {
        *count = sizeof(all_pins) / sizeof(all_pins[0]);
    }
    return all_pins;
}

const board_pin_t *board_output_pin(board_output_id_t output)
{
    return output < BOARD_OUTPUT_COUNT ? output_pins[output] : NULL;
}

const board_pin_t *board_input_pin(board_input_id_t input)
{
    return input < BOARD_INPUT_COUNT ? input_pins[input] : NULL;
}

void board_output_write(board_output_id_t output, uint8_t high)
{
    if (output >= BOARD_OUTPUT_COUNT) {
        return;
    }
    output_state[output] = high ? 1u : 0u;
    board_hal_write_output(output_pins[output], output_state[output]);
}

uint8_t board_output_last_state(board_output_id_t output)
{
    return output < BOARD_OUTPUT_COUNT ? output_state[output] : 0u;
}

uint8_t board_output_read(board_output_id_t output)
{
    return output < BOARD_OUTPUT_COUNT ? board_hal_read_input(output_pins[output]) : 0u;
}

uint8_t board_input_read(board_input_id_t input)
{
    return input < BOARD_INPUT_COUNT ? board_hal_read_input(input_pins[input]) : 0u;
}

const char *board_safe_state_name(board_pin_safe_state_t state)
{
    switch (state) {
    case BOARD_PIN_SAFE_UNUSED: return "unused";
    case BOARD_PIN_SAFE_INPUT: return "input";
    case BOARD_PIN_SAFE_INPUT_PULLUP: return "input_pullup";
    case BOARD_PIN_SAFE_INPUT_PULLDOWN: return "input_pulldown";
    case BOARD_PIN_SAFE_OUTPUT_LOW: return "output_low";
    case BOARD_PIN_SAFE_OUTPUT_HIGH: return "output_high";
    case BOARD_PIN_SAFE_ANALOG: return "analog";
    case BOARD_PIN_SAFE_ALTERNATE: return "alternate";
    case BOARD_PIN_SAFE_POWER: return "power";
    case BOARD_PIN_SAFE_RF: return "rf";
    default: return "unknown";
    }
}

const char *board_direction_name(board_pin_direction_t direction)
{
    switch (direction) {
    case BOARD_PIN_DIR_UNUSED: return "unused";
    case BOARD_PIN_DIR_INPUT: return "input";
    case BOARD_PIN_DIR_OUTPUT: return "output";
    case BOARD_PIN_DIR_BIDIRECTIONAL: return "bidir";
    case BOARD_PIN_DIR_ANALOG: return "analog";
    case BOARD_PIN_DIR_ALTERNATE: return "alt";
    case BOARD_PIN_DIR_POWER: return "power";
    case BOARD_PIN_DIR_RF: return "rf";
    default: return "unknown";
    }
}

const char *board_active_name(board_pin_active_t active)
{
    switch (active) {
    case BOARD_PIN_ACTIVE_NONE: return "none";
    case BOARD_PIN_ACTIVE_HIGH: return "high";
    case BOARD_PIN_ACTIVE_LOW: return "low";
    case BOARD_PIN_ACTIVE_PWM_HIGH: return "pwm_high";
    case BOARD_PIN_ACTIVE_ANALOG_DELTA: return "analog_delta";
    default: return "unknown";
    }
}
