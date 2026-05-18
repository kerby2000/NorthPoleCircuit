#ifndef BROADCASTER_LADDER_H
#define BROADCASTER_LADDER_H

#ifdef __cplusplus
extern "C" {
#endif

#define LADDER_START_DEVICE_EVT 0x0001

void Broadcaster_Ladder_Init(void);
uint16_t Broadcaster_Ladder_ProcessEvent(uint8_t task_id, uint16_t events);

#ifdef __cplusplus
}
#endif

#endif /* BROADCASTER_LADDER_H */
