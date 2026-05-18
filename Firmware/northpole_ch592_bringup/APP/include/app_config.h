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

#define APP_BRINGUP_MOTOR_ARM_MAX_MS 10000u
#define APP_BRINGUP_MOTOR_DUTY_LIMIT_PERMILLE 50u
#define APP_PRODUCTION_MOTOR_DUTY_LIMIT_PERMILLE 250u
#define APP_MOTOR_COMMAND_TIMEOUT_MS 500u
#define APP_MOTOR_SLEEP_WAKE_DELAY_MS 2u
#define APP_MOTOR_SLEEP_SETTLE_DELAY_MS 1u

#define APP_RGB_BRINGUP_BRIGHTNESS_LIMIT 24u
#define APP_RGB_DEFAULT_BRIGHTNESS 8u
#define APP_WS2812_BITBANG_ASSUMED_FREQ_SYS_HZ 60000000u
#define APP_WS2812_LOGIC_ANALYZER_VALIDATED 0u
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

#ifndef APP_MOTOR_PWM_BACKEND_ENABLE
#define APP_MOTOR_PWM_BACKEND_ENABLE 0
#endif

#define APP_MOTOR_PWM_DEFAULT_HZ 20000u
#define APP_MOTOR_PWM_MAX_DUTY_PERMILLE APP_BRINGUP_MOTOR_DUTY_LIMIT_PERMILLE

#ifndef APP_SETTINGS_FLASH_ENABLE
#define APP_SETTINGS_FLASH_ENABLE 0
#endif

#define APP_SETTINGS_VERSION 1u
#define APP_DEFAULT_SCENE 0u
#define APP_MOTOR_INTENSITY_LIMIT_DEFAULT 25u

#endif /* APP_CONFIG_H */
