#include "ble_service.h"

static ble_service_state_t current_state = BLE_SERVICE_IDLE;

void ble_service_init(void)
{
    current_state = BLE_SERVICE_IDLE;
}

void ble_service_poll(void)
{
}

ble_service_state_t ble_service_state(void)
{
    return current_state;
}

void ble_service_start_advertising(void)
{
    current_state = BLE_SERVICE_ADVERTISING;
}

void ble_service_mark_connected(void)
{
    current_state = BLE_SERVICE_CONNECTED;
}

void ble_service_mark_idle(void)
{
    current_state = BLE_SERVICE_IDLE;
}

void ble_service_mark_error(void)
{
    current_state = BLE_SERVICE_ERROR;
}

const char *ble_service_state_name(ble_service_state_t state)
{
    switch (state) {
    case BLE_SERVICE_IDLE: return "idle";
    case BLE_SERVICE_ADVERTISING: return "advertising";
    case BLE_SERVICE_CONNECTED: return "connected";
    case BLE_SERVICE_ERROR: return "error";
    default: return "?";
    }
}
