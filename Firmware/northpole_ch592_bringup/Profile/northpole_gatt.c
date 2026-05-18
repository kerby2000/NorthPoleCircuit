#include "CONFIG.h"
#include "northpole_gatt.h"

#include "app_config.h"
#include "audio_wt2003.h"
#include "battery.h"
#include "ble_service.h"
#include "build_profile.h"
#include "fault.h"
#include "hall.h"
#include "motor_drv8837.h"
#include "power_ip5209.h"
#include "rgb_ws2812.h"
#include "settings.h"
#include "timebase.h"
#include "touch.h"

#include <string.h>

static bStatus_t northpole_ReadAttrCB(uint16_t connHandle,
                                      gattAttribute_t *pAttr,
                                      uint8_t *pValue,
                                      uint16_t *pLen,
                                      uint16_t offset,
                                      uint16_t maxLen,
                                      uint8_t method);
static bStatus_t northpole_WriteAttrCB(uint16_t connHandle,
                                       gattAttribute_t *pAttr,
                                       uint8_t *pValue,
                                       uint16_t len,
                                       uint16_t offset,
                                       uint8_t method);

static const uint8_t northpoleServiceUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(NORTHPOLE_DIAG_SERVICE_UUID), HI_UINT16(NORTHPOLE_DIAG_SERVICE_UUID)
};
static const uint8_t northpoleVersionUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(NORTHPOLE_DIAG_VERSION_UUID), HI_UINT16(NORTHPOLE_DIAG_VERSION_UUID)
};
static const uint8_t northpoleBoardUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(NORTHPOLE_DIAG_BOARD_UUID), HI_UINT16(NORTHPOLE_DIAG_BOARD_UUID)
};
static const uint8_t northpoleStatusUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(NORTHPOLE_DIAG_STATUS_UUID), HI_UINT16(NORTHPOLE_DIAG_STATUS_UUID)
};
static const uint8_t northpoleProfileUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(NORTHPOLE_DIAG_PROFILE_UUID), HI_UINT16(NORTHPOLE_DIAG_PROFILE_UUID)
};
static const uint8_t northpoleCountersUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(NORTHPOLE_DIAG_COUNTERS_UUID), HI_UINT16(NORTHPOLE_DIAG_COUNTERS_UUID)
};
static const uint8_t northpoleControlUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(NORTHPOLE_DIAG_CONTROL_UUID), HI_UINT16(NORTHPOLE_DIAG_CONTROL_UUID)
};

static const gattAttrType_t northpoleService = {ATT_BT_UUID_SIZE, northpoleServiceUUID};

static uint8_t readProps = GATT_PROP_READ;
static uint8_t writeProps = GATT_PROP_WRITE;
static uint8_t versionValue[20];
static uint8_t boardValue[20];
static uint8_t profileValue[20];
static uint8_t statusValue[20];
static uint8_t countersValue[20];
static uint8_t controlValue[8];

