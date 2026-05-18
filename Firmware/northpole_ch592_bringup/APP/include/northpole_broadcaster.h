#ifndef NORTHPOLE_BROADCASTER_H
#define NORTHPOLE_BROADCASTER_H

#ifdef __cplusplus
extern "C" {
#endif

#define NORTHPOLE_BROADCASTER_START_DEVICE_EVT 0x0001

void NorthPole_Broadcaster_Init(void);
uint16_t NorthPole_Broadcaster_ProcessEvent(uint8_t task_id, uint16_t events);

#ifdef __cplusplus
}
#endif

#endif /* NORTHPOLE_BROADCASTER_H */
