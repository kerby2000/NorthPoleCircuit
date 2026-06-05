#include "northpole_ch592_port.h"

#include "CONFIG.h"
#include "app_config.h"
#include "audio_wt2003.h"
#include "board.h"
#include "i2c_bus.h"
#include "motor_drv8837.h"
#include "power_ip5209.h"
#include "rgb_ws2812.h"
#include "shell.h"
#include "timebase.h"
#include "usb_cdc_shell.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef NORTHPOLE_ENABLE_UART1_LOG
#define NORTHPOLE_ENABLE_UART1_LOG 0
#endif

#ifndef NORTHPOLE_ENABLE_WT2003_UART
#define NORTHPOLE_ENABLE_WT2003_UART 1
#endif

#if APP_TARGET_SAFE_ENABLE_MOTOR_A
#define CH592_GPIOA_MOTOR_A_SAFE_MASK (bTMR1 | bTMR2)
#else
#define CH592_GPIOA_MOTOR_A_SAFE_MASK 0u
#endif

#if APP_TARGET_SAFE_ENABLE_MOTOR_G
#define CH592_GPIOA_MOTOR_G_SAFE_MASK (bPWM5 | bPWM4)
#else
#define CH592_GPIOA_MOTOR_G_SAFE_MASK 0u
#endif

#if APP_TARGET_SAFE_ENABLE_MOTOR_B
#define CH592_GPIOB_MOTOR_B_SAFE_MASK (bPWM9 | bPWM7)
#else
#define CH592_GPIOB_MOTOR_B_SAFE_MASK 0u
#endif

#if APP_TARGET_SAFE_ENABLE_MOTOR_SLEEP
#define CH592_GPIOB_MOTOR_SLEEP_SAFE_MASK (GPIO_Pin_0)
#else
#define CH592_GPIOB_MOTOR_SLEEP_SAFE_MASK 0u
#endif

#define CH592_GPIOA_RGB_MASK (GPIO_Pin_15)

#if APP_TARGET_SAFE_ENABLE_RGB_DATA
#define CH592_GPIOA_RGB_SAFE_MASK CH592_GPIOA_RGB_MASK
#else
#define CH592_GPIOA_RGB_SAFE_MASK 0u
#endif

#define CH592_GPIOA_SAFE_OUTPUT_LOW_MASK \
    (CH592_GPIOA_MOTOR_A_SAFE_MASK | CH592_GPIOA_MOTOR_G_SAFE_MASK | CH592_GPIOA_RGB_SAFE_MASK)
#define CH592_GPIOB_SAFE_OUTPUT_LOW_MASK \
    (CH592_GPIOB_MOTOR_B_SAFE_MASK | CH592_GPIOB_MOTOR_SLEEP_SAFE_MASK)

#define MOTOR_PWMX_CHANNELS (CH_PWM4 | CH_PWM5 | CH_PWM7 | CH_PWM9)
#define MOTOR_PWMX_CLOCK_DIV 4u

#if defined(FREQ_SYS) && (FREQ_SYS != APP_WS2812_BITBANG_ASSUMED_FREQ_SYS_HZ)
#warning "WS2812 bit-bang timing assumes 60 MHz FREQ_SYS; validate timing or retune NOP counts before using RGB."
#endif

typedef enum {
    HW_PORT_NONE = 0,
    HW_PORT_A,
    HW_PORT_B,
} hw_port_t;

typedef struct {
    hw_port_t port;
    uint32_t mask;
} hw_pin_t;

static uint8_t audio_uart_ready;
static uint8_t debug_uart_ready;
static uint32_t motor_pwm_hz = APP_MOTOR_PWM_DEFAULT_HZ;
static uint32_t motor_timer_cycle_ticks;
static uint16_t motor_pwmx_cycle_ticks;
static uint8_t motor_pwm_platform_initialized;

static int lookup_hw_pin(const board_pin_t *pin, hw_pin_t *hw)
{
    hw->port = HW_PORT_NONE;
    hw->mask = 0;

    switch (pin->u2_pad) {
    case 1: hw->port = HW_PORT_A; hw->mask = bTMR1; break;       /* /PWM_A2 */
    case 2: hw->port = HW_PORT_B; hw->mask = GPIO_Pin_6; break;  /* /HALL1 */
    case 3: hw->port = HW_PORT_B; hw->mask = GPIO_Pin_0; break;  /* /SLEEP */
    case 7: hw->port = HW_PORT_A; hw->mask = bAIN10; break;      /* /SPD-- */
    case 8: hw->port = HW_PORT_A; hw->mask = bAIN11; break;      /* /RUN */
    case 9: hw->port = HW_PORT_A; hw->mask = bAIN12; break;      /* /SPD++ */
    case 10: hw->port = HW_PORT_A; hw->mask = GPIO_Pin_9; break; /* /INT */
    case 11: hw->port = HW_PORT_B; hw->mask = bSCL; break;       /* /SCL */
    case 12: hw->port = HW_PORT_B; hw->mask = bSDA; break;       /* /SDA */
    case 13: hw->port = HW_PORT_B; hw->mask = bTXD1_; break;     /* WT RX */
    case 14: hw->port = HW_PORT_B; hw->mask = bRXD1_; break;     /* WT TX */
    case 17: hw->port = HW_PORT_B; hw->mask = bPWM9; break;      /* /PWM_B1 */
    case 18: hw->port = HW_PORT_B; hw->mask = bPWM7; break;      /* /PWM_B2 */
    case 26: hw->port = HW_PORT_A; hw->mask = GPIO_Pin_4; break; /* /HALL2 */
    case 27: hw->port = HW_PORT_A; hw->mask = bAIN1; break;      /* /MUSIC */
    case 28: hw->port = HW_PORT_A; hw->mask = GPIO_Pin_15; break;/* /LED */
    case 29: hw->port = HW_PORT_A; hw->mask = GPIO_Pin_14; break;/* /BUSY */
    case 30: hw->port = HW_PORT_A; hw->mask = bPWM5; break;      /* /PWM_G1 */
    case 31: hw->port = HW_PORT_A; hw->mask = bPWM4; break;      /* /PWM_G2 */
    case 32: hw->port = HW_PORT_A; hw->mask = bTMR2; break;      /* /PWM_A1 */
    default: return 0;
    }

    return 1;
}

