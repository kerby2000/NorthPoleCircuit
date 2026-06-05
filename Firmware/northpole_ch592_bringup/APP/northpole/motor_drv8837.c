#include "motor_drv8837.h"

#include "app_config.h"
#include "board.h"
#include "build_profile.h"
#include "fault.h"
#include "timebase.h"

#if defined(__GNUC__)
#define FW_WEAK __attribute__((weak))
#define FW_NOINLINE __attribute__((noinline))
#else
#define FW_WEAK
#define FW_NOINLINE
#endif

static motor_drv8837_state_t state[MOTOR_DRV_COUNT];
static uint32_t armed_until_ms;
static uint8_t motor_platform_initialized;

static const board_output_id_t in1_pin[MOTOR_DRV_COUNT] = {
    BOARD_OUTPUT_PWM_A1,
    BOARD_OUTPUT_PWM_B1,
    BOARD_OUTPUT_PWM_G1,
};

static const board_output_id_t in2_pin[MOTOR_DRV_COUNT] = {
    BOARD_OUTPUT_PWM_A2,
    BOARD_OUTPUT_PWM_B2,
    BOARD_OUTPUT_PWM_G2,
};

static uint16_t duty_limit(void)
{
#if BUILD_PROFILE == BUILD_PROFILE_PRODUCTION
    return APP_PRODUCTION_MOTOR_DUTY_LIMIT_PERMILLE;
#else
    return APP_BRINGUP_MOTOR_DUTY_LIMIT_PERMILLE;
#endif
}

static void write_driver(motor_driver_id_t driver, uint8_t in1, uint8_t in2)
{
    board_output_write(in1_pin[driver], in1);
    board_output_write(in2_pin[driver], in2);
}

static void motor_sleep_set(uint8_t awake)
{
#if BOARD_MOTOR_SLEEP_ACTIVE_HIGH
    board_output_write(BOARD_OUTPUT_MOTOR_SLEEP, awake ? 1u : 0u);
#else
    board_output_write(BOARD_OUTPUT_MOTOR_SLEEP, awake ? 0u : 1u);
#endif
}

static void motor_outputs_safe_direct(void)
{
    for (uint8_t i = 0; i < MOTOR_DRV_COUNT; ++i) {
        state[i].mode = MOTOR_DRV_COAST;
        state[i].duty_permille = 0;
        state[i].expires_ms = 0;
        write_driver((motor_driver_id_t)i, 0, 0);
    }
    motor_sleep_set(0);
}

static void platform_apply_static(motor_driver_id_t driver, motor_drv8837_mode_t mode, uint16_t duty_permille)
{
    if (duty_permille == 0 && mode != MOTOR_DRV_BRAKE) {
        mode = MOTOR_DRV_COAST;
    }

    switch (mode) {
    case MOTOR_DRV_FORWARD:
        write_driver(driver, 1, 0);
        break;
    case MOTOR_DRV_REVERSE:
        write_driver(driver, 0, 1);
        break;
    case MOTOR_DRV_BRAKE:
        write_driver(driver, 1, 1);
        break;
    case MOTOR_DRV_COAST:
    default:
        write_driver(driver, 0, 0);
        break;
    }
}

FW_WEAK FW_NOINLINE void motor_drv8837_platform_init(uint32_t pwm_hz)
{
    volatile uint32_t keep_call = pwm_hz;
    (void)keep_call;
}

FW_WEAK void motor_drv8837_platform_apply(motor_driver_id_t driver,
                                          motor_drv8837_mode_t mode,
                                          uint16_t duty_permille)
{
    platform_apply_static(driver, mode, duty_permille);
}

void motor_drv8837_init(void)
{
    motor_drv8837_platform_init(APP_MOTOR_PWM_DEFAULT_HZ);
    motor_platform_initialized = 1u;
    armed_until_ms = 0;
    motor_drv8837_off();
}

void motor_drv8837_poll(void)
{
    uint32_t now = timebase_ms();

    if (armed_until_ms != 0 && (int32_t)(now - armed_until_ms) >= 0) {
        fault_raise(FAULT_MOTOR_TIMEOUT);
        motor_drv8837_disarm();
        return;
    }

    for (uint8_t i = 0; i < MOTOR_DRV_COUNT; ++i) {
        if (state[i].mode != MOTOR_DRV_COAST &&
            state[i].expires_ms != 0 &&
            (int32_t)(now - state[i].expires_ms) >= 0) {
            state[i].mode = MOTOR_DRV_COAST;
            state[i].duty_permille = 0;
            state[i].expires_ms = 0;
            motor_drv8837_platform_apply((motor_driver_id_t)i, MOTOR_DRV_COAST, 0);
        }
    }
}

void motor_drv8837_arm(uint32_t duration_ms)
{
    if (duration_ms > APP_BRINGUP_MOTOR_ARM_MAX_MS) {
        duration_ms = APP_BRINGUP_MOTOR_ARM_MAX_MS;
    }
    motor_drv8837_all_coast();
    motor_sleep_set(1);
    timebase_delay_ms(APP_MOTOR_SLEEP_WAKE_DELAY_MS);
    armed_until_ms = timebase_ms() + duration_ms;
}

