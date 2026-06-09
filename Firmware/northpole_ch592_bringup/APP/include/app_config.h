#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "board_pins_autogen_notes.h"

#define APP_FIRMWARE_VERSION "0.1.0-bringup"
#define APP_BOARD_REVISION "north-pole-ble-audio-current-pcb"

#ifndef APP_GIT_COMMIT
#define APP_GIT_COMMIT "unknown"
#endif

#ifndef APP_BUILD_DATE
#define APP_BUILD_DATE __DATE__ " " __TIME__
#endif

#define APP_RGB_LED_COUNT BOARD_AUTOGEN_RGB_LED_COUNT
#define APP_TOUCH_PAD_COUNT 4u
#define APP_HALL_SENSOR_COUNT 2u
#define APP_MOTOR_DRIVER_COUNT 3u

#ifndef APP_BRINGUP_MOTOR_ARM_MAX_MS
#define APP_BRINGUP_MOTOR_ARM_MAX_MS 10000u
#endif
#ifndef APP_BRINGUP_MOTOR_DUTY_LIMIT_PERMILLE
#define APP_BRINGUP_MOTOR_DUTY_LIMIT_PERMILLE 50u
#endif
#ifndef APP_PRODUCTION_MOTOR_DUTY_LIMIT_PERMILLE
#define APP_PRODUCTION_MOTOR_DUTY_LIMIT_PERMILLE 250u
#endif
#define APP_MOTOR_COMMAND_TIMEOUT_MS 500u
#define APP_MOTOR_SLEEP_WAKE_DELAY_MS 2u
#define APP_MOTOR_SLEEP_SETTLE_DELAY_MS 1u

#define APP_RGB_BRINGUP_BRIGHTNESS_LIMIT 24u
#define APP_RGB_DEFAULT_BRIGHTNESS 8u
#define APP_WS2812_BITBANG_ASSUMED_FREQ_SYS_HZ 60000000u
#define APP_WS2812_LOGIC_ANALYZER_VALIDATED 0u
#define APP_WS2812_T0H_NOPS 16
#define APP_WS2812_T0L_NOPS 54
#define APP_WS2812_T1H_NOPS 52
#define APP_WS2812_T1L_NOPS 16
#define APP_WS2812_RESET_US 240u
#ifndef APP_RGB_WS2812_USE_SPI0_MOSI_PA14
#define APP_RGB_WS2812_USE_SPI0_MOSI_PA14 0
#endif
#ifndef APP_RGB_WS2812_USE_PA14_BITBANG
#define APP_RGB_WS2812_USE_PA14_BITBANG 0
#endif
#if APP_RGB_WS2812_USE_SPI0_MOSI_PA14 && APP_RGB_WS2812_USE_PA14_BITBANG
#error "Select only one PA14 WS2812 backend: SPI0 MOSI or GPIO bit-bang."
#endif
#ifndef APP_WS2812_SPI0_MOSI_HZ
#define APP_WS2812_SPI0_MOSI_HZ 3200000u
#endif
#ifndef APP_WS2812_SPI0_RESET_LOW_BYTES
#define APP_WS2812_SPI0_RESET_LOW_BYTES (((APP_WS2812_RESET_US * APP_WS2812_SPI0_MOSI_HZ) + 7999999u) / 8000000u)
#endif
#define APP_RGB_COLOR_ORDER_GRB 0u
#define APP_RGB_COLOR_ORDER_RGB 1u
#define APP_RGB_COLOR_ORDER_BRG 2u
#ifndef APP_RGB_COLOR_ORDER
#define APP_RGB_COLOR_ORDER APP_RGB_COLOR_ORDER_GRB
#endif
#define APP_AUDIO_DEFAULT_VOLUME 6u
#define APP_AUDIO_UART_BAUD 9600u
#define APP_AUDIO_POWER_ON_DELAY_MS 1000u
#define APP_AUDIO_COMMAND_TIMEOUT_MS 500u
#define APP_AUDIO_FORMAT_TIMEOUT_MS 12000u
#define APP_AUDIO_COMMAND_SPACING_MS 250u
#define APP_AUDIO_RETRY_COUNT 0u
#ifndef APP_AUDIO_ALLOW_FORMAT_COMMAND
#define APP_AUDIO_ALLOW_FORMAT_COMMAND 0
#endif
#define APP_IP5209_I2C_ADDR 0x75u

