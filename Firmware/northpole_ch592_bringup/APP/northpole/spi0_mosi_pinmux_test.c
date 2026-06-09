#include "spi0_mosi_pinmux_test.h"

#include "CH59x_common.h"
#include "app_config.h"
#include "board.h"
#include "log.h"
#include "northpole_ch592_port.h"
#include "timebase.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if APP_SPI0_MOSI_PINMUX_TEST

#define SPI0TEST_PA12_SCS_OR_PWM4 GPIO_Pin_12
#define SPI0TEST_PA13_SCK_OR_PWM5 GPIO_Pin_13
#define SPI0TEST_PA14_MOSI        GPIO_Pin_14
#define SPI0TEST_PA15_MISO_OR_LED GPIO_Pin_15
#define SPI0TEST_ALL_PINS         (SPI0TEST_PA12_SCS_OR_PWM4 | SPI0TEST_PA13_SCK_OR_PWM5 | SPI0TEST_PA14_MOSI | SPI0TEST_PA15_MISO_OR_LED)
#define SPI0TEST_DEFAULT_HZ       2400000u
#define SPI0TEST_PWM_HZ           20000u
#define SPI0TEST_MAX_SECONDS      30u
#define SPI0TEST_MAX_REPEAT       65535u
#define SPI0TEST_MAX_FRAMES       64u

typedef enum {
    SPI0TEST_MODE_SAFE = 0,
    SPI0TEST_MODE_MOSI_ONLY,
    SPI0TEST_MODE_DEFAULT_SPI,
} spi0test_mode_t;

static spi0test_mode_t active_mode = SPI0TEST_MODE_SAFE;
static uint32_t active_speed_hz = SPI0TEST_DEFAULT_HZ;
static uint8_t active_clock_div = 25u;

static uint32_t parse_u32_arg(const char *text, uint32_t fallback)
{
    char *end = NULL;
    unsigned long value;

    if (!text) {
        return fallback;
    }
    value = strtoul(text, &end, 0);
    if (end == text || *end != '\0') {
        return fallback;
    }
    return (uint32_t)value;
}

static int parse_pattern_arg(const char *text, uint8_t *pattern)
{
    uint32_t value;

    if (!text || !pattern) {
        return -1;
    }
    if (strcmp(text, "00") == 0 || strcmp(text, "0x00") == 0) {
        *pattern = 0x00u;
        return 0;
    }
    if (strcmp(text, "ff") == 0 || strcmp(text, "FF") == 0 || strcmp(text, "0xff") == 0 || strcmp(text, "0xFF") == 0) {
        *pattern = 0xffu;
        return 0;
    }
    if (strcmp(text, "aa") == 0 || strcmp(text, "AA") == 0 || strcmp(text, "0xaa") == 0 || strcmp(text, "0xAA") == 0) {
        *pattern = 0xaau;
        return 0;
    }
    if (strcmp(text, "55") == 0 || strcmp(text, "0x55") == 0) {
        *pattern = 0x55u;
        return 0;
    }
    value = parse_u32_arg(text, 0xffffffffu);
    if (value <= 0xffu) {
        *pattern = (uint8_t)value;
        return 0;
    }
    return -1;
}

static uint8_t spi0_clock_div_for_hz(uint32_t hz)
{
    uint32_t div;

    if (hz == 0u) {
        hz = SPI0TEST_DEFAULT_HZ;
    }
    div = (FREQ_SYS + (hz / 2u)) / hz;
    if (div < 2u) {
        div = 2u;
    }
    if (div > 255u) {
        div = 255u;
    }
    return (uint8_t)div;
}

static uint32_t spi0_effective_hz(uint8_t div)
{
    if (div == 0u) {
        return 0u;
    }
    return FREQ_SYS / div;
}

static const char *mode_name(spi0test_mode_t mode)
{
    switch (mode) {
    case SPI0TEST_MODE_MOSI_ONLY:
        return "mosi-only";
    case SPI0TEST_MODE_DEFAULT_SPI:
        return "default-spi";
    case SPI0TEST_MODE_SAFE:
    default:
        return "safe";
    }
}

