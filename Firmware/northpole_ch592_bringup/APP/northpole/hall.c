#include "hall.h"

#include "board.h"
#include "timebase.h"

static hall_state_t hall_states[HALL_SENSOR_COUNT];

static const board_input_id_t hall_inputs[HALL_SENSOR_COUNT] = {
    BOARD_INPUT_HALL1,
    BOARD_INPUT_HALL2,
};

void hall_init(void)
{
    for (uint8_t i = 0; i < HALL_SENSOR_COUNT; ++i) {
        hall_states[i].level = board_input_read(hall_inputs[i]);
        hall_states[i].edge_count = 0;
        hall_states[i].last_edge_ms = 0;
    }
}

void hall_poll(void)
{
    for (uint8_t i = 0; i < HALL_SENSOR_COUNT; ++i) {
        uint8_t level = board_input_read(hall_inputs[i]);
        if (level != hall_states[i].level) {
            hall_states[i].level = level;
            hall_states[i].edge_count++;
            hall_states[i].last_edge_ms = timebase_ms();
        }
    }
}

uint8_t hall_read(hall_sensor_id_t sensor)
{
    return sensor < HALL_SENSOR_COUNT ? hall_states[sensor].level : 0u;
}

hall_state_t hall_get_state(hall_sensor_id_t sensor)
{
    hall_state_t empty = {0, 0, 0};
    return sensor < HALL_SENSOR_COUNT ? hall_states[sensor] : empty;
}