static void hw_mode(hw_pin_t hw, GPIOModeTypeDef mode)
{
    if (hw.port == HW_PORT_A) {
        GPIOA_ModeCfg(hw.mask, mode);
    } else if (hw.port == HW_PORT_B) {
        GPIOB_ModeCfg(hw.mask, mode);
    }
}

static void hw_write(hw_pin_t hw, uint8_t high)
{
    if (hw.port == HW_PORT_A) {
        if (high) {
            GPIOA_SetBits(hw.mask);
        } else {
            GPIOA_ResetBits(hw.mask);
        }
    } else if (hw.port == HW_PORT_B) {
        if (high) {
            GPIOB_SetBits(hw.mask);
        } else {
            GPIOB_ResetBits(hw.mask);
        }
    }
}

static uint8_t hw_read(hw_pin_t hw)
{
    if (hw.port == HW_PORT_A) {
        return GPIOA_ReadPortPin(hw.mask) ? 1u : 0u;
    }
    if (hw.port == HW_PORT_B) {
        return GPIOB_ReadPortPin(hw.mask) ? 1u : 0u;
    }
    return 0;
}

static uint8_t safe_output_gate_enabled(const board_pin_t *pin)
{
    switch (pin->u2_pad) {
    case 1:
    case 32:
        return APP_TARGET_SAFE_ENABLE_MOTOR_A ? 1u : 0u;
    case 17:
    case 18:
        return APP_TARGET_SAFE_ENABLE_MOTOR_B ? 1u : 0u;
    case 30:
    case 31:
        return APP_TARGET_SAFE_ENABLE_MOTOR_G ? 1u : 0u;
    case 3:
        return APP_TARGET_SAFE_ENABLE_MOTOR_SLEEP ? 1u : 0u;
    case 28:
        return APP_TARGET_SAFE_ENABLE_RGB_DATA ? 1u : 0u;
    default:
        return 1u;
    }
}

void northpole_ch592_early_safe_pins(void)
{
    if (CH592_GPIOA_SAFE_OUTPUT_LOW_MASK != 0u) {
        GPIOA_ResetBits(CH592_GPIOA_SAFE_OUTPUT_LOW_MASK);
        GPIOA_ModeCfg(CH592_GPIOA_SAFE_OUTPUT_LOW_MASK, GPIO_ModeOut_PP_5mA);
    }

    if (CH592_GPIOB_SAFE_OUTPUT_LOW_MASK != 0u) {
        GPIOB_ResetBits(CH592_GPIOB_SAFE_OUTPUT_LOW_MASK);
        GPIOB_ModeCfg(CH592_GPIOB_SAFE_OUTPUT_LOW_MASK, GPIO_ModeOut_PP_5mA);
    }

    GPIOA_ModeCfg(GPIO_Pin_9 | GPIO_Pin_14, GPIO_ModeIN_Floating);
    GPIOB_ModeCfg(bRXD1_ | bTXD1_, GPIO_ModeIN_Floating);
}

