#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <stdint.h>

typedef enum {
    BLE_SERVICE_IDLE = 0,
    BLE_SERVICE_ADVERTISING,
    BLE_SERVICE_CONNECTED,
    BLE_SERVICE_ERROR,
} ble_service_state_t;

void ble_service_init(void);
void ble_service_poll(void);
ble_service_state_t ble_service_state(void);
void ble_service_start_advertising(void);
void ble_service_mark_connected(void);
void ble_service_mark_idle(void);
void ble_service_mark_error(void);
const char *ble_service_state_name(ble_service_state_t state);

#endif /* BLE_SERVICE_H */
