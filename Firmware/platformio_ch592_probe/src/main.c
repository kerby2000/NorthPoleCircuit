#include <CH59x_common.h>

#define PROBE_LED_PIN GPIO_Pin_4

int main(void)
{
    SetSysClock(CLK_SOURCE_PLL_60MHz);

    /* CH592X-EVT-R1-LinkE schematic: LED2 is tied to 3V3 and PA4, active low.
       LED1 is on PB23, shared with the Download button, so avoid it here. */
    GPIOA_SetBits(PROBE_LED_PIN);
    GPIOA_ModeCfg(PROBE_LED_PIN, GPIO_ModeOut_PP_20mA);

    while (1) {
        DelayMs(500);
        GPIOA_InverseBits(PROBE_LED_PIN);
    }
}