void motor_drv8837_disarm(void)
{
    motor_drv8837_off();
}

void motor_drv8837_off(void)
{
    armed_until_ms = 0;
    if (!motor_platform_initialized) {
        motor_outputs_safe_direct();
        return;
    }
    motor_drv8837_all_coast();
    timebase_delay_ms(APP_MOTOR_SLEEP_SETTLE_DELAY_MS);
    motor_sleep_set(0);
}

uint8_t motor_drv8837_is_armed(void)
{
    if (armed_until_ms == 0) {
        return 0;
    }
    if ((int32_t)(timebase_ms() - armed_until_ms) >= 0) {
        fault_raise(FAULT_MOTOR_TIMEOUT);
        motor_drv8837_disarm();
        return 0;
    }
    return 1;
}

uint32_t motor_drv8837_arm_remaining_ms(void)
{
    uint32_t now;

    if (!motor_drv8837_is_armed()) {
        return 0;
    }

    now = timebase_ms();
    return (uint32_t)(armed_until_ms - now);
}

int motor_drv8837_command(motor_driver_id_t driver, motor_drv8837_mode_t mode, uint16_t duty_permille)
{
    return motor_drv8837_command_for(driver, mode, duty_permille, APP_MOTOR_COMMAND_TIMEOUT_MS);
}

int motor_drv8837_command_for(motor_driver_id_t driver,
                              motor_drv8837_mode_t mode,
                              uint16_t duty_permille,
                              uint32_t duration_ms)
{
    if (driver >= MOTOR_DRV_COUNT) {
        return -1;
    }

    if (duty_permille == 0 && mode != MOTOR_DRV_BRAKE) {
        mode = MOTOR_DRV_COAST;
    }

    if (mode != MOTOR_DRV_COAST && !motor_drv8837_is_armed()) {
        fault_raise(FAULT_MOTOR_NOT_ARMED);
        state[driver].mode = MOTOR_DRV_COAST;
        state[driver].duty_permille = 0;
        state[driver].expires_ms = 0;
        motor_drv8837_platform_apply(driver, MOTOR_DRV_COAST, 0);
        return -2;
    }

    if (duty_permille > duty_limit()) {
        fault_raise(FAULT_MOTOR_DUTY_LIMIT);
        state[driver].mode = MOTOR_DRV_COAST;
        state[driver].duty_permille = 0;
        state[driver].expires_ms = 0;
        motor_drv8837_platform_apply(driver, MOTOR_DRV_COAST, 0);
        return -3;
    }

    if (mode != MOTOR_DRV_COAST && duration_ms == 0) {
        motor_drv8837_platform_apply(driver, MOTOR_DRV_COAST, 0);
        return -4;
    }

    if (duration_ms > APP_MOTOR_COMMAND_TIMEOUT_MS) {
        duration_ms = APP_MOTOR_COMMAND_TIMEOUT_MS;
    }

    state[driver].mode = mode;
    state[driver].duty_permille = duty_permille;
    state[driver].expires_ms = mode == MOTOR_DRV_COAST ? 0 : timebase_ms() + duration_ms;
    motor_drv8837_platform_apply(driver, mode, duty_permille);
    return 0;
}

void motor_drv8837_all_coast(void)
{
    for (uint8_t i = 0; i < MOTOR_DRV_COUNT; ++i) {
        state[i].mode = MOTOR_DRV_COAST;
        state[i].duty_permille = 0;
        state[i].expires_ms = 0;
        motor_drv8837_platform_apply((motor_driver_id_t)i, MOTOR_DRV_COAST, 0);
    }
}

motor_drv8837_state_t motor_drv8837_get_state(motor_driver_id_t driver)
{
    motor_drv8837_state_t empty = {MOTOR_DRV_COAST, 0, 0};
    return driver < MOTOR_DRV_COUNT ? state[driver] : empty;
}

const char *motor_drv8837_driver_name(motor_driver_id_t driver)
{
    switch (driver) {
    case MOTOR_DRV_A: return "A";
    case MOTOR_DRV_B: return "B";
    case MOTOR_DRV_G: return "G";
    default: return "?";
    }
}

const char *motor_drv8837_mode_name(motor_drv8837_mode_t mode)
{
    switch (mode) {
    case MOTOR_DRV_COAST: return "coast";
    case MOTOR_DRV_FORWARD: return "forward";
    case MOTOR_DRV_REVERSE: return "reverse";
    case MOTOR_DRV_BRAKE: return "brake";
    default: return "?";
    }
}

void fault_platform_on_fault(fault_code_t fault)
{
    static uint8_t handling_fault;

    if (fault == FAULT_NONE || handling_fault) {
        return;
    }

    handling_fault = 1u;
    motor_drv8837_off();
    handling_fault = 0u;
}