void northpole_ch592_debug_uart_init(void)
{
#if NORTHPOLE_ENABLE_UART1_LOG
    GPIOPinRemap(ENABLE, RB_PIN_UART1);
    GPIOB_SetBits(bTXD1_);
    GPIOB_ModeCfg(bTXD1_, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(bRXD1_, GPIO_ModeIN_PU);
    UART1_DefInit();
    debug_uart_ready = 1;
#else
    debug_uart_ready = 0;
#endif
}

void northpole_ch592_audio_uart_init(void)
{
#if NORTHPOLE_ENABLE_WT2003_UART
    if (!audio_uart_ready) {
        GPIOPinRemap(ENABLE, RB_PIN_UART1);
        GPIOB_SetBits(bTXD1_);
        GPIOB_ModeCfg(bTXD1_, GPIO_ModeOut_PP_5mA);
        GPIOB_ModeCfg(bRXD1_, GPIO_ModeIN_PU);
        UART1_DefInit();
        UART1_BaudRateCfg(APP_AUDIO_UART_BAUD);
        audio_uart_ready = 1;
    }
#endif
}

void board_hal_apply_safe_state(const board_pin_t *pin)
{
    hw_pin_t hw;

    if (!lookup_hw_pin(pin, &hw)) {
        return;
    }

    switch (pin->safe_state) {
    case BOARD_PIN_SAFE_OUTPUT_LOW:
        if (!safe_output_gate_enabled(pin)) {
            return;
        }
        hw_write(hw, 0);
        hw_mode(hw, GPIO_ModeOut_PP_5mA);
        break;
    case BOARD_PIN_SAFE_OUTPUT_HIGH:
        if (!safe_output_gate_enabled(pin)) {
            return;
        }
        hw_write(hw, 1);
        hw_mode(hw, GPIO_ModeOut_PP_5mA);
        break;
    case BOARD_PIN_SAFE_INPUT_PULLUP:
        hw_mode(hw, GPIO_ModeIN_PU);
        break;
    case BOARD_PIN_SAFE_INPUT_PULLDOWN:
        hw_mode(hw, GPIO_ModeIN_PD);
        break;
    case BOARD_PIN_SAFE_ANALOG:
    case BOARD_PIN_SAFE_INPUT:
        hw_mode(hw, GPIO_ModeIN_Floating);
        break;
    case BOARD_PIN_SAFE_UNUSED:
        hw_mode(hw, GPIO_ModeIN_PD);
        break;
    case BOARD_PIN_SAFE_ALTERNATE:
    case BOARD_PIN_SAFE_POWER:
    case BOARD_PIN_SAFE_RF:
    default:
        break;
    }
}

void board_hal_write_output(const board_pin_t *pin, uint8_t high)
{
    hw_pin_t hw;

    if (lookup_hw_pin(pin, &hw)) {
        if (!safe_output_gate_enabled(pin)) {
            return;
        }
        hw_write(hw, high);
    }
}

uint8_t board_hal_read_input(const board_pin_t *pin)
{
    hw_pin_t hw;

    if (!lookup_hw_pin(pin, &hw)) {
        return 0;
    }
    return hw_read(hw);
}

uint32_t timebase_platform_ms(void)
{
    return (uint32_t)(((uint64_t)TMOS_GetSystemClock() * SYSTEM_TIME_MICROSEN) / 1000u);
}

void timebase_platform_delay_ms(uint32_t delay_ms)
{
    while (delay_ms > 0) {
        uint16_t chunk = delay_ms > 60000u ? 60000u : (uint16_t)delay_ms;
        mDelaymS(chunk);
        delay_ms -= chunk;
    }
}

void timebase_platform_delay_us(uint32_t delay_us)
{
    while (delay_us > 0) {
        uint16_t chunk = delay_us > 60000u ? 60000u : (uint16_t)delay_us;
        mDelayuS(chunk);
        delay_us -= chunk;
    }
}

void log_platform_write(const char *text)
{
    size_t len = strlen(text);

    if (usb_cdc_shell_write_wait((const uint8_t *)text, len, 10000u) == len) {
        return;
    }

#if NORTHPOLE_ENABLE_UART1_LOG
    if (!debug_uart_ready) {
        northpole_ch592_debug_uart_init();
    }
    UART1_SendString((uint8_t *)text, (uint16_t)len);
#else
    (void)text;
#endif
}

int shell_platform_read_line(char *buffer, size_t buffer_size)
{
    if (usb_cdc_shell_read_line(buffer, buffer_size) > 0) {
        return 1;
    }

#if NORTHPOLE_ENABLE_UART1_LOG
    static char line[96];
    static size_t used;

    if (buffer_size == 0) {
        return 0;
    }

    while (R8_UART1_RFC) {
        char c = (char)UART1_RecvByte();
        if (c == '\r' || c == '\n') {
            line[used] = '\0';
            strncpy(buffer, line, buffer_size - 1u);
            buffer[buffer_size - 1u] = '\0';
            used = 0;
            return 1;
        }
        if (used < sizeof(line) - 1u) {
            line[used++] = c;
        }
    }
#else
    (void)buffer;
    (void)buffer_size;
#endif
    return 0;
}

int audio_wt2003_platform_send(const uint8_t *data, uint8_t len)
{
#if NORTHPOLE_ENABLE_WT2003_UART
    northpole_ch592_audio_uart_init();
    UART1_SendString((uint8_t *)data, len);
    return 0;
#else
    (void)data;
    (void)len;
    return -1;
#endif
}

int audio_wt2003_platform_read_byte(uint8_t *byte)
{
#if NORTHPOLE_ENABLE_WT2003_UART
    if (!byte) {
        return -1;
    }
    if (!audio_uart_ready) {
        return 0;
    }
    if (R8_UART1_RFC) {
        *byte = UART1_RecvByte();
        return 1;
    }
    return 0;
#else
    (void)byte;
    return -1;
#endif
}

int audio_wt2003_platform_recv(uint8_t *data, uint8_t max_len, uint32_t timeout_ms)
{
#if NORTHPOLE_ENABLE_WT2003_UART
    uint32_t start_ms = timebase_ms();
    uint8_t count = 0;

    northpole_ch592_audio_uart_init();
    while ((uint32_t)(timebase_ms() - start_ms) <= timeout_ms) {
        while (R8_UART1_RFC && count < max_len) {
            data[count++] = UART1_RecvByte();
        }
        if (count > 0) {
            return count;
        }
    }
    return 0;
#else
    (void)data;
    (void)max_len;
    (void)timeout_ms;
    return -1;
#endif
}

static uint32_t motor_timer_cycle_for_hz(uint32_t pwm_hz)
{
    uint32_t cycle;

    if (pwm_hz == 0) {
        pwm_hz = APP_MOTOR_PWM_DEFAULT_HZ;
    }

    cycle = FREQ_SYS / pwm_hz;
    if (cycle < 2u) {
        cycle = 2u;
    }
    if (cycle > 65535u) {
        cycle = 65535u;
    }
    return cycle;
}

static uint16_t motor_pwmx_cycle_for_hz(uint32_t pwm_hz)
{
    uint32_t cycle;

    if (pwm_hz == 0) {
        pwm_hz = APP_MOTOR_PWM_DEFAULT_HZ;
    }

    cycle = FREQ_SYS / (pwm_hz * MOTOR_PWMX_CLOCK_DIV);
    if (cycle < 2u) {
        cycle = 2u;
    }
    if (cycle > 65535u) {
        cycle = 65535u;
    }
    return (uint16_t)cycle;
}

static uint16_t motor_pwm_duty_ticks(uint16_t duty_permille)
{
    uint32_t ticks = ((uint32_t)motor_pwmx_cycle_ticks * duty_permille) / 1000u;

    if (duty_permille > 0u && ticks == 0u) {
        ticks = 1u;
    }
    if (ticks > motor_pwmx_cycle_ticks) {
        ticks = motor_pwmx_cycle_ticks;
    }
    return (uint16_t)ticks;
}

static void motor_pwmx_configure_globals(void)
{
#if APP_MOTOR_PWM_BACKEND_ENABLE
    if (motor_pwmx_cycle_ticks == 0u) {
        motor_pwm_hz = APP_MOTOR_PWM_DEFAULT_HZ;
        motor_timer_cycle_ticks = motor_timer_cycle_for_hz(motor_pwm_hz);
        motor_pwmx_cycle_ticks = motor_pwmx_cycle_for_hz(motor_pwm_hz);
    }
    motor_pwm_platform_initialized = 1u;
    PWMX_CLKCfg(MOTOR_PWMX_CLOCK_DIV);
    PWMX_16bit_CycleCfg((uint16_t)(motor_pwmx_cycle_ticks - 1u));
#endif
}

static void motor_timer_configure_globals(void)
{
#if APP_MOTOR_PWM_BACKEND_ENABLE
    if (motor_timer_cycle_ticks == 0u) {
        motor_pwm_hz = APP_MOTOR_PWM_DEFAULT_HZ;
        motor_timer_cycle_ticks = motor_timer_cycle_for_hz(motor_pwm_hz);
        motor_pwmx_cycle_ticks = motor_pwmx_cycle_for_hz(motor_pwm_hz);
    }
    motor_pwm_platform_initialized = 1u;

    GPIOPinRemap(DISABLE, RB_PIN_TMR1); /* PA10/TMR1 is /PWM_A2 on this PCB. */
    GPIOPinRemap(DISABLE, RB_PIN_TMR2); /* PA11/TMR2 is /PWM_A1 on this PCB. */

    TMR1_PWMCycleCfg(motor_timer_cycle_ticks);
    TMR1_PWMInit(High_Level, PWM_Times_1);

    TMR2_PWMCycleCfg(motor_timer_cycle_ticks);
    TMR2_PWMInit(High_Level, PWM_Times_1);
#endif
}

static void motor_pwmx_disable(uint8_t channel)
{
#if APP_MOTOR_PWM_BACKEND_ENABLE
    PWMX_16bit_ACTOUT(channel, 0, High_Level, DISABLE);
#else
    (void)channel;
#endif
}

static void motor_timer1_disable_low(void)
{
#if APP_MOTOR_PWM_BACKEND_ENABLE
    TMR1_PWMDisable();
    TMR1_Disable();
#endif
    GPIOPinRemap(DISABLE, RB_PIN_TMR1); /* Keep TMR1 on PA10; PB10 is USB D-. */
    GPIOA_ResetBits(bTMR1);
    GPIOA_ModeCfg(bTMR1, GPIO_ModeOut_PP_5mA);
}

static void motor_timer2_disable_low(void)
{
#if APP_MOTOR_PWM_BACKEND_ENABLE
    TMR2_PWMDisable();
    TMR2_Disable();
#endif
    GPIOPinRemap(DISABLE, RB_PIN_TMR2); /* Keep TMR2 on PA11; PB11 is USB D+. */
    GPIOA_ResetBits(bTMR2);
    GPIOA_ModeCfg(bTMR2, GPIO_ModeOut_PP_5mA);
}

static void motor_pwmx_disable_low(uint8_t channel, hw_pin_t hw)
{
    motor_pwmx_disable(channel);
    hw_write(hw, 0);
    hw_mode(hw, GPIO_ModeOut_PP_5mA);
}

void northpole_diag_force_gpio_output(board_output_id_t output, uint8_t high)
{
    hw_pin_t hw;
    const board_pin_t *pin;

    if (output >= BOARD_OUTPUT_COUNT) {
        return;
    }

    switch (output) {
    case BOARD_OUTPUT_PWM_A1:
        motor_timer2_disable_low();
        break;
    case BOARD_OUTPUT_PWM_A2:
        motor_timer1_disable_low();
        break;
    case BOARD_OUTPUT_PWM_B1:
        motor_pwmx_disable(CH_PWM9);
        break;
    case BOARD_OUTPUT_PWM_B2:
        motor_pwmx_disable(CH_PWM7);
        break;
    case BOARD_OUTPUT_PWM_G1:
        motor_pwmx_disable(CH_PWM5);
        break;
    case BOARD_OUTPUT_PWM_G2:
        motor_pwmx_disable(CH_PWM4);
        break;
    default:
        break;
    }

    board_output_write(output, high);
    pin = board_output_pin(output);
    if (pin != NULL && lookup_hw_pin(pin, &hw)) {
        hw_write(hw, high);
        hw_mode(hw, GPIO_ModeOut_PP_5mA);
    }
}

static void motor_driver_static_apply(motor_driver_id_t driver,
                                      motor_drv8837_mode_t mode,
                                      uint16_t duty_permille)
{
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

    if (duty_permille == 0 && mode != MOTOR_DRV_BRAKE) {
        mode = MOTOR_DRV_COAST;
    }

    switch (mode) {
    case MOTOR_DRV_FORWARD:
        board_output_write(in1_pin[driver], 1);
        board_output_write(in2_pin[driver], 0);
        break;
    case MOTOR_DRV_REVERSE:
        board_output_write(in1_pin[driver], 0);
        board_output_write(in2_pin[driver], 1);
        break;
    case MOTOR_DRV_BRAKE:
        board_output_write(in1_pin[driver], 1);
        board_output_write(in2_pin[driver], 1);
        break;
    case MOTOR_DRV_COAST:
    default:
        board_output_write(in1_pin[driver], 0);
        board_output_write(in2_pin[driver], 0);
        break;
    }
}

void motor_drv8837_platform_init(uint32_t pwm_hz)
{
    motor_pwm_hz = pwm_hz == 0 ? APP_MOTOR_PWM_DEFAULT_HZ : pwm_hz;
    motor_timer_cycle_ticks = motor_timer_cycle_for_hz(motor_pwm_hz);
    motor_pwmx_cycle_ticks = motor_pwmx_cycle_for_hz(motor_pwm_hz);
    motor_pwm_platform_initialized = 1u;

    motor_timer1_disable_low();
    motor_timer2_disable_low();
    motor_pwmx_disable_low(CH_PWM9, (hw_pin_t){HW_PORT_B, bPWM9});
    motor_pwmx_disable_low(CH_PWM7, (hw_pin_t){HW_PORT_B, bPWM7});
    motor_pwmx_disable_low(CH_PWM5, (hw_pin_t){HW_PORT_A, bPWM5});
    motor_pwmx_disable_low(CH_PWM4, (hw_pin_t){HW_PORT_A, bPWM4});

#if APP_MOTOR_PWM_BACKEND_ENABLE
    GPIOPinRemap(DISABLE, RB_PIN_TMR1); /* PA10/TMR1 is /PWM_A2 on this PCB. */
    GPIOPinRemap(DISABLE, RB_PIN_TMR2); /* PA11/TMR2 is /PWM_A1 on this PCB. */

    TMR1_PWMCycleCfg(motor_timer_cycle_ticks);
    TMR1_PWMActDataWidth(0);
    TMR1_PWMInit(High_Level, PWM_Times_1);

    TMR2_PWMCycleCfg(motor_timer_cycle_ticks);
    TMR2_PWMActDataWidth(0);
    TMR2_PWMInit(High_Level, PWM_Times_1);

    motor_pwmx_configure_globals();
    PWMX_16bit_ACTOUT(MOTOR_PWMX_CHANNELS, 0, High_Level, DISABLE);
#endif
}

static void motor_pwm_a_apply(motor_drv8837_mode_t mode, uint16_t duty_permille)
{
#if APP_MOTOR_PWM_BACKEND_ENABLE
    uint32_t duty;

    motor_timer_configure_globals();

    duty = ((uint32_t)motor_timer_cycle_ticks * duty_permille) / 1000u;

    if (duty > motor_timer_cycle_ticks) {
        duty = motor_timer_cycle_ticks;
    }

    switch (mode) {
    case MOTOR_DRV_FORWARD:
        motor_timer1_disable_low();
        GPIOPinRemap(DISABLE, RB_PIN_TMR2);
        GPIOA_ModeCfg(bTMR2, GPIO_ModeOut_PP_5mA);
        TMR2_PWMActDataWidth(duty);
        TMR2_PWMEnable();
        TMR2_Enable();
        break;
    case MOTOR_DRV_REVERSE:
        motor_timer2_disable_low();
        GPIOPinRemap(DISABLE, RB_PIN_TMR1);
        GPIOA_ModeCfg(bTMR1, GPIO_ModeOut_PP_5mA);
        TMR1_PWMActDataWidth(duty);
        TMR1_PWMEnable();
        TMR1_Enable();
        break;
    case MOTOR_DRV_BRAKE:
        TMR1_PWMDisable();
        TMR2_PWMDisable();
        GPIOA_SetBits(bTMR2);
        GPIOA_SetBits(bTMR1);
        GPIOA_ModeCfg(bTMR2, GPIO_ModeOut_PP_5mA);
        GPIOA_ModeCfg(bTMR1, GPIO_ModeOut_PP_5mA);
        break;
    case MOTOR_DRV_COAST:
    default:
        motor_timer1_disable_low();
        motor_timer2_disable_low();
        break;
    }
#else
    motor_driver_static_apply(MOTOR_DRV_A, mode, duty_permille);
#endif
}

static void motor_pwmx_pair_apply(uint8_t forward_ch,
                                  hw_pin_t forward_pin,
                                  uint8_t reverse_ch,
                                  hw_pin_t reverse_pin,
                                  motor_drv8837_mode_t mode,
                                  uint16_t duty_permille)
{
#if APP_MOTOR_PWM_BACKEND_ENABLE
    uint16_t duty = motor_pwm_duty_ticks(duty_permille);

    switch (mode) {
    case MOTOR_DRV_FORWARD:
        motor_pwmx_disable_low(reverse_ch, reverse_pin);
        motor_pwmx_configure_globals();
        hw_mode(forward_pin, GPIO_ModeOut_PP_5mA);
        PWMX_16bit_ACTOUT(forward_ch, duty, High_Level, ENABLE);
        break;
    case MOTOR_DRV_REVERSE:
        motor_pwmx_disable_low(forward_ch, forward_pin);
        motor_pwmx_configure_globals();
        hw_mode(reverse_pin, GPIO_ModeOut_PP_5mA);
        PWMX_16bit_ACTOUT(reverse_ch, duty, High_Level, ENABLE);
        break;
    case MOTOR_DRV_BRAKE:
        motor_pwmx_disable(forward_ch | reverse_ch);
        hw_write(forward_pin, 1);
        hw_write(reverse_pin, 1);
        hw_mode(forward_pin, GPIO_ModeOut_PP_5mA);
        hw_mode(reverse_pin, GPIO_ModeOut_PP_5mA);
        break;
    case MOTOR_DRV_COAST:
    default:
        motor_pwmx_disable_low(forward_ch, forward_pin);
        motor_pwmx_disable_low(reverse_ch, reverse_pin);
        break;
    }
#else
    (void)forward_ch;
    (void)forward_pin;
    (void)reverse_ch;
    (void)reverse_pin;
    (void)mode;
    (void)duty_permille;
#endif
}

void motor_drv8837_platform_apply(motor_driver_id_t driver,
                                  motor_drv8837_mode_t mode,
                                  uint16_t duty_permille)
{
    if (duty_permille == 0 && mode != MOTOR_DRV_BRAKE) {
        mode = MOTOR_DRV_COAST;
    }

#if !APP_MOTOR_PWM_BACKEND_ENABLE
    motor_driver_static_apply(driver, mode, duty_permille);
#else
    switch (driver) {
    case MOTOR_DRV_A:
        motor_pwm_a_apply(mode, duty_permille);
        break;
    case MOTOR_DRV_B:
        motor_pwmx_pair_apply(CH_PWM9, (hw_pin_t){HW_PORT_B, bPWM9},
                              CH_PWM7, (hw_pin_t){HW_PORT_B, bPWM7},
                              mode, duty_permille);
        break;
    case MOTOR_DRV_G:
        motor_pwmx_pair_apply(CH_PWM5, (hw_pin_t){HW_PORT_A, bPWM5},
                              CH_PWM4, (hw_pin_t){HW_PORT_A, bPWM4},
                              mode, duty_permille);
        break;
    default:
        break;
    }
#endif
}

void northpole_motor_pwm_diag_apply(motor_driver_id_t driver,
                                    motor_drv8837_mode_t mode,
                                    uint16_t duty_permille)
{
    if (duty_permille > 1000u) {
        duty_permille = 1000u;
    }
    if (duty_permille == 0u && mode != MOTOR_DRV_BRAKE) {
        mode = MOTOR_DRV_COAST;
    }

#if !APP_MOTOR_PWM_BACKEND_ENABLE
    motor_driver_static_apply(driver, mode, duty_permille);
#else
    switch (driver) {
    case MOTOR_DRV_A:
        motor_pwm_a_apply(mode, duty_permille);
        break;
    case MOTOR_DRV_B:
        motor_pwmx_pair_apply(CH_PWM9, (hw_pin_t){HW_PORT_B, bPWM9},
                              CH_PWM7, (hw_pin_t){HW_PORT_B, bPWM7},
                              mode, duty_permille);
        break;
    case MOTOR_DRV_G:
        motor_pwmx_pair_apply(CH_PWM5, (hw_pin_t){HW_PORT_A, bPWM5},
                              CH_PWM4, (hw_pin_t){HW_PORT_A, bPWM4},
                              mode, duty_permille);
        break;
    default:
        break;
    }
#endif
}

void northpole_ch592_motor_pwm_debug(uint8_t *initialized,
                                     uint32_t *pwm_hz,
                                     uint32_t *timer_cycle_ticks,
                                     uint16_t *pwmx_cycle_ticks,
                                     uint8_t *pwmx_clock_div,
                                     uint16_t *duty_50_permille_ticks)
{
    if (initialized) {
        *initialized = motor_pwm_platform_initialized;
    }
    if (pwm_hz) {
        *pwm_hz = motor_pwm_hz;
    }
    if (timer_cycle_ticks) {
        *timer_cycle_ticks = motor_timer_cycle_ticks;
    }
    if (pwmx_cycle_ticks) {
        *pwmx_cycle_ticks = motor_pwmx_cycle_ticks;
    }
    if (pwmx_clock_div) {
        *pwmx_clock_div = MOTOR_PWMX_CLOCK_DIV;
    }
    if (duty_50_permille_ticks) {
        *duty_50_permille_ticks = motor_pwm_duty_ticks(50u);
    }
}

#define WS2812_STR2(value) #value
#define WS2812_STR(value) WS2812_STR2(value)
#define WS2812_NOPS(count) __asm__ volatile(".rept " WS2812_STR(count) "\n\tnop\n\t.endr\n")

static inline void ws2812_send_bit(uint8_t bit)
{
    R32_PA_OUT |= CH592_GPIOA_RGB_MASK;
    if (bit) {
        WS2812_NOPS(APP_WS2812_T1H_NOPS);
        R32_PA_CLR = CH592_GPIOA_RGB_MASK;
        WS2812_NOPS(APP_WS2812_T1L_NOPS);
    } else {
        WS2812_NOPS(APP_WS2812_T0H_NOPS);
        R32_PA_CLR = CH592_GPIOA_RGB_MASK;
        WS2812_NOPS(APP_WS2812_T0L_NOPS);
    }
}

static void ws2812_send_byte(uint8_t value)
{
    for (uint8_t mask = 0x80u; mask != 0; mask >>= 1) {
        ws2812_send_bit((value & mask) ? 1u : 0u);
    }
}

static uint8_t ws2812_scale(uint8_t value, uint8_t brightness)
{
    return (uint8_t)(((uint16_t)value * brightness) / 255u);
}

static void ws2812_send_color(rgb_color_t color, uint8_t brightness)
{
    uint8_t r = ws2812_scale(color.r, brightness);
    uint8_t g = ws2812_scale(color.g, brightness);
    uint8_t b = ws2812_scale(color.b, brightness);

#if APP_RGB_COLOR_ORDER == APP_RGB_COLOR_ORDER_RGB
    ws2812_send_byte(r);
    ws2812_send_byte(g);
    ws2812_send_byte(b);
#elif APP_RGB_COLOR_ORDER == APP_RGB_COLOR_ORDER_BRG
    ws2812_send_byte(b);
    ws2812_send_byte(r);
    ws2812_send_byte(g);
#else
    ws2812_send_byte(g);
    ws2812_send_byte(r);
    ws2812_send_byte(b);
#endif
}

int rgb_ws2812_platform_write(const rgb_color_t *colors, uint8_t count, uint8_t global_brightness)
{
    uint32_t irq_status = 0;

    /* Fixed NOP timing is calibrated for 60 MHz FREQ_SYS and is still
     * hardware-validation pending. The XL-1010RGBC-WS2812B datasheet
     * requires reset-low >200 us and GRB, MSB-first data. Validate actual
     * T0H/T1H/period/reset timing on /LED with a logic analyzer.
     */
    if (!colors) {
        return -1;
    }
    if (count > APP_RGB_LED_COUNT) {
        count = APP_RGB_LED_COUNT;
    }
    if (global_brightness > APP_RGB_BRINGUP_BRIGHTNESS_LIMIT) {
        global_brightness = APP_RGB_BRINGUP_BRIGHTNESS_LIMIT;
    }

    GPIOA_ResetBits(CH592_GPIOA_RGB_MASK);
    GPIOA_ModeCfg(CH592_GPIOA_RGB_MASK, GPIO_ModeOut_PP_5mA);
    mDelayuS(APP_WS2812_RESET_US);

    SYS_DisableAllIrq(&irq_status);
    for (uint8_t i = 0; i < count; ++i) {
        ws2812_send_color(colors[i], global_brightness);
    }
    R32_PA_CLR = CH592_GPIOA_RGB_MASK;
    SYS_RecoverIrq(irq_status);

    mDelayuS(APP_WS2812_RESET_US);
    return 0;
}

static int ch592_i2c_timeout_ms(uint32_t timeout_ms)
{
    return timeout_ms == 0 ? I2C_BUS_DEFAULT_TIMEOUT_MS : (int)timeout_ms;
}

static int ch592_i2c_wait_event(uint32_t event, uint32_t timeout_ms)
{
    uint32_t start_ms = timebase_ms();
    uint32_t limit_ms = (uint32_t)ch592_i2c_timeout_ms(timeout_ms);

    while (!I2C_CheckEvent(event)) {
        if (I2C_GetFlagStatus(I2C_FLAG_AF) != RESET) {
            I2C_ClearFlag(I2C_FLAG_AF);
            I2C_GenerateSTOP(ENABLE);
            return -2;
        }
        if (I2C_GetFlagStatus(I2C_FLAG_BERR) != RESET) {
            I2C_ClearFlag(I2C_FLAG_BERR);
            I2C_GenerateSTOP(ENABLE);
            return -3;
        }
        if (I2C_GetFlagStatus(I2C_FLAG_ARLO) != RESET) {
            I2C_ClearFlag(I2C_FLAG_ARLO);
            I2C_GenerateSTOP(ENABLE);
            return -4;
        }
        if ((uint32_t)(timebase_ms() - start_ms) > limit_ms) {
            I2C_GenerateSTOP(ENABLE);
            return -5;
        }
    }
    return 0;
}

static int ch592_i2c_wait_not_busy(uint32_t timeout_ms)
{
    uint32_t start_ms = timebase_ms();
    uint32_t limit_ms = (uint32_t)ch592_i2c_timeout_ms(timeout_ms);

    while (I2C_GetFlagStatus(I2C_FLAG_BUSY) != RESET) {
        if ((uint32_t)(timebase_ms() - start_ms) > limit_ms) {
            I2C_GenerateSTOP(ENABLE);
            return -1;
        }
    }
    return 0;
}

static void ch592_i2c_start(void)
{
    I2C_GenerateSTOP(DISABLE);
    I2C_GenerateSTART(ENABLE);
}

int i2c_bus_platform_init(uint32_t bus_hz)
{
    if (bus_hz == 0) {
        bus_hz = I2C_BUS_DEFAULT_HZ;
    }
    if (bus_hz > 400000u) {
        bus_hz = 400000u;
    }

    GPIOB_ModeCfg(bSCL | bSDA, GPIO_ModeIN_PU);
    I2C_Init(I2C_Mode_I2C,
             bus_hz,
             I2C_DutyCycle_16_9,
             I2C_Ack_Enable,
             I2C_AckAddr_7bit,
             0x42u);
    I2C_ITConfig(I2C_IT_BUF, DISABLE);
    I2C_ITConfig(I2C_IT_EVT, DISABLE);
    I2C_ITConfig(I2C_IT_ERR, DISABLE);
    return 0;
}

int i2c_bus_platform_debug_snapshot(i2c_bus_debug_t *debug)
{
    if (!debug) {
        return -1;
    }

    debug->scl_level = (R32_PB_PIN & bSCL) ? 1u : 0u;
    debug->sda_level = (R32_PB_PIN & bSDA) ? 1u : 0u;
    debug->ctrl1 = R16_I2C_CTRL1;
    debug->ctrl2 = R16_I2C_CTRL2;
    debug->star1 = R16_I2C_STAR1;
    debug->star2 = R16_I2C_STAR2;
    debug->ckcfgr = R16_I2C_CKCFGR;
    debug->pin_alternate = R16_PIN_ALTERNATE;
    debug->pin_config2 = R32_PIN_CONFIG2;
    return 0;
}

int i2c_bus_platform_release_debug_pins(void)
{
    /*
     * PB14/PB15 are shared by CH592 TIO/TCK debug and SCL/SDA on this PCB.
     * Set RB_DEBUG_EN to disable the runtime debug function so the I2C
     * peripheral can own the pins. This does not change flash config fuses,
     * but WCH-Link attach may be unavailable until reset/download mode.
     */
    R16_PIN_ALTERNATE &= (uint16_t)~(RB_PIN_SPI0 | RB_PIN_MODEM);
    R16_PIN_ALTERNATE |= RB_DEBUG_EN;
    GPIOB_ModeCfg(bSCL | bSDA, GPIO_ModeIN_PU);
    I2C_SoftwareResetCmd(ENABLE);
    I2C_SoftwareResetCmd(DISABLE);
    return 0;
}

int i2c_bus_platform_probe(uint8_t addr7, uint32_t timeout_ms)
{
    int rc;

    if (ch592_i2c_wait_not_busy(timeout_ms) < 0) {
        return -1;
    }

    ch592_i2c_start();
    rc = ch592_i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT, timeout_ms);
    if (rc < 0) {
        return rc;
    }

    I2C_Send7bitAddress((uint8_t)(addr7 << 1), I2C_Direction_Transmitter);
    rc = ch592_i2c_wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, timeout_ms);
    I2C_GenerateSTOP(ENABLE);
    return rc;
}