#define APP_AUDIO_UART_CONNECTED BOARD_AUTOGEN_AUDIO_UART_CONNECTED
#define APP_AUDIO_HW_BLOCKED BOARD_AUTOGEN_AUDIO_HW_BLOCKED

#ifndef APP_USB_CDC_SHELL_ENABLE
#define APP_USB_CDC_SHELL_ENABLE 1
#endif

/* Dev-board isolation mode: keep the WCH BLE peripheral startup path close to
 * the EVT example, while preserving early motor/RGB safe-pin initialization.
 */
#ifndef APP_DEV_BOARD_BLE_SMOKE
#define APP_DEV_BOARD_BLE_SMOKE 0
#endif

/* Stronger isolation mode for the CH592 dev board: use the same BLE Broadcaster
 * role as the WCH EVT example that appears as "abc" in nRF Connect.
 */
#ifndef APP_DEV_BOARD_BLE_BROADCASTER_SMOKE
#define APP_DEV_BOARD_BLE_BROADCASTER_SMOKE 0
#endif

/* Dev-board app smoke mode: run the NorthPole BLE peripheral startup plus the
 * hardware-independent bring-up app core. Target-PCB GPIO peripherals remain
 * skipped because the CH592X-EVT-R1-LinkE pinout conflicts with NorthPole pins.
 */
#ifndef APP_DEV_BOARD_BRINGUP_APP_SMOKE
#define APP_DEV_BOARD_BRINGUP_APP_SMOKE 0
#endif

/* Dev-board BLE smoke builds run on CH592X-EVT-R1-LinkE, whose pins do not
 * match the NorthPole PCB. In particular PA11/TMR2 is the dev-board 32 kHz
 * crystal pin, while NorthPole uses the same CH592 function as PWM_A1. Do not
 * apply NorthPole motor/RGB safe-pin GPIO modes in dev-board smoke builds.
 */
#ifndef APP_DEV_BOARD_SKIP_NORTHPOLE_SAFE_PINS
#define APP_DEV_BOARD_SKIP_NORTHPOLE_SAFE_PINS (APP_DEV_BOARD_BLE_SMOKE || APP_DEV_BOARD_BLE_BROADCASTER_SMOKE || APP_DEV_BOARD_BRINGUP_APP_SMOKE)
#endif

/* Target-board bring-up gates. These default to the intended full target
 * firmware behavior, but can be disabled one subsystem at a time to isolate
 * first-board failures without falling back to the dev-board smoke build.
 */
#ifndef APP_TARGET_ENABLE_SAFE_PINS
#define APP_TARGET_ENABLE_SAFE_PINS 1
#endif

#ifndef APP_TARGET_ENABLE_EARLY_SAFE_PINS
#define APP_TARGET_ENABLE_EARLY_SAFE_PINS APP_TARGET_ENABLE_SAFE_PINS
#endif

#ifndef APP_TARGET_ENABLE_BOARD_SAFE_INIT
#define APP_TARGET_ENABLE_BOARD_SAFE_INIT APP_TARGET_ENABLE_SAFE_PINS
#endif

/* Fine-grained safe-output gates for first target-board isolation. These only
 * affect forced safe GPIO configuration; subsystem gates still control whether
 * drivers such as RGB, motor, I2C, audio, touch, and Hall are initialized.
 */
#ifndef APP_TARGET_SAFE_ENABLE_MOTOR_A
#define APP_TARGET_SAFE_ENABLE_MOTOR_A 1
#endif

#ifndef APP_TARGET_SAFE_ENABLE_MOTOR_B
#define APP_TARGET_SAFE_ENABLE_MOTOR_B 1
#endif

