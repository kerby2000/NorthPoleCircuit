/********************************** (C) COPYRIGHT *******************************
 * File Name          : peripheral_main.c
 * Author             : WCH / North Pole firmware bring-up
 * Version            : V0.1
 * Description        : CH592 BLE peripheral startup with North Pole safe bring-up.
 ********************************************************************************/

#include "CONFIG.h"
#include "HAL.h"
#include "app_bringup.h"
#include "gattprofile.h"
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
        app_bringup_poll();
    }
}

int main(void)
{
#if(defined(DCDC_ENABLE)) && (DCDC_ENABLE == TRUE)
    PWR_DCDCCfg(ENABLE);
#endif

    SetSysClock(CLK_SOURCE_PLL_60MHz);
    northpole_ch592_early_safe_pins();

#if(defined(HAL_SLEEP)) && (HAL_SLEEP == TRUE)
    GPIOA_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
    /* HAL sleep examples blanket-configure GPIOs. Reassert motor-safe outputs
     * immediately so DRV8837 IN pins and PB0 /SLEEP are safe before BLE/HAL/app init.
     */
    northpole_ch592_early_safe_pins();
#endif

#ifdef DEBUG
    northpole_ch592_debug_uart_init();
#endif

    CH59x_BLEInit();
    HAL_Init();
    GAPRole_PeripheralInit();
    Peripheral_Init();
    app_bringup_init();
    Main_Circulation();
}
