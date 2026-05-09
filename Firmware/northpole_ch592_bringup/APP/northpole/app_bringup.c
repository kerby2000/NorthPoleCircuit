#include "audio_wt2003.h"
#include "ble_service.h"
#include "board.h"
#include "fault.h"
#include "hall.h"
#include "log.h"
#include "motor_drv8837.h"
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
    board_init_safe_pins();
    usb_cdc_shell_init();

    LOG_INFO("bring-up profile active\r\n");
    board_print_pin_map();

    settings_init();
    power_ip5209_init();
    hall_init();
    touch_init();
    rgb_ws2812_init();
    rgb_ws2812_set_brightness(settings_get()->brightness);
    audio_wt2003_init();
    motor_drv8837_init();
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
    hall_poll();
    touch_poll();
    audio_wt2003_poll();
    motor_drv8837_poll();
    ble_service_poll();

    if (!motor_drv8837_is_armed()) {
        motor_drv8837_all_coast();
    }
}

void app_bringup_run(void)
{
    app_bringup_init();
    for (;;) {
        app_bringup_poll();
    }
}
