#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>

void timebase_init(void);
uint32_t timebase_ms(void);
void timebase_advance_ms(uint32_t delta_ms);
void timebase_delay_ms(uint32_t delay_ms);

#endif /* TIMEBASE_H */