int i2c_bus_platform_write_reg8(uint8_t addr7, uint8_t reg, uint8_t value, uint32_t timeout_ms)
{
    int rc;

    if (ch592_i2c_wait_not_busy(timeout_ms) < 0) {
        return -1;
    }

    ch592_i2c_start();
    rc = ch592_i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT, timeout_ms);
    if (rc < 0) {
        return rc;
    }
    I2C_Send7bitAddress((uint8_t)(addr7 << 1), I2C_Direction_Transmitter);
    rc = ch592_i2c_wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, timeout_ms);
    if (rc < 0) {
        return rc;
    }
    I2C_SendData(reg);
    rc = ch592_i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTING, timeout_ms);
    if (rc < 0) {
        return rc;
    }
    I2C_SendData(value);
    rc = ch592_i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED, timeout_ms);
    I2C_GenerateSTOP(ENABLE);
    return rc;
}

int i2c_bus_platform_read_reg8(uint8_t addr7, uint8_t reg, uint8_t *value, uint32_t timeout_ms)
{
    int rc;

    if (!value) {
        return -1;
    }
    if (ch592_i2c_wait_not_busy(timeout_ms) < 0) {
        return -2;
    }

    ch592_i2c_start();
    rc = ch592_i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT, timeout_ms);
    if (rc < 0) {
        return rc;
    }
    I2C_Send7bitAddress((uint8_t)(addr7 << 1), I2C_Direction_Transmitter);
    rc = ch592_i2c_wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, timeout_ms);
    if (rc < 0) {
        return rc;
    }
    I2C_SendData(reg);
    rc = ch592_i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED, timeout_ms);
    if (rc < 0) {
        return rc;
    }

    ch592_i2c_start();
    rc = ch592_i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT, timeout_ms);
    if (rc < 0) {
        return rc;
    }
    I2C_AcknowledgeConfig(DISABLE);
    I2C_Send7bitAddress((uint8_t)(addr7 << 1), I2C_Direction_Receiver);
    rc = ch592_i2c_wait_event(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, timeout_ms);
    if (rc < 0) {
        return rc;
    }
    rc = ch592_i2c_wait_event(I2C_EVENT_MASTER_BYTE_RECEIVED, timeout_ms);
    if (rc < 0) {
        return rc;
    }
    *value = I2C_ReceiveData();
    I2C_GenerateSTOP(ENABLE);
    I2C_AcknowledgeConfig(ENABLE);
    return 0;
}

int power_ip5209_platform_read_register(uint8_t reg, uint8_t *value)
{
    return i2c_bus_read_reg8(APP_IP5209_I2C_ADDR, reg, value, I2C_BUS_DEFAULT_TIMEOUT_MS);
}

int power_ip5209_platform_write_register(uint8_t reg, uint8_t value)
{
    return i2c_bus_write_reg8(APP_IP5209_I2C_ADDR, reg, value, I2C_BUS_DEFAULT_TIMEOUT_MS);
}

uint16_t touch_platform_measure(const board_pin_t *pin)
{
    return board_hal_read_input(pin);
}