#ifndef APP_TARGET_SAFE_ENABLE_MOTOR_G
#define APP_TARGET_SAFE_ENABLE_MOTOR_G 1
#endif

#ifndef APP_TARGET_SAFE_ENABLE_MOTOR_SLEEP
#define APP_TARGET_SAFE_ENABLE_MOTOR_SLEEP 1
#endif

#ifndef APP_TARGET_SAFE_ENABLE_RGB_DATA
#define APP_TARGET_SAFE_ENABLE_RGB_DATA 1
#endif

#ifndef APP_TARGET_ENABLE_POWER_IP5209
#define APP_TARGET_ENABLE_POWER_IP5209 1
#endif

#ifndef APP_TARGET_ENABLE_HALL
#define APP_TARGET_ENABLE_HALL 1
#endif

#ifndef APP_TARGET_ENABLE_TOUCH
#define APP_TARGET_ENABLE_TOUCH 1
#endif

#ifndef APP_TARGET_ENABLE_RGB
#define APP_TARGET_ENABLE_RGB 1
#endif

#ifndef APP_TARGET_ENABLE_AUDIO
#define APP_TARGET_ENABLE_AUDIO 1
#endif

#ifndef APP_TARGET_ENABLE_MOTOR
#define APP_TARGET_ENABLE_MOTOR 1
#endif

#ifndef APP_TARGET_PRINT_PIN_MAP_AT_BOOT
#define APP_TARGET_PRINT_PIN_MAP_AT_BOOT 0
#endif

/* Debug isolation gate. WCH EVT BLE examples initialize UART1 before BLE init,
 * and WCH PRINT() uses UART1 when DEBUG=1. The target board routes UART1 to
 * the WT2003 audio chip, so normal target firmware should not depend on that
 * path. This gate is only for isolating startup differences.
 */
#ifndef APP_TARGET_USE_EVT_UART_DEBUG_STARTUP
#define APP_TARGET_USE_EVT_UART_DEBUG_STARTUP 0
#endif

#ifndef APP_SPI0_MOSI_PINMUX_TEST
#define APP_SPI0_MOSI_PINMUX_TEST 0
#endif

#ifndef APP_MOTOR_PWM_BACKEND_ENABLE
#define APP_MOTOR_PWM_BACKEND_ENABLE 0
#endif

#define APP_MOTOR_PWM_DEFAULT_HZ 20000u
#ifndef APP_MOTOR_PWM_MAX_DUTY_PERMILLE
#define APP_MOTOR_PWM_MAX_DUTY_PERMILLE APP_BRINGUP_MOTOR_DUTY_LIMIT_PERMILLE
#endif

#ifndef APP_MOTION_CONTROL_ENABLE
#define APP_MOTION_CONTROL_ENABLE 1
#endif

#ifndef APP_MOTION_DEFAULT_SPEED_HZ_X1000
#define APP_MOTION_DEFAULT_SPEED_HZ_X1000 3000
#endif

#ifndef APP_MOTION_SPEED_STEP_HZ_X1000
#define APP_MOTION_SPEED_STEP_HZ_X1000 500
#endif

#ifndef APP_MOTION_MAX_SPEED_HZ_X1000
#define APP_MOTION_MAX_SPEED_HZ_X1000 12000
#endif

#ifndef APP_MOTION_AMPLITUDE_PERMILLE
#define APP_MOTION_AMPLITUDE_PERMILLE 1000u
#endif

#ifndef APP_MOTION_GUARD_DUTY_PERMILLE
#define APP_MOTION_GUARD_DUTY_PERMILLE 1000u
#endif

#ifndef APP_MOTION_GUARD_MODE
#define APP_MOTION_GUARD_MODE 1u
#endif

#ifndef APP_SETTINGS_FLASH_ENABLE
#define APP_SETTINGS_FLASH_ENABLE 0
#endif

#define APP_SETTINGS_VERSION 1u
#define APP_DEFAULT_SCENE 0u
#define APP_MOTOR_INTENSITY_LIMIT_DEFAULT 25u

#endif /* APP_CONFIG_H */
