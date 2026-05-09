#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

typedef struct {
    uint8_t version;
    uint8_t volume;
    uint8_t brightness;
    uint8_t default_scene;
    uint8_t motor_intensity_limit;
    uint8_t demo_mode;
    uint16_t reserved;
    uint16_t crc;
} settings_t;

void settings_init(void);
const settings_t *settings_get(void);
void settings_set(const settings_t *settings);
int settings_save(void);
void settings_factory_reset(void);
void settings_corrupt_for_test(void);
uint8_t settings_valid(void);
uint16_t settings_crc16(const settings_t *settings);

#endif /* SETTINGS_H */
