#ifndef NORTHPOLE_CH592_PORT_H
#define NORTHPOLE_CH592_PORT_H

#include <stdint.h>

void northpole_ch592_early_safe_pins(void);
void northpole_ch592_debug_uart_init(void);
void northpole_ch592_audio_uart_init(void);

#define NORTHPOLE_MOTOR_WAVE_TARGET_A 0x01u
#define NORTHPOLE_MOTOR_WAVE_TARGET_B 0x02u
#define NORTHPOLE_MOTOR_WAVE_TARGET_G 0x04u

#define NORTHPOLE_MOTOR_GUARD_OFF 0u
#define NORTHPOLE_MOTOR_GUARD_FORWARD 1u
#define NORTHPOLE_MOTOR_GUARD_REVERSE 2u
#define NORTHPOLE_MOTOR_GUARD_PHASE_A 3u
#define NORTHPOLE_MOTOR_GUARD_PHASE_B 4u

typedef struct {
    uint8_t running;
    uint8_t target_flags;
    uint8_t sleep_high;
    uint8_t dma_mode;
    uint8_t dma_supported_flags;
    uint8_t dma_error;
    uint8_t phase;
    int8_t direction;
    uint8_t guard_mode;
    uint16_t amplitude_permille;
    uint16_t guard_duty_permille;
    uint32_t carrier_hz;
    uint32_t electrical_hz_x1000;
    uint32_t sample_ticks;
    uint32_t tick_count;
    uint32_t dma_entries;
    uint32_t dma_repeat_per_sample;
} northpole_motor_wave_status_t;

int northpole_motor_wave_start(uint32_t electrical_hz_x1000,
                               uint16_t amplitude_permille,
                               uint8_t target_flags,
                               uint8_t sleep_high);
int northpole_motor_wave_start_ex(uint32_t electrical_hz_x1000,
                                  uint16_t amplitude_permille,
                                  uint8_t target_flags,
                                  uint8_t sleep_high,
                                  int8_t direction,
                                  uint8_t guard_mode,
                                  uint16_t guard_duty_permille);
int northpole_motor_wave_start_slot_us(uint32_t slot_us,
                                       uint16_t amplitude_permille,
                                       uint8_t target_flags,
                                       uint8_t sleep_high);
int northpole_motor_wave_start_slot_us_ex(uint32_t slot_us,
                                          uint16_t amplitude_permille,
                                          uint8_t target_flags,
                                          uint8_t sleep_high,
                                          int8_t direction,
                                          uint8_t guard_mode,
                                          uint16_t guard_duty_permille);
int northpole_motor_wave_dma_a_start_slot_us(uint32_t slot_us,
                                             uint16_t amplitude_permille,
                                             uint8_t sleep_high);
int northpole_motor_wave_dma_hybrid_start_slot_us(uint32_t slot_us,
                                                  uint16_t amplitude_permille,
                                                  uint8_t target_flags,
                                                  uint8_t sleep_high);
void northpole_motor_wave_stop(void);
void northpole_motor_wave_status(northpole_motor_wave_status_t *status);
const char *northpole_motor_guard_mode_name(uint8_t guard_mode);

#endif /* NORTHPOLE_CH592_PORT_H */