static gattAttribute_t northpoleAttrTbl[] = {
    {{ATT_BT_UUID_SIZE, primaryServiceUUID}, GATT_PERMIT_READ, 0, (uint8_t *)&northpoleService},

    {{ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &readProps},
    {{ATT_BT_UUID_SIZE, northpoleVersionUUID}, GATT_PERMIT_READ, 0, versionValue},

    {{ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &readProps},
    {{ATT_BT_UUID_SIZE, northpoleBoardUUID}, GATT_PERMIT_READ, 0, boardValue},

    {{ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &readProps},
    {{ATT_BT_UUID_SIZE, northpoleProfileUUID}, GATT_PERMIT_READ, 0, profileValue},

    {{ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &readProps},
    {{ATT_BT_UUID_SIZE, northpoleStatusUUID}, GATT_PERMIT_READ, 0, statusValue},

    {{ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &readProps},
    {{ATT_BT_UUID_SIZE, northpoleCountersUUID}, GATT_PERMIT_READ, 0, countersValue},

    {{ATT_BT_UUID_SIZE, characterUUID}, GATT_PERMIT_READ, 0, &writeProps},
    {{ATT_BT_UUID_SIZE, northpoleControlUUID}, GATT_PERMIT_WRITE, 0, controlValue},
};

static gattServiceCBs_t northpoleCBs = {
    northpole_ReadAttrCB,
    northpole_WriteAttrCB,
    NULL
};

static void put_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
}

static bStatus_t read_string(uint8_t *dst, uint16_t *out_len, const char *src,
                             uint16_t offset, uint16_t maxLen)
{
    uint16_t len = (uint16_t)strlen(src);
    uint16_t remaining;

    if (offset > len) {
        *out_len = 0;
        return ATT_ERR_INVALID_OFFSET;
    }
    remaining = (uint16_t)(len - offset);
    if (remaining > maxLen) {
        remaining = maxLen;
    }
    tmos_memcpy(dst, (void *)(src + offset), remaining);
    *out_len = remaining;
    return SUCCESS;
}

static uint16_t build_status(uint8_t *dst, uint16_t maxLen)
{
    power_ip5209_status_t power = power_ip5209_status();
    audio_wt2003_report_t audio = audio_wt2003_report();
    const settings_t *settings = settings_get();

    if (maxLen > 20u) {
        maxLen = 20u;
    }
    memset(dst, 0, maxLen);
    if (maxLen < 16u) {
        return maxLen;
    }

    put_u32_le(&dst[0], timebase_ms());
    put_u32_le(&dst[4], fault_snapshot());
    dst[8] = 0xFFu; /* battery status placeholder: UNKNOWN */
    dst[9] = power.i2c_present;
    dst[10] = power.int_level;
    dst[11] = (uint8_t)audio.hw_state;
    dst[12] = motor_drv8837_is_armed();
    dst[13] = settings->brightness;
    dst[14] = settings_valid();
    dst[15] = (uint8_t)ble_service_state();
    return 16u;
}

static uint16_t build_counters(uint8_t *dst, uint16_t maxLen)
{
    uint8_t touch_mask = 0;
    hall_state_t hall1 = hall_get_state(HALL_SENSOR_1);
    hall_state_t hall2 = hall_get_state(HALL_SENSOR_2);

    if (maxLen > 20u) {
        maxLen = 20u;
    }
    memset(dst, 0, maxLen);
    if (maxLen < 12u) {
        return maxLen;
    }

    for (uint8_t i = 0; i < TOUCH_COUNT; ++i) {
        if (touch_get_state((touch_pad_id_t)i).pressed) {
            touch_mask |= (uint8_t)(1u << i);
        }
    }

    put_u32_le(&dst[0], hall1.edge_count);
    put_u32_le(&dst[4], hall2.edge_count);
    dst[8] = hall1.level;
    dst[9] = hall2.level;
    dst[10] = touch_mask;
    dst[11] = 0xFFu; /* touch event placeholder */
    return 12u;
}

bStatus_t NorthPoleDiag_AddService(void)
{
    return GATTServApp_RegisterService(northpoleAttrTbl,
                                       GATT_NUM_ATTRS(northpoleAttrTbl),
                                       GATT_MAX_ENCRYPT_KEY_SIZE,
                                       &northpoleCBs);
}

static bStatus_t northpole_ReadAttrCB(uint16_t connHandle,
                                      gattAttribute_t *pAttr,
                                      uint8_t *pValue,
                                      uint16_t *pLen,
                                      uint16_t offset,
                                      uint16_t maxLen,
                                      uint8_t method)
{
    uint16_t uuid;
    (void)connHandle;
    (void)method;

    if (pAttr->type.len != ATT_BT_UUID_SIZE) {
        *pLen = 0;
        return ATT_ERR_INVALID_HANDLE;
    }

    uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);
    switch (uuid) {
    case NORTHPOLE_DIAG_VERSION_UUID:
        return read_string(pValue, pLen, APP_FIRMWARE_VERSION, offset, maxLen);
    case NORTHPOLE_DIAG_BOARD_UUID:
        return read_string(pValue, pLen, APP_BOARD_REVISION, offset, maxLen);
    case NORTHPOLE_DIAG_PROFILE_UUID:
        return read_string(pValue, pLen, BUILD_PROFILE_NAME, offset, maxLen);
    case NORTHPOLE_DIAG_STATUS_UUID:
        if (offset > 0) {
            *pLen = 0;
            return ATT_ERR_ATTR_NOT_LONG;
        }
        *pLen = build_status(pValue, maxLen);
        return SUCCESS;
    case NORTHPOLE_DIAG_COUNTERS_UUID:
        if (offset > 0) {
            *pLen = 0;
            return ATT_ERR_ATTR_NOT_LONG;
        }
        *pLen = build_counters(pValue, maxLen);
        return SUCCESS;
    default:
        *pLen = 0;
        return ATT_ERR_ATTR_NOT_FOUND;
    }
}

static bStatus_t northpole_WriteAttrCB(uint16_t connHandle,
                                       gattAttribute_t *pAttr,
                                       uint8_t *pValue,
                                       uint16_t len,
                                       uint16_t offset,
                                       uint8_t method)
{
    uint16_t uuid;
    (void)connHandle;
    (void)method;

    if (offset > 0) {
        return ATT_ERR_ATTR_NOT_LONG;
    }
    if (pAttr->type.len != ATT_BT_UUID_SIZE) {
        return ATT_ERR_INVALID_HANDLE;
    }

    uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);
    if (uuid != NORTHPOLE_DIAG_CONTROL_UUID) {
        return ATT_ERR_ATTR_NOT_FOUND;
    }
    if (len == 0 || len > sizeof(controlValue)) {
        return ATT_ERR_INVALID_VALUE_SIZE;
    }

    tmos_memcpy(controlValue, pValue, len);
#if APP_DEV_BOARD_BRINGUP_APP_SMOKE
    if (pValue[0] != NORTHPOLE_DIAG_CONTROL_CLEAR_FAULTS) {
        return ATT_ERR_UNLIKELY;
    }
#endif
    switch (pValue[0]) {
    case NORTHPOLE_DIAG_CONTROL_RGB_ALL:
        if (len < 4u) {
            return ATT_ERR_INVALID_VALUE_SIZE;
        } else {
            rgb_color_t color = {pValue[1], pValue[2], pValue[3]};
            for (uint8_t i = 0; i < APP_RGB_LED_COUNT; ++i) {
                rgb_ws2812_set(i, color);
            }
            rgb_ws2812_show();
        }
        return SUCCESS;
    case NORTHPOLE_DIAG_CONTROL_CLEAR_FAULTS:
        fault_clear_all();
        return SUCCESS;
    case NORTHPOLE_DIAG_CONTROL_AUDIO_STOP:
        return wt2003_stop() == AUDIO_STATUS_OK ? SUCCESS : ATT_ERR_UNLIKELY;
    case NORTHPOLE_DIAG_CONTROL_AUDIO_PLAY_INDEX:
        if (len < 3u) {
            return ATT_ERR_INVALID_VALUE_SIZE;
        }
        return wt2003_play_external_index(BUILD_UINT16(pValue[1], pValue[2])) == AUDIO_STATUS_OK ? SUCCESS : ATT_ERR_UNLIKELY;
    case NORTHPOLE_DIAG_CONTROL_AUDIO_VOLUME:
        if (len < 2u) {
            return ATT_ERR_INVALID_VALUE_SIZE;
        }
        if (pValue[1] > 31u) {
            return ATT_ERR_INVALID_VALUE;
        }
        return wt2003_set_volume(pValue[1]) == AUDIO_STATUS_OK ? SUCCESS : ATT_ERR_UNLIKELY;
    case NORTHPOLE_DIAG_CONTROL_AUDIO_PAUSE:
        return wt2003_pause_resume() == AUDIO_STATUS_OK ? SUCCESS : ATT_ERR_UNLIKELY;
    case NORTHPOLE_DIAG_CONTROL_AUDIO_QSTATUS:
        return wt2003_query_status() == AUDIO_STATUS_OK ? SUCCESS : ATT_ERR_UNLIKELY;
    default:
        return ATT_ERR_INVALID_VALUE;
    }
}