static void log_pin_state(void)
{
    uint32_t pin = R32_PA_PIN;
    uint32_t out = R32_PA_OUT;
    uint32_t dir = R32_PA_DIR;

    LOG_INFO("pa12=%u pa13=%u pa14=%u pa15=%u out=0x%08lx dir=0x%08lx alt=0x%04x\r\n",
             (unsigned)((pin & SPI0TEST_PA12_SCS_OR_PWM4) ? 1u : 0u),
             (unsigned)((pin & SPI0TEST_PA13_SCK_OR_PWM5) ? 1u : 0u),
             (unsigned)((pin & SPI0TEST_PA14_MOSI) ? 1u : 0u),
             (unsigned)((pin & SPI0TEST_PA15_MISO_OR_LED) ? 1u : 0u),
             (unsigned long)out,
             (unsigned long)dir,
             (unsigned)R16_PIN_ALTERNATE);
}

static void spi0test_disable_spi_pwm(void)
{
    SPI0_Disable();
    R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;
    R8_SPI0_CTRL_MOD = 0;
    R8_SPI0_CTRL_CFG &= (uint8_t)~(RB_SPI_DMA_ENABLE | RB_SPI_DMA_LOOP);
    GPIOPinRemap(DISABLE, RB_PIN_SPI0);
    GPIOPinRemap(DISABLE, RB_PIN_PWMX);
    PWMX_16bit_ACTOUT(CH_PWM4, 0, High_Level, DISABLE);
}

