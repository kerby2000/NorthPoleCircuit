#include "audio_wt2003.h"
#include "ble_service.h"
#include "board.h"
#include "demo_scene.h"
#include "fault.h"
#include "hall.h"
#include "log.h"
#include "motion_control.h"
#include "motor_drv8837.h"
#include "northpole_ch592_port.h"
#include "power_ip5209.h"
#include "rgb_ws2812.h"
#include "settings.h"
#include "shell.h"
#include "timebase.h"
#include "touch.h"
#include "usb_cdc_shell.h"

static uint8_t bringup_initialized;

void app_bringup_init(void)
{
    if (bringup_initialized) {
        return;
    }

    timebase_init();
    log_init(LOG_LEVEL_INFO);
    fault_init();
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_BOARD_SAFE_INIT
    board_init_safe_pins();
#endif
    usb_cdc_shell_init();

    LOG_INFO("bring-up profile active\r\n");
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_PRINT_PIN_MAP_AT_BOOT
    board_print_pin_map();
#endif

    settings_init();
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_POWER_IP5209
    power_ip5209_init();
#endif
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_HALL
    hall_init();
#endif
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_TOUCH
    touch_init();
#endif
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_RGB
    rgb_ws2812_init();
    rgb_ws2812_set_brightness(settings_get()->brightness);
#elif !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_BOARD_SAFE_INIT
    board_output_write(BOARD_OUTPUT_RGB_DATA, 0);
#endif
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_AUDIO
    audio_wt2003_init();
#endif
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_MOTOR
    motor_drv8837_init();
#if APP_MOTION_CONTROL_ENABLE
    motion_control_init();
#endif
#endif
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_DEMO_SCENE_ENABLE
    demo_scene_init();
#endif
    ble_service_init();
    shell_init();
    bringup_initialized = 1;
}

void app_bringup_poll(void)
{
    if (!bringup_initialized) {
        app_bringup_init();
    }

    shell_poll();
    usb_cdc_shell_poll();
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_HALL
    hall_poll();
#endif
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_TOUCH
    touch_poll();
#endif
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_DEMO_SCENE_ENABLE
    demo_scene_poll();
#endif
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_MOTOR && APP_TARGET_ENABLE_TOUCH && APP_MOTION_CONTROL_ENABLE
    motion_control_poll();
#endif
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_AUDIO
    audio_wt2003_poll();
#endif
#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_MOTOR
    motor_drv8837_poll();
#endif
    ble_service_poll();

#if !APP_DEV_BOARD_BRINGUP_APP_SMOKE && APP_TARGET_ENABLE_MOTOR
    northpole_motor_wave_status_t wave_status;

    northpole_motor_wave_status(&wave_status);
    if (!motor_drv8837_is_armed() && !wave_status.running) {
        motor_drv8837_all_coast();
    }
#endif
}

void app_bringup_run(void)
{
    app_bringup_init();
    for (;;) {
        app_bringup_poll();
    }
}
