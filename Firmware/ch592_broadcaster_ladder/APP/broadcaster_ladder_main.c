#include "CONFIG.h"
#include "HAL.h"
#include "broadcaster_ladder.h"

#ifndef LADDER_USE_NORTHPOLE_BROADCASTER
#define LADDER_USE_NORTHPOLE_BROADCASTER 0
#endif

#ifndef LADDER_LINK_NORTHPOLE_MODULES
#define LADDER_LINK_NORTHPOLE_MODULES 0
#endif

#ifndef LADDER_INIT_NORTHPOLE_CORE
#define LADDER_INIT_NORTHPOLE_CORE 0
#endif

#ifndef LADDER_INIT_NORTHPOLE_BOARD_SAFE
#define LADDER_INIT_NORTHPOLE_BOARD_SAFE 0
#endif

#ifndef LADDER_LINK_FULL_NORTHPOLE_APP
#define LADDER_LINK_FULL_NORTHPOLE_APP 0
#endif

#if LADDER_USE_NORTHPOLE_BROADCASTER
#include "northpole_broadcaster.h"
#endif

#if LADDER_INIT_NORTHPOLE_CORE
#include "fault.h"
#include "log.h"
#include "settings.h"
#include "timebase.h"
#endif

#if LADDER_INIT_NORTHPOLE_BOARD_SAFE
#include "board.h"
#endif

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#if(defined(BLE_MAC)) && (BLE_MAC == TRUE)
const uint8_t MacAddr[6] = {0x84, 0xC2, 0xE4, 0x03, 0x02, 0x02};
#endif

__HIGH_CODE
__attribute__((noinline))
void Main_Circulation(void)
{
    while(1) {
        TMOS_SystemProcess();
    }
}

int main(void)
{
#if(defined(DCDC_ENABLE)) && (DCDC_ENABLE == TRUE)
    PWR_DCDCCfg(ENABLE);
#endif

    SetSysClock(CLK_SOURCE_PLL_60MHz);

#if LADDER_INIT_NORTHPOLE_CORE
    timebase_init();
    log_init(LOG_LEVEL_INFO);
    fault_init();
    settings_init();
#endif

#if LADDER_INIT_NORTHPOLE_BOARD_SAFE
    board_init_safe_pins();
#endif

#if(defined(HAL_SLEEP)) && (HAL_SLEEP == TRUE)
    GPIOA_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
#endif

#ifdef DEBUG
    GPIOA_SetBits(bTXD1);
    GPIOA_ModeCfg(bTXD1, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
#endif

    PRINT("%s\n", VER_LIB);

#if LADDER_LINK_NORTHPOLE_MODULES
    PRINT("NorthPole support modules linked\n");
#endif

    CH59x_BLEInit();
    HAL_Init();
    GAPRole_BroadcasterInit();
#if LADDER_USE_NORTHPOLE_BROADCASTER
    NorthPole_Broadcaster_Init();
#else
    Broadcaster_Ladder_Init();
#endif
    Main_Circulation();
}
