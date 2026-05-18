#include "CONFIG.h"
#include "northpole_broadcaster.h"

#define NORTHPOLE_BROADCASTER_ADV_INTERVAL 160u

static uint8_t northpole_broadcaster_task_id = INVALID_TASK_ID;

static uint8_t scan_rsp_data[] = {
    0x0E,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'N', 'o', 'r', 't', 'h', 'P', 'o', 'l', 'e', ' ', 'B', 'L', 'E',

    0x02,
    GAP_ADTYPE_POWER_LEVEL,
    0
};

static uint8_t advert_data[] = {
    0x02,
    GAP_ADTYPE_FLAGS,
    GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

    0x05,
    GAP_ADTYPE_MANUFACTURER_SPECIFIC,
    'N', 'P', 'O', 'L',

    0x04,
    GAP_ADTYPE_LOCAL_NAME_SHORT,
    'N', 'P', 'B'
};

static void northpole_broadcaster_process_tmos_msg(tmos_event_hdr_t *msg);
static void northpole_broadcaster_state_cb(gapRole_States_t new_state);

static gapRolesBroadcasterCBs_t broadcaster_callbacks = {
    northpole_broadcaster_state_cb,
    NULL
};

void NorthPole_Broadcaster_Init(void)
{
    uint8_t advertising_enable = TRUE;
    uint8_t adv_event_type = GAP_ADTYPE_ADV_NONCONN_IND;
    uint16_t adv_int = NORTHPOLE_BROADCASTER_ADV_INTERVAL;

    northpole_broadcaster_task_id = TMOS_ProcessEventRegister(NorthPole_Broadcaster_ProcessEvent);

    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(advertising_enable), &advertising_enable);
    GAPRole_SetParameter(GAPROLE_ADV_EVENT_TYPE, sizeof(adv_event_type), &adv_event_type);
    GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scan_rsp_data), scan_rsp_data);
    GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advert_data), advert_data);

    GAP_SetParamValue(TGAP_DISC_ADV_INT_MIN, adv_int);
    GAP_SetParamValue(TGAP_DISC_ADV_INT_MAX, adv_int);

    tmos_set_event(northpole_broadcaster_task_id, NORTHPOLE_BROADCASTER_START_DEVICE_EVT);
}

uint16_t NorthPole_Broadcaster_ProcessEvent(uint8_t task_id, uint16_t events)
{
    (void)task_id;

    if (events & SYS_EVENT_MSG) {
        uint8_t *msg = tmos_msg_receive(northpole_broadcaster_task_id);
        if (msg != NULL) {
            northpole_broadcaster_process_tmos_msg((tmos_event_hdr_t *)msg);
            tmos_msg_deallocate(msg);
        }
        return (uint16_t)(events ^ SYS_EVENT_MSG);
    }

    if (events & NORTHPOLE_BROADCASTER_START_DEVICE_EVT) {
        GAPRole_BroadcasterStartDevice(&broadcaster_callbacks);
        return (uint16_t)(events ^ NORTHPOLE_BROADCASTER_START_DEVICE_EVT);
    }

    return 0;
}

static void northpole_broadcaster_process_tmos_msg(tmos_event_hdr_t *msg)
{
    (void)msg;
}

static void northpole_broadcaster_state_cb(gapRole_States_t new_state)
{
    switch (new_state) {
    case GAPROLE_STARTED:
        PRINT("NorthPole broadcaster initialized\n");
        break;
    case GAPROLE_ADVERTISING:
        PRINT("NorthPole broadcaster advertising\n");
        break;
    case GAPROLE_WAITING:
        PRINT("NorthPole broadcaster waiting\n");
        break;
    case GAPROLE_ERROR:
        PRINT("NorthPole broadcaster error\n");
        break;
    default:
        break;
    }
}
