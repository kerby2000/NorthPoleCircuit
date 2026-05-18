/********************************** (C) COPYRIGHT *******************************
 * File Name          : peripheral_main.c
 * Author             : WCH / North Pole firmware bring-up
 * Version            : V0.1
 * Description        : CH592 BLE peripheral startup with North Pole safe bring-up.
 ********************************************************************************/

#include "CONFIG.h"
#include "HAL.h"
#include "app_bringup.h"
#include "app_config.h"
#include "gattprofile.h"
#include "northpole_broadcaster.h"
#include "northpole_ch592_port.h"
#include "peripheral.h"

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#if(defined(BLE_MAC)) && (BLE_MAC == TRUE)
const uint8_t MacAddr[6] = {0x84, 0xC2, 0xE4, 0x03, 0x02, 0x02};
#endif

__HIGH_CODE
__attribute__((noinline))
void Main_Circulation(void)
{
    while(1)
    {
        TMOS_SystemProcess();
#if (!APP_DEV_BOARD_BLE_SMOKE || APP_DEV_BOARD_BRINGUP_APP_SMOKE) && !APP_DEV_BOARD_BLE_BROADCASTER_SMOKE
        app_bringup_poll();
#endif
    }
}

int main(void)
{
#if(defined(DCDC_ENABLE)) && (DCDC_ENABLE == TRUE)
    PWR_DCDCCfg(ENABLE);
#endif

    SetSysClock(CLK_SOURCE_PLL_60MHz);
#if !APP_DEV_BOARD_SKIP_NORTHPOLE_SAFE_PINS
    northpole_ch592_early_safe_pins();
#endif

#if(defined(HAL_SLEEP)) && (HAL_SLEEP == TRUE)
    GPIOA_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
    /* HAL sleep examples blanket-configure GPIOs. Reassert motor-safe outputs
     * immediately so DRV8837 IN pins and PB0 /SLEEP are safe before BLE/HAL/app init.
     * Dev-board smoke builds deliberately skip this because NorthPole motor
     * pins conflict with CH592X-EVT-R1-LinkE crystal/USB/LCD pins.
     */
#if !APP_DEV_BOARD_SKIP_NORTHPOLE_SAFE_PINS
    northpole_ch592_early_safe_pins();
#endif
#endif

#if APP_DEV_BOARD_BLE_SMOKE || APP_DEV_BOARD_BLE_BROADCASTER_SMOKE || APP_DEV_BOARD_BRINGUP_APP_SMOKE
#ifdef DEBUG
    /* Keep dev-board BLE smoke startup close to WCH EVT examples. The proven
     * Broadcaster path initializes default UART1 and prints VER_LIB before
     * CH59x_BLEInit(); preserve that sequence while isolating BLE startup.
     */
    GPIOA_SetBits(bTXD1);
    GPIOA_ModeCfg(bTXD1, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
#endif
    PRINT("%s\n", VER_LIB);
#else
#ifdef DEBUG
    northpole_ch592_debug_uart_init();
#endif
#endif

    CH59x_BLEInit();
    HAL_Init();
#if APP_DEV_BOARD_BLE_BROADCASTER_SMOKE
    GAPRole_BroadcasterInit();
    NorthPole_Broadcaster_Init();
#else
    GAPRole_PeripheralInit();
    Peripheral_Init();
#if !APP_DEV_BOARD_BLE_SMOKE || APP_DEV_BOARD_BRINGUP_APP_SMOKE
    app_bringup_init();
#endif
#endif
    Main_Circulation();
}
