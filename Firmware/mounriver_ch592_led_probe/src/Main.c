/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Author             : WCH / North Pole probe
 * Description        : CH592X-EVT-R1-LinkE LED probe.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "CH59x_common.h"

#define LED2_PIN    GPIO_Pin_4
#define LED1_PIN    GPIO_Pin_23

static void led_probe_init(void)
{
    /* CH592X-EVT-R1-LinkE schematic:
     * D4 / LED2: 3V3 -> LED -> R10 -> PA4, active low.
     * D3 / LED1: 3V3 -> LED -> R7 -> PB23, active low and shared with Download.
     */
    GPIOA_SetBits(LED2_PIN);
    GPIOA_ModeCfg(LED2_PIN, GPIO_ModeOut_PP_20mA);

    GPIOB_SetBits(LED1_PIN);
    GPIOB_ModeCfg(LED1_PIN, GPIO_ModeOut_PP_20mA);
}

int main(void)
{
    SetSysClock(CLK_SOURCE_PLL_60MHz);
    led_probe_init();

    while(1)
    {
        GPIOA_InverseBits(LED2_PIN);
        DelayMs(250);
        GPIOB_InverseBits(LED1_PIN);
        DelayMs(250);
    }
}
