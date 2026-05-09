#ifndef HALL_H
#define HALL_H

#include <stdint.h>

typedef enum {
    HALL_SENSOR_1 = 0,
    HALL_SENSOR_2,
    HALL_SENSOR_COUNT,
} hall_sensor_id_t;

typedef struct {
    uint8_t level;
    uint32_t edge_count;
    uint32_t last_edge_ms;
} hall_state_t;

void hall_init(void);
void hall_poll(void);
uint8_t hall_read(hall_sensor_id_t sensor);
hall_state_t hall_get_state(hall_sensor_id_t sensor);

#endif /* HALL_H */
