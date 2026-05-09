#include "timebase.h"

#if defined(__GNUC__)
#define FW_WEAK __attribute__((weak))
#else
#define FW_WEAK
#endif

static volatile uint32_t synthetic_ms;

FW_WEAK uint32_t timebase_platform_ms(void)
{
    return synthetic_ms;
}

FW_WEAK void timebase_platform_delay_ms(uint32_t delay_ms)
{
    synthetic_ms += delay_ms;
}

void timebase_init(void)
{
    synthetic_ms = 0;
}

uint32_t timebase_ms(void)
{
    return timebase_platform_ms();
}

void timebase_advance_ms(uint32_t delta_ms)
{
    synthetic_ms += delta_ms;
}

void timebase_delay_ms(uint32_t delay_ms)
{
    timebase_platform_delay_ms(delay_ms);
}