static void spi0test_all_safe(void)
{
    northpole_motor_wave_stop();
    board_output_write(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
    spi0test_disable_spi_pwm();

    GPIOA_ResetBits(SPI0TEST_ALL_PINS);
    GPIOA_ModeCfg(SPI0TEST_PA12_SCS_OR_PWM4 | SPI0TEST_PA13_SCK_OR_PWM5 | SPI0TEST_PA15_MISO_OR_LED,
                  GPIO_ModeOut_PP_5mA);
    GPIOA_ModeCfg(SPI0TEST_PA14_MOSI, GPIO_ModeIN_Floating);
    active_mode = SPI0TEST_MODE_SAFE;
}

static void spi0test_configure_spi(uint8_t mosi_only)
{
    uint8_t div = spi0_clock_div_for_hz(active_speed_hz);

    northpole_motor_wave_stop();
    board_output_write(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
    spi0test_disable_spi_pwm();
    active_clock_div = div;

    if (mosi_only) {
        GPIOA_ResetBits(SPI0TEST_ALL_PINS);
        GPIOA_ModeCfg(SPI0TEST_PA12_SCS_OR_PWM4 | SPI0TEST_PA13_SCK_OR_PWM5 | SPI0TEST_PA15_MISO_OR_LED,
                      GPIO_ModeOut_PP_5mA);
        GPIOA_ModeCfg(SPI0TEST_PA14_MOSI, GPIO_ModeOut_PP_5mA);
        R8_SPI0_CLOCK_DIV = div;
        R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;
        R8_SPI0_CTRL_MOD = RB_SPI_MOSI_OE;
        R8_SPI0_CTRL_CFG = (uint8_t)((R8_SPI0_CTRL_CFG | RB_SPI_AUTO_IF) & (uint8_t)~(RB_SPI_DMA_ENABLE | RB_SPI_DMA_LOOP | RB_SPI_BIT_ORDER | RB_SPI_MST_DLY_EN));
        if (div == 2u) {
            R8_SPI0_CTRL_CFG |= RB_SPI_MST_DLY_EN;
        }
        active_mode = SPI0TEST_MODE_MOSI_ONLY;
    } else {
        GPIOA_SetBits(SPI0TEST_PA12_SCS_OR_PWM4);
        GPIOA_ModeCfg(SPI0TEST_PA12_SCS_OR_PWM4 | SPI0TEST_PA13_SCK_OR_PWM5 | SPI0TEST_PA14_MOSI,
                      GPIO_ModeOut_PP_5mA);
        GPIOA_ModeCfg(SPI0TEST_PA15_MISO_OR_LED, GPIO_ModeIN_Floating);
        R8_SPI0_CLOCK_DIV = div;
        R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;
        R8_SPI0_CTRL_MOD = RB_SPI_MOSI_OE | RB_SPI_SCK_OE;
        R8_SPI0_CTRL_CFG = (uint8_t)((R8_SPI0_CTRL_CFG | RB_SPI_AUTO_IF) & (uint8_t)~(RB_SPI_DMA_ENABLE | RB_SPI_DMA_LOOP | RB_SPI_BIT_ORDER | RB_SPI_MST_DLY_EN));
        if (div == 2u) {
            R8_SPI0_CTRL_CFG |= RB_SPI_MST_DLY_EN;
        }
        active_mode = SPI0TEST_MODE_DEFAULT_SPI;
    }
}

static void spi0test_ensure_spi_ready(void)
{
    if (active_mode == SPI0TEST_MODE_SAFE) {
        spi0test_configure_spi(1u);
        LOG_INFO("spi0test auto init-mosi-only\r\n");
    }
}

static int spi0test_send_byte(uint8_t value)
{
    uint32_t guard = 1000000u;

    R8_SPI0_CTRL_MOD &= (uint8_t)~RB_SPI_FIFO_DIR;
    R8_SPI0_BUFFER = value;
    while ((R8_SPI0_INT_FLAG & RB_SPI_FREE) == 0u) {
        if (--guard == 0u) {
            return -1;
        }
    }
    return 0;
}

static int spi0test_send_pattern(uint8_t pattern, uint32_t repeat_count)
{
    spi0test_ensure_spi_ready();
    if (repeat_count == 0u) {
        repeat_count = 1u;
    }
    if (repeat_count > SPI0TEST_MAX_REPEAT) {
        repeat_count = SPI0TEST_MAX_REPEAT;
    }
    for (uint32_t i = 0; i < repeat_count; ++i) {
        if (spi0test_send_byte(pattern) != 0) {
            return -1;
        }
    }
    return 0;
}

static int spi0test_send_ws2812_byte_3bit(uint8_t value)
{
    uint16_t shifter = 0;
    uint8_t used = 0;

    for (uint8_t mask = 0x80u; mask != 0u; mask >>= 1) {
        uint8_t code = (value & mask) ? 0x06u : 0x04u; /* 1=110, 0=100 */
        shifter = (uint16_t)((shifter << 3) | code);
        used = (uint8_t)(used + 3u);
        while (used >= 8u) {
            uint8_t out = (uint8_t)(shifter >> (used - 8u));
            if (spi0test_send_byte(out) != 0) {
                return -1;
            }
            used = (uint8_t)(used - 8u);
            shifter &= (uint16_t)((1u << used) - 1u);
        }
    }
    return 0;
}

static int spi0test_send_ws2812_byte_4bit(uint8_t value)
{
    for (int shift = 6; shift >= 0; shift -= 2) {
        uint8_t hi_mask = (uint8_t)(1u << (shift + 1));
        uint8_t lo_mask = (uint8_t)(1u << shift);
        uint8_t hi_code = (value & hi_mask) ? 0x0eu : 0x08u; /* 1=1110, 0=1000 */
        uint8_t lo_code = (value & lo_mask) ? 0x0eu : 0x08u;
        if (spi0test_send_byte((uint8_t)((hi_code << 4) | lo_code)) != 0) {
            return -1;
        }
    }
    return 0;
}

static int spi0test_send_ws2812(uint8_t value, uint32_t frames)
{
    uint8_t use_4bit;

    spi0test_ensure_spi_ready();
    if (frames == 0u) {
        frames = 1u;
    }
    if (frames > SPI0TEST_MAX_FRAMES) {
        frames = SPI0TEST_MAX_FRAMES;
    }

    use_4bit = active_speed_hz >= 3000000u ? 1u : 0u;
    for (uint32_t frame = 0; frame < frames; ++frame) {
        for (uint8_t led = 0; led < APP_RGB_LED_COUNT; ++led) {
            for (uint8_t byte = 0; byte < 3u; ++byte) {
                int rc = use_4bit ? spi0test_send_ws2812_byte_4bit(value) : spi0test_send_ws2812_byte_3bit(value);
                if (rc != 0) {
                    return rc;
                }
            }
        }
    }
    GPIOA_ResetBits(SPI0TEST_PA14_MOSI);
    mDelayuS(APP_WS2812_RESET_US);
    return 0;
}

static void spi0test_mosi_square(uint32_t hz, uint32_t seconds)
{
    uint32_t half_us;
    uint32_t edges;

    if (hz == 0u) {
        hz = 1u;
    }
    if (seconds == 0u) {
        seconds = 1u;
    }
    if (seconds > SPI0TEST_MAX_SECONDS) {
        seconds = SPI0TEST_MAX_SECONDS;
    }
    half_us = 500000u / hz;
    if (half_us == 0u) {
        half_us = 1u;
    }
    if (hz > 1000000u / (2u * seconds)) {
        edges = 2000000u;
    } else {
        edges = hz * seconds * 2u;
    }

    northpole_motor_wave_stop();
    board_output_write(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
    spi0test_disable_spi_pwm();
    GPIOA_ResetBits(SPI0TEST_PA14_MOSI);
    GPIOA_ModeCfg(SPI0TEST_PA14_MOSI, GPIO_ModeOut_PP_5mA);
    GPIOA_ResetBits(SPI0TEST_PA12_SCS_OR_PWM4 | SPI0TEST_PA13_SCK_OR_PWM5 | SPI0TEST_PA15_MISO_OR_LED);
    GPIOA_ModeCfg(SPI0TEST_PA12_SCS_OR_PWM4 | SPI0TEST_PA13_SCK_OR_PWM5 | SPI0TEST_PA15_MISO_OR_LED,
                  GPIO_ModeOut_PP_5mA);

    for (uint32_t i = 0; i < edges; ++i) {
        GPIOA_InverseBits(SPI0TEST_PA14_MOSI);
        mDelayuS((uint16_t)half_us);
    }
    GPIOA_ResetBits(SPI0TEST_PA14_MOSI);
    active_mode = SPI0TEST_MODE_SAFE;
}

static void spi0test_pwm_pa12(uint16_t duty_permille, uint32_t seconds)
{
    uint32_t cycle = FREQ_SYS / (SPI0TEST_PWM_HZ * 4u);
    uint32_t duty;

    if (cycle < 2u) {
        cycle = 2u;
    }
    if (cycle > 65535u) {
        cycle = 65535u;
    }
    if (duty_permille > 1000u) {
        duty_permille = 1000u;
    }
    duty = (cycle * duty_permille) / 1000u;
    if (duty_permille > 0u && duty == 0u) {
        duty = 1u;
    }
    if (seconds == 0u) {
        seconds = 1u;
    }
    if (seconds > SPI0TEST_MAX_SECONDS) {
        seconds = SPI0TEST_MAX_SECONDS;
    }

    northpole_motor_wave_stop();
    board_output_write(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
    SPI0_Disable();
    GPIOPinRemap(DISABLE, RB_PIN_PWMX);
    GPIOA_ModeCfg(SPI0TEST_PA12_SCS_OR_PWM4, GPIO_ModeOut_PP_5mA);
    PWMX_CLKCfg(4);
    PWMX_16bit_CycleCfg((uint16_t)(cycle - 1u));
    PWMX_16bit_ACTOUT(CH_PWM4, (uint16_t)duty, High_Level, ENABLE);
    LOG_INFO("spi0test pwm-pa12 hz=%u duty=%u/%lu seconds=%lu sleep=0\r\n",
             (unsigned)SPI0TEST_PWM_HZ,
             (unsigned)duty_permille,
             (unsigned long)cycle,
             (unsigned long)seconds);
    mDelaymS((uint16_t)(seconds * 1000u));
    PWMX_16bit_ACTOUT(CH_PWM4, 0, High_Level, DISABLE);
    GPIOA_ResetBits(SPI0TEST_PA12_SCS_OR_PWM4);
    GPIOA_ModeCfg(SPI0TEST_PA12_SCS_OR_PWM4, GPIO_ModeOut_PP_5mA);
}

static void spi0test_hall_pa13_read(uint32_t seconds)
{
    uint8_t last = 0xffu;
    uint32_t start;

    if (seconds == 0u) {
        seconds = 1u;
    }
    if (seconds > SPI0TEST_MAX_SECONDS) {
        seconds = SPI0TEST_MAX_SECONDS;
    }

    northpole_motor_wave_stop();
    board_output_write(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
    SPI0_Disable();
    GPIOA_ModeCfg(SPI0TEST_PA13_SCK_OR_PWM5, GPIO_ModeIN_PU);
    start = timebase_ms();
    while ((uint32_t)(timebase_ms() - start) < (seconds * 1000u)) {
        uint8_t now = (R32_PA_PIN & SPI0TEST_PA13_SCK_OR_PWM5) ? 1u : 0u;
        if (now != last) {
            LOG_INFO("spi0test pa13 level=%u t_ms=%lu\r\n", (unsigned)now, (unsigned long)(timebase_ms() - start));
            last = now;
        }
        mDelaymS(50);
    }
}

static void spi0test_status(void)
{
    LOG_INFO("spi0test enabled=1 mode=%s requested_hz=%lu div=%u effective_hz=%lu sleep=0\r\n",
             mode_name(active_mode),
             (unsigned long)active_speed_hz,
             (unsigned)active_clock_div,
             (unsigned long)spi0_effective_hz(active_clock_div));
    LOG_INFO("spi0 regs ctrl_mod=0x%02x ctrl_cfg=0x%02x clock_div=%u int_flag=0x%02x fifo=%u run=0x%02x\r\n",
             (unsigned)R8_SPI0_CTRL_MOD,
             (unsigned)R8_SPI0_CTRL_CFG,
             (unsigned)R8_SPI0_CLOCK_DIV,
             (unsigned)R8_SPI0_INT_FLAG,
             (unsigned)R8_SPI0_FIFO_COUNT,
             (unsigned)R8_SPI0_RUN_FLAG);
    log_pin_state();
}

int spi0_mosi_pinmux_test_command(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "status") == 0) {
        spi0test_status();
        return 0;
    }
    if (strcmp(argv[1], "gpio-baseline") == 0) {
        spi0test_all_safe();
        LOG_INFO("spi0test gpio-baseline: pa12/pa13/pa15 low outputs, pa14 input; /SLEEP low\r\n");
        log_pin_state();
        return 0;
    }
    if (strcmp(argv[1], "all-safe") == 0) {
        spi0test_all_safe();
        LOG_INFO("spi0test all-safe done; /SLEEP low\r\n");
        log_pin_state();
        return 0;
    }
    if (strcmp(argv[1], "speed") == 0 && argc >= 3) {
        active_speed_hz = parse_u32_arg(argv[2], active_speed_hz);
        active_clock_div = spi0_clock_div_for_hz(active_speed_hz);
        LOG_INFO("spi0test speed requested_hz=%lu div=%u effective_hz=%lu\r\n",
                 (unsigned long)active_speed_hz,
                 (unsigned)active_clock_div,
                 (unsigned long)spi0_effective_hz(active_clock_div));
        return 0;
    }
    if (strcmp(argv[1], "init-mosi-only") == 0) {
        spi0test_configure_spi(1u);
        LOG_INFO("spi0test init-mosi-only done; MOSI_OE only, SCK_OE disabled\r\n");
        spi0test_status();
        return 0;
    }
    if (strcmp(argv[1], "init-default-spi") == 0) {
        spi0test_configure_spi(0u);
        LOG_INFO("spi0test init-default-spi done; MOSI_OE and SCK_OE enabled\r\n");
        spi0test_status();
        return 0;
    }
    if (strcmp(argv[1], "mosi-square") == 0 && argc >= 4) {
        uint32_t hz = parse_u32_arg(argv[2], 1000u);
        uint32_t seconds = parse_u32_arg(argv[3], 1u);
        LOG_INFO("spi0test mosi-square hz=%lu seconds=%lu sleep=0\r\n", (unsigned long)hz, (unsigned long)seconds);
        spi0test_mosi_square(hz, seconds);
        LOG_INFO("spi0test mosi-square done\r\n");
        return 0;
    }
    if (strcmp(argv[1], "send-pattern") == 0 && argc >= 4) {
        uint8_t pattern = 0;
        uint32_t repeat = parse_u32_arg(argv[3], 1u);
        if (parse_pattern_arg(argv[2], &pattern) != 0) {
            LOG_WARN("bad spi0test pattern\r\n");
            return -1;
        }
        int rc = spi0test_send_pattern(pattern, repeat);
        LOG_INFO("spi0test send-pattern pattern=0x%02x repeat=%lu mode=%s rc=%d\r\n",
                 (unsigned)pattern,
                 (unsigned long)repeat,
                 mode_name(active_mode),
                 rc);
        return rc;
    }
    if (strcmp(argv[1], "send-ws2812-zeros") == 0 && argc >= 3) {
        uint32_t frames = parse_u32_arg(argv[2], 1u);
        int rc = spi0test_send_ws2812(0x00u, frames);
        LOG_INFO("spi0test send-ws2812-zeros frames=%lu encoding=%ubit rc=%d\r\n",
                 (unsigned long)frames,
                 active_speed_hz >= 3000000u ? 4u : 3u,
                 rc);
        return rc;
    }
    if (strcmp(argv[1], "send-ws2812-ones") == 0 && argc >= 3) {
        uint32_t frames = parse_u32_arg(argv[2], 1u);
        int rc = spi0test_send_ws2812(0xffu, frames);
        LOG_INFO("spi0test send-ws2812-ones frames=%lu encoding=%ubit rc=%d\r\n",
                 (unsigned long)frames,
                 active_speed_hz >= 3000000u ? 4u : 3u,
                 rc);
        return rc;
    }
    if (strcmp(argv[1], "send-ws2812-pattern") == 0 && argc >= 4) {
        uint8_t pattern = 0;
        uint32_t frames = parse_u32_arg(argv[3], 1u);
        if (parse_pattern_arg(argv[2], &pattern) != 0) {
            LOG_WARN("bad spi0test ws2812 pattern\r\n");
            return -1;
        }
        int rc = spi0test_send_ws2812(pattern, frames);
        LOG_INFO("spi0test send-ws2812-pattern pattern=0x%02x frames=%lu encoding=%ubit rc=%d\r\n",
                 (unsigned)pattern,
                 (unsigned long)frames,
                 active_speed_hz >= 3000000u ? 4u : 3u,
                 rc);
        return rc;
    }
    if (strcmp(argv[1], "pwm-pa12") == 0 && argc >= 4) {
        uint16_t duty = (uint16_t)parse_u32_arg(argv[2], 500u);
        uint32_t seconds = parse_u32_arg(argv[3], 1u);
        spi0test_pwm_pa12(duty, seconds);
        return 0;
    }
    if (strcmp(argv[1], "hall-pa13-read") == 0 && argc >= 3) {
        uint32_t seconds = parse_u32_arg(argv[2], 1u);
        spi0test_hall_pa13_read(seconds);
        return 0;
    }

    LOG_WARN("usage: spi0test status|gpio-baseline|all-safe|speed <hz>|mosi-square <hz> <seconds>|init-mosi-only|init-default-spi|send-pattern <00|ff|aa|55> <repeat>|send-ws2812-zeros <frames>|send-ws2812-ones <frames>|send-ws2812-pattern <00|ff|aa|55> <frames>|pwm-pa12 <duty_permille> <seconds>|hall-pa13-read <seconds>\r\n");
    return -1;
}

#else

int spi0_mosi_pinmux_test_command(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LOG_WARN("spi0test disabled; rebuild with APP_SPI0_MOSI_PINMUX_TEST=1\r\n");
    return -1;
}

#endif
