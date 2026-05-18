#include "CONFIG.h"
#include "broadcaster_ladder.h"

#define LADDER_DEFAULT_ADVERTISING_INTERVAL 160u

#ifndef LADDER_ADV_NORTHPOLE
#define LADDER_ADV_NORTHPOLE 0
#endif

static uint8_t broadcaster_ladder_task_id;

#if LADDER_ADV_NORTHPOLE
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
#else
static uint8_t scan_rsp_data[] = {
    0x0C,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'B', 'r', 'o', 'a', 'd', 'c', 'a', 's', 't', 'e', 'r',

    0x02,
    GAP_ADTYPE_POWER_LEVEL,
    0
};

static uint8_t advert_data[] = {
    0x02,
    GAP_ADTYPE_FLAGS,
    GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

    0x04,
    GAP_ADTYPE_MANUFACTURER_SPECIFIC,
    'b', 'l', 'e',

    0x04,
    GAP_ADTYPE_LOCAL_NAME_SHORT,
    'a', 'b', 'c'
};
#endif

static void broadcaster_ladder_process_tmos_msg(tmos_event_hdr_t *msg);
static void broadcaster_ladder_state_cb(gapRole_States_t new_state);

static gapRolesBroadcasterCBs_t broadcaster_callbacks = {
    broadcaster_ladder_state_cb,
    NULL
};

void Broadcaster_Ladder_Init(void)
{
    uint8_t initial_advertising_enable = TRUE;
    uint8_t initial_adv_event_type = GAP_ADTYPE_ADV_NONCONN_IND;
    uint16_t adv_int = LADDER_DEFAULT_ADVERTISING_INTERVAL;

    broadcaster_ladder_task_id = TMOS_ProcessEventRegister(Broadcaster_Ladder_ProcessEvent);

    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(initial_advertising_enable), &initial_advertising_enable);
    GAPRole_SetParameter(GAPROLE_ADV_EVENT_TYPE, sizeof(initial_adv_event_type), &initial_adv_event_type);
    GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scan_rsp_data), scan_rsp_data);
    GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advert_data), advert_data);

    GAP_SetParamValue(TGAP_DISC_ADV_INT_MIN, adv_int);
    GAP_SetParamValue(TGAP_DISC_ADV_INT_MAX, adv_int);

    tmos_set_event(broadcaster_ladder_task_id, LADDER_START_DEVICE_EVT);
}

uint16_t Broadcaster_Ladder_ProcessEvent(uint8_t task_id, uint16_t events)
{
    (void)task_id;

    if (events & SYS_EVENT_MSG) {
        uint8_t *msg = tmos_msg_receive(broadcaster_ladder_task_id);
        if (msg != NULL) {
            broadcaster_ladder_process_tmos_msg((tmos_event_hdr_t *)msg);
            tmos_msg_deallocate(msg);
        }
        return (uint16_t)(events ^ SYS_EVENT_MSG);
    }

    if (events & LADDER_START_DEVICE_EVT) {
        GAPRole_BroadcasterStartDevice(&broadcaster_callbacks);
        return (uint16_t)(events ^ LADDER_START_DEVICE_EVT);
    }

    return 0;
}

static void broadcaster_ladder_process_tmos_msg(tmos_event_hdr_t *msg)
{
    (void)msg;
}

static void broadcaster_ladder_state_cb(gapRole_States_t new_state)
{
    switch (new_state) {
    case GAPROLE_STARTED:
        PRINT("Ladder initialized\n");
        break;
    case GAPROLE_ADVERTISING:
        PRINT("Ladder advertising\n");
        break;
    case GAPROLE_WAITING:
        PRINT("Ladder waiting\n");
        break;
    case GAPROLE_ERROR:
        PRINT("Ladder error\n");
        break;
    default:
        break;
    }
}
