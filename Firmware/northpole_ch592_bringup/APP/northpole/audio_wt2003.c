#include "audio_wt2003.h"

#include "app_config.h"
#include "board.h"
#include "fault.h"
#include "timebase.h"

#include <string.h>

#if defined(__GNUC__)
#define FW_WEAK __attribute__((weak))
#else
#define FW_WEAK
#endif

#define WT2003_QUEUE_DEPTH 4u

typedef struct {
    uint8_t frame[WT2003_MAX_FRAME_SIZE];
    uint8_t len;
    uint8_t wt_command;
    audio_command_t logical_command;
    uint16_t value;
    uint16_t timeout_ms;
    uint8_t retries_left;
} wt2003_queued_command_t;

FW_WEAK int audio_wt2003_platform_send(const uint8_t *data, uint8_t len)
{
    (void)data;
    (void)len;
    return -1;
}

FW_WEAK int audio_wt2003_platform_read_byte(uint8_t *byte)
{
    (void)byte;
    return 0;
}

static audio_wt2003_report_t report;
static wt2003_parser_t parser;
static wt2003_queued_command_t queue[WT2003_QUEUE_DEPTH];
static wt2003_queued_command_t active_command;
static uint8_t queue_head;
static uint8_t queue_tail;
static uint8_t active_valid;

static void wt2003_parser_reset(wt2003_parser_t *p)
{
    p->state = WT2003_PARSE_WAIT_START;
    p->length = 0;
    p->payload_pos = 0;
    p->started_ms = 0;
}

static uint8_t queue_count(void)
{
    uint8_t count;

    if (queue_tail >= queue_head) {
        count = (uint8_t)(queue_tail - queue_head);
    } else {
        count = (uint8_t)(WT2003_QUEUE_DEPTH - queue_head + queue_tail);
    }
    return count;
}

static uint8_t queue_next(uint8_t index)
{
    index++;
    if (index >= WT2003_QUEUE_DEPTH) {
        index = 0;
    }
    return index;
}

static int queue_full(void)
{
    return queue_next(queue_tail) == queue_head;
}

static void clear_queue(void)
{
    queue_head = 0;
    queue_tail = 0;
    active_valid = 0;
    report.pending = 0;
    report.queue_depth = 0;
}

bool wt2003_build_frame(uint8_t cmd,
                        const uint8_t *params,
                        size_t param_len,
                        uint8_t *out,
                        size_t out_cap,
                        size_t *out_len)
{
    uint8_t length;
    uint16_t checksum;
    size_t pos;

    if (!out || !out_len) {
        return false;
    }
    if (param_len > WT2003_MAX_PARAMS || (param_len > 0u && !params)) {
        return false;
    }
    if (out_cap < (param_len + 5u)) {
        return false;
    }

    length = (uint8_t)(1u + param_len + 1u + 1u);
    checksum = (uint16_t)(length + cmd);

    out[0] = WT2003_FRAME_START;
    out[1] = length;
    out[2] = cmd;
    pos = 3u;
    for (size_t i = 0; i < param_len; ++i) {
        out[pos++] = params[i];
        checksum = (uint16_t)(checksum + params[i]);
    }
    out[pos++] = (uint8_t)checksum;
    out[pos++] = WT2003_FRAME_END;
    *out_len = pos;
    return true;
}

void wt2003_parser_init(wt2003_parser_t *p)
{
    if (p) {
        memset(p, 0, sizeof(*p));
        wt2003_parser_reset(p);
    }
}

wt2003_parse_result_t wt2003_parse_byte(wt2003_parser_t *p,
                                        uint8_t byte,
                                        uint32_t now_ms,
                                        wt2003_frame_t *out)
{
    if (!p || !out) {
        return WT2003_PARSE_FRAMING_ERROR;
    }

    if (byte == WT2003_FRAME_START) {
        wt2003_parse_state_t previous_state = p->state;

        wt2003_parser_reset(p);
        p->state = WT2003_PARSE_LENGTH;
        p->started_ms = now_ms;
        return previous_state == WT2003_PARSE_END ? WT2003_PARSE_FRAMING_ERROR : WT2003_PARSE_NONE;
    }

    switch (p->state) {
    case WT2003_PARSE_WAIT_START:
        return WT2003_PARSE_NONE;

    case WT2003_PARSE_LENGTH:
        if (byte < 3u || byte > (WT2003_MAX_PARAMS + 3u)) {
            wt2003_parser_reset(p);
            return WT2003_PARSE_LENGTH_ERROR;
        }
        p->length = byte;
        p->payload_pos = 0;
        p->state = WT2003_PARSE_PAYLOAD;
        return WT2003_PARSE_NONE;

    case WT2003_PARSE_PAYLOAD:
        p->payload[p->payload_pos++] = byte;
        if (p->payload_pos >= (uint8_t)(p->length - 1u)) {
            p->state = WT2003_PARSE_END;
        }
        return WT2003_PARSE_NONE;

    case WT2003_PARSE_END:
        if (byte != WT2003_FRAME_END) {
            wt2003_parser_reset(p);
            return WT2003_PARSE_FRAMING_ERROR;
        } else {
            uint16_t checksum = p->length;
            uint8_t payload_len = (uint8_t)(p->length - 1u);
            size_t param_len = (size_t)(p->length - 3u);

            for (uint8_t i = 0; i < (uint8_t)(payload_len - 1u); ++i) {
                checksum = (uint16_t)(checksum + p->payload[i]);
            }
            if (((uint8_t)checksum) != p->payload[payload_len - 1u]) {
                wt2003_parser_reset(p);
                return WT2003_PARSE_CHECKSUM_ERROR;
            }

            out->command = p->payload[0];
            out->param_len = param_len;
            if (param_len > 0u) {
                memcpy(out->params, &p->payload[1], param_len);
            }
            out->checksum = p->payload[payload_len - 1u];
            wt2003_parser_reset(p);
            return WT2003_PARSE_FRAME;
        }

    default:
        wt2003_parser_reset(p);
        return WT2003_PARSE_FRAMING_ERROR;
    }
}

wt2003_parse_result_t wt2003_parser_tick(wt2003_parser_t *p,
                                         uint32_t now_ms,
                                         uint32_t timeout_ms)
{
    if (!p || p->state == WT2003_PARSE_WAIT_START || timeout_ms == 0u) {
        return WT2003_PARSE_NONE;
    }
    if ((uint32_t)(now_ms - p->started_ms) >= timeout_ms) {
        wt2003_parser_reset(p);
        return WT2003_PARSE_TIMEOUT;
    }
    return WT2003_PARSE_NONE;
}

static void store_last_tx(const uint8_t *data, uint8_t len)
{
    if (len > WT2003_MAX_FRAME_SIZE) {
        len = WT2003_MAX_FRAME_SIZE;
    }
    memcpy(report.last_tx, data, len);
    report.last_tx_len = len;
}

static void store_last_rx(const wt2003_frame_t *frame)
{
    size_t len = 0;

    if (wt2003_build_frame(frame->command,
                           frame->params,
                           frame->param_len,
                           report.last_rx,
                           sizeof(report.last_rx),
                           &len)) {
        report.last_rx_len = (uint8_t)len;
    }
}

static audio_status_t reject_command(audio_command_t logical_command,
                                     uint8_t wt_command,
                                     uint16_t value,
                                     audio_status_t status)
{
    report.last_command = logical_command;
    report.last_wt_command = wt_command;
    report.last_value = value;
    report.last_error = status;
    return status;
}

static int hardware_blocked(void)
{
#if AUDIO_WT2003_HW_BLOCKED || !APP_AUDIO_UART_CONNECTED
    return 1;
#else
    return 0;
#endif
}

bool wt2003_is_ready(void)
{
    if (hardware_blocked() || report.hw_state == AUDIO_HW_BLOCKED) {
        return false;
    }
    return (int32_t)(timebase_ms() - report.ready_after_ms) >= 0;
}

static audio_status_t enqueue_prebuilt(audio_command_t logical_command,
                                       uint8_t wt_command,
                                       uint16_t value,
                                       const uint8_t *frame,
                                       size_t frame_len,
                                       uint16_t timeout_ms)
{
    wt2003_queued_command_t *slot;

    if (!frame || frame_len == 0u || frame_len > WT2003_MAX_FRAME_SIZE) {
        return reject_command(logical_command, wt_command, value, AUDIO_STATUS_BAD_ARGUMENT);
    }
    if (hardware_blocked()) {
        report.hw_state = AUDIO_HW_BLOCKED;
        fault_raise(FAULT_AUDIO_HW_BLOCKED);
        return reject_command(logical_command, wt_command, value, AUDIO_STATUS_HW_BLOCKED);
    }
    if (!wt2003_is_ready()) {
        return reject_command(logical_command, wt_command, value, AUDIO_STATUS_NOT_READY);
    }
    if (queue_full()) {
        return reject_command(logical_command, wt_command, value, AUDIO_STATUS_QUEUE_FULL);
    }

    slot = &queue[queue_tail];
    memset(slot, 0, sizeof(*slot));
    memcpy(slot->frame, frame, frame_len);
    slot->len = (uint8_t)frame_len;
    slot->wt_command = wt_command;
    slot->logical_command = logical_command;
    slot->value = value;
    slot->timeout_ms = timeout_ms == 0u ? APP_AUDIO_COMMAND_TIMEOUT_MS : timeout_ms;
    slot->retries_left = APP_AUDIO_RETRY_COUNT;
    queue_tail = queue_next(queue_tail);
    report.queue_depth = queue_count();
    return AUDIO_STATUS_OK;
}

static audio_status_t enqueue_command(audio_command_t logical_command,
                                      uint8_t wt_command,
                                      const uint8_t *params,
                                      size_t param_len,
                                      uint16_t value,
                                      uint16_t timeout_ms)
{
    uint8_t frame[WT2003_MAX_FRAME_SIZE];
    size_t frame_len = 0;

    if (!wt2003_build_frame(wt_command, params, param_len, frame, sizeof(frame), &frame_len)) {
        return reject_command(logical_command, wt_command, value, AUDIO_STATUS_BAD_ARGUMENT);
    }
    return enqueue_prebuilt(logical_command, wt_command, value, frame, frame_len, timeout_ms);
}

static int send_active_command(uint32_t now_ms)
{
    if (audio_wt2003_platform_send(active_command.frame, active_command.len) < 0) {
        active_valid = 0;
        report.pending = 0;
        report.hw_state = AUDIO_HW_ERROR;
        report.last_error = AUDIO_STATUS_TIMEOUT;
        report.timeout_errors++;
        fault_raise(FAULT_AUDIO_TIMEOUT);
        return -1;
    }

    store_last_tx(active_command.frame, active_command.len);
    report.uart_ready = 1;
    report.hw_state = AUDIO_HW_UART_READY;
    report.pending = 1;
    report.last_command = active_command.logical_command;
    report.last_wt_command = active_command.wt_command;
    report.last_value = active_command.value;
    report.last_error = AUDIO_STATUS_UART_READY;
    report.deadline_ms = now_ms + active_command.timeout_ms;
    report.tx_count++;
    return 0;
}

static void maybe_send_next(uint32_t now_ms)
{
    if (active_valid || queue_head == queue_tail) {
        return;
    }
    if ((int32_t)(now_ms - report.next_tx_allowed_ms) < 0) {
        return;
    }

    active_command = queue[queue_head];
    queue_head = queue_next(queue_head);
    report.queue_depth = queue_count();
    active_valid = 1;
    if (send_active_command(now_ms) < 0) {
        clear_queue();
    }
}

static void update_known_response_fields(const wt2003_frame_t *frame)
{
    switch (frame->command) {
    case WT2003_CMD_QUERY_VERSION:
        memset(report.version, 0, sizeof(report.version));
        if (frame->param_len > 0u) {
            size_t len = frame->param_len;
            if (len >= sizeof(report.version)) {
                len = sizeof(report.version) - 1u;
            }
            memcpy(report.version, frame->params, len);
        }
        break;
    case WT2003_CMD_QUERY_VOLUME:
        if (frame->param_len >= 1u) {
            report.volume = frame->params[0];
        }
        break;
    case WT2003_CMD_QUERY_STATUS:
        if (frame->param_len >= 1u) {
            report.playback_status = frame->params[0];
        }
        break;
    case WT2003_CMD_QUERY_EXT_FLASH_COUNT:
        if (frame->param_len >= 2u) {
            report.external_flash_count =
                (uint16_t)(((uint16_t)frame->params[0] << 8) | frame->params[1]);
        }
        break;
    case WT2003_CMD_QUERY_PERIPHERAL_STATUS:
        if (frame->param_len >= 1u) {
            report.peripheral_status = frame->params[0];
        }
        break;
    case WT2003_CMD_VOLUME:
        if (frame->param_len >= 1u && frame->params[0] == 0u) {
            report.volume = (uint8_t)active_command.value;
        }
        break;
    default:
        break;
    }
}

static void handle_frame(const wt2003_frame_t *frame, uint32_t now_ms)
{
    store_last_rx(frame);
    report.rx_count++;
    report.last_result_code = frame->param_len > 0u ? frame->params[0] : WT2003_RESULT_UNKNOWN;
    update_known_response_fields(frame);

    if (!active_valid) {
        report.unsolicited_count++;
        report.last_wt_command = frame->command;
        report.last_error = AUDIO_STATUS_OK;
        return;
    }

    active_valid = 0;
    report.pending = 0;
    report.deadline_ms = 0;
    report.next_tx_allowed_ms = now_ms + APP_AUDIO_COMMAND_SPACING_MS;
    report.last_error = AUDIO_STATUS_OK;
}

static void handle_parse_result(wt2003_parse_result_t result,
                                const wt2003_frame_t *frame,
                                uint32_t now_ms)
{
    switch (result) {
    case WT2003_PARSE_FRAME:
        handle_frame(frame, now_ms);
        break;
    case WT2003_PARSE_CHECKSUM_ERROR:
        report.checksum_errors++;
        report.last_error = AUDIO_STATUS_PROTOCOL_ERROR;
        break;
    case WT2003_PARSE_FRAMING_ERROR:
    case WT2003_PARSE_LENGTH_ERROR:
        report.framing_errors++;
        report.last_error = AUDIO_STATUS_PROTOCOL_ERROR;
        break;
    case WT2003_PARSE_TIMEOUT:
        report.timeout_errors++;
        report.last_error = AUDIO_STATUS_TIMEOUT;
        break;
    case WT2003_PARSE_NONE:
    default:
        break;
    }
}

void wt2003_init(void)
{
    memset(&report, 0, sizeof(report));
    wt2003_parser_init(&parser);
    clear_queue();

    report.hw_state = AUDIO_HW_NOT_TESTED;
    report.busy_state = wt2003_get_busy_pin();
    report.last_command = AUDIO_CMD_NONE;
    report.last_wt_command = 0;
    report.last_value = 0;
    report.last_error = AUDIO_STATUS_HW_NOT_TESTED;
    report.ready_after_ms = timebase_ms() + APP_AUDIO_POWER_ON_DELAY_MS;
    report.next_tx_allowed_ms = report.ready_after_ms;
    report.last_result_code = WT2003_RESULT_UNKNOWN;
    report.volume = APP_AUDIO_DEFAULT_VOLUME;
    report.playback_status = WT2003_RESULT_UNKNOWN;
    report.peripheral_status = 0;
    report.external_flash_count = 0xffffu;

    if (hardware_blocked()) {
        report.hw_state = AUDIO_HW_BLOCKED;
        report.last_error = AUDIO_STATUS_HW_BLOCKED;
        fault_raise(FAULT_AUDIO_HW_BLOCKED);
    }
}

void wt2003_tick(void)
{
    uint32_t now_ms = timebase_ms();
    wt2003_frame_t frame;
    uint8_t byte;
    int read_rc;

    report.busy_state = wt2003_get_busy_pin();

    while ((read_rc = audio_wt2003_platform_read_byte(&byte)) > 0) {
        wt2003_parse_result_t result = wt2003_parse_byte(&parser, byte, now_ms, &frame);
        handle_parse_result(result, &frame, now_ms);
    }
    (void)read_rc;

    handle_parse_result(wt2003_parser_tick(&parser, now_ms, APP_AUDIO_COMMAND_TIMEOUT_MS),
                        &frame,
                        now_ms);

    if (active_valid && report.deadline_ms != 0u &&
        (int32_t)(now_ms - report.deadline_ms) >= 0) {
        report.timeout_errors++;
        if (active_command.retries_left > 0u) {
            active_command.retries_left--;
            (void)send_active_command(now_ms);
        } else {
            active_valid = 0;
            report.pending = 0;
            report.last_error = AUDIO_STATUS_TIMEOUT;
            report.hw_state = AUDIO_HW_ERROR;
            report.deadline_ms = 0;
            report.next_tx_allowed_ms = now_ms + APP_AUDIO_COMMAND_SPACING_MS;
            fault_raise(FAULT_AUDIO_TIMEOUT);
        }
    }

    maybe_send_next(now_ms);
}

uint8_t wt2003_get_busy_pin(void)
{
    return board_input_read(BOARD_INPUT_AUDIO_BUSY);
}

audio_status_t wt2003_query_version(void)
{
    return enqueue_command(AUDIO_CMD_QUERY_VERSION, WT2003_CMD_QUERY_VERSION, NULL, 0, 0, 0);
}

audio_status_t wt2003_query_volume(void)
{
    return enqueue_command(AUDIO_CMD_QUERY_VOLUME, WT2003_CMD_QUERY_VOLUME, NULL, 0, 0, 0);
}

audio_status_t wt2003_query_status(void)
{
    return enqueue_command(AUDIO_CMD_QUERY_STATUS, WT2003_CMD_QUERY_STATUS, NULL, 0, 0, 0);
}

audio_status_t wt2003_query_external_flash_count(void)
{
    return enqueue_command(AUDIO_CMD_QUERY_EXT_FLASH_COUNT, WT2003_CMD_QUERY_EXT_FLASH_COUNT, NULL, 0, 0, 0);
}

audio_status_t wt2003_query_peripheral_status(void)
{
    return enqueue_command(AUDIO_CMD_QUERY_PERIPHERAL_STATUS, WT2003_CMD_QUERY_PERIPHERAL_STATUS, NULL, 0, 0, 0);
}

audio_status_t wt2003_play_external_index(uint16_t index)
{
    uint8_t params[2];

    if (index == 0u) {
        return reject_command(AUDIO_CMD_PLAY, WT2003_CMD_PLAY_EXT_ROOT_INDEX, index, AUDIO_STATUS_BAD_ARGUMENT);
    }
    params[0] = (uint8_t)(index >> 8);
    params[1] = (uint8_t)index;
    return enqueue_command(AUDIO_CMD_PLAY, WT2003_CMD_PLAY_EXT_ROOT_INDEX, params, sizeof(params), index, 0);
}

audio_status_t wt2003_play_external_name(const char *name_no_ext)
{
    size_t len;

    if (!name_no_ext) {
        return reject_command(AUDIO_CMD_PLAY_NAME, WT2003_CMD_PLAY_EXT_ROOT_NAME, 0, AUDIO_STATUS_BAD_ARGUMENT);
    }
    len = strlen(name_no_ext);
    if (len == 0u || len > 8u) {
        return reject_command(AUDIO_CMD_PLAY_NAME, WT2003_CMD_PLAY_EXT_ROOT_NAME, 0, AUDIO_STATUS_BAD_ARGUMENT);
    }
    return enqueue_command(AUDIO_CMD_PLAY_NAME,
                           WT2003_CMD_PLAY_EXT_ROOT_NAME,
                           (const uint8_t *)name_no_ext,
                           len,
                           0,
                           0);
}

audio_status_t wt2003_stop(void)
{
    return enqueue_command(AUDIO_CMD_STOP, WT2003_CMD_STOP, NULL, 0, 0, 0);
}

audio_status_t wt2003_pause_resume(void)
{
    return enqueue_command(AUDIO_CMD_PAUSE_RESUME, WT2003_CMD_PAUSE, NULL, 0, 0, 0);
}

audio_status_t wt2003_next(void)
{
    return enqueue_command(AUDIO_CMD_NEXT, WT2003_CMD_NEXT, NULL, 0, 0, 0);
}

audio_status_t wt2003_prev(void)
{
    return enqueue_command(AUDIO_CMD_PREV, WT2003_CMD_PREV, NULL, 0, 0, 0);
}

audio_status_t wt2003_set_volume(uint8_t volume_0_31)
{
    if (volume_0_31 > 31u) {
        return reject_command(AUDIO_CMD_SET_VOLUME, WT2003_CMD_VOLUME, volume_0_31, AUDIO_STATUS_BAD_ARGUMENT);
    }
    return enqueue_command(AUDIO_CMD_SET_VOLUME, WT2003_CMD_VOLUME, &volume_0_31, 1, volume_0_31, 0);
}

audio_status_t wt2003_set_playback_mode(wt2003_playback_mode_t mode)
{
    uint8_t param = (uint8_t)mode;

    if (param > WT2003_PLAYBACK_RANDOM) {
        return reject_command(AUDIO_CMD_SET_PLAYBACK_MODE, WT2003_CMD_PLAYBACK_MODE, param, AUDIO_STATUS_BAD_ARGUMENT);
    }
    return enqueue_command(AUDIO_CMD_SET_PLAYBACK_MODE, WT2003_CMD_PLAYBACK_MODE, &param, 1, param, 0);
}

static audio_status_t set_output_mode(wt2003_output_mode_t mode)
{
    uint8_t param = (uint8_t)mode;

    if (param > WT2003_OUTPUT_DAC) {
        return reject_command(AUDIO_CMD_SET_OUTPUT_MODE, WT2003_CMD_OUTPUT_MODE, param, AUDIO_STATUS_BAD_ARGUMENT);
    }
    return enqueue_command(AUDIO_CMD_SET_OUTPUT_MODE, WT2003_CMD_OUTPUT_MODE, &param, 1, param, 0);
}

audio_status_t wt2003_set_output_spk(void)
{
    return set_output_mode(WT2003_OUTPUT_SPK);
}

audio_status_t wt2003_set_output_dac(void)
{
    return set_output_mode(WT2003_OUTPUT_DAC);
}

static audio_status_t enter_sleep(wt2003_sleep_mode_t mode)
{
    uint8_t param = (uint8_t)mode;

    if (param > WT2003_SLEEP_IDLE) {
        return reject_command(AUDIO_CMD_SLEEP, WT2003_CMD_LOW_POWER, param, AUDIO_STATUS_BAD_ARGUMENT);
    }
    return enqueue_command(AUDIO_CMD_SLEEP, WT2003_CMD_LOW_POWER, &param, 1, param, 0);
}

audio_status_t wt2003_enter_idle_sleep(void)
{
    return enter_sleep(WT2003_SLEEP_IDLE);
}

audio_status_t wt2003_enter_deep_sleep(void)
{
    return enter_sleep(WT2003_SLEEP_DEEP);
}

audio_status_t wt2003_format_external_flash_for_bringup(void)
{
#if APP_AUDIO_ALLOW_FORMAT_COMMAND
    return enqueue_command(AUDIO_CMD_FORMAT_EXT_FLASH,
                           WT2003_CMD_FORMAT_EXT_FLASH,
                           NULL,
                           0,
                           0,
                           APP_AUDIO_FORMAT_TIMEOUT_MS);
#else
    return reject_command(AUDIO_CMD_FORMAT_EXT_FLASH,
                          WT2003_CMD_FORMAT_EXT_FLASH,
                          0,
                          AUDIO_STATUS_DISABLED);
#endif
}

audio_status_t wt2003_send_raw(const uint8_t *data, size_t len)
{
    uint8_t wt_command;

    if (!data) {
        return reject_command(AUDIO_CMD_RAW, 0, 0, AUDIO_STATUS_BAD_ARGUMENT);
    }
    wt_command = len > 2u ? data[2] : 0u;
    return enqueue_prebuilt(AUDIO_CMD_RAW, wt_command, 0, data, len, APP_AUDIO_COMMAND_TIMEOUT_MS);
}

audio_wt2003_report_t wt2003_report(void)
{
    wt2003_tick();
    return report;
}

const char *wt2003_result_code_to_string(uint8_t cmd, uint8_t code)
{
    if (code == WT2003_RESULT_UNKNOWN) {
        return "unknown";
    }

    switch (cmd) {
    case WT2003_CMD_QUERY_STATUS:
        switch (code) {
        case 0x01: return "playing";
        case 0x02: return "stopped";
        case 0x03: return "paused";
        default: return "unknown_status";
        }
    case WT2003_CMD_QUERY_VOLUME:
        return "volume_value";
    case WT2003_CMD_QUERY_EXT_FLASH_COUNT:
        return "count_high_byte";
    case WT2003_CMD_QUERY_PERIPHERAL_STATUS:
        return "peripheral_status_bits";
    default:
        break;
    }

    switch (code) {
    case 0x00: return "ok";
    case 0x01: return "ambiguous_error_or_flash_error";
    case 0x02: return "file_not_found";
    case 0x05: return "device_offline";
    default: return "unknown_result";
    }
}

void audio_wt2003_init(void)
{
    wt2003_init();
}

void audio_wt2003_poll(void)
{
    wt2003_tick();
}

audio_status_t audio_wt2003_ping(void)
{
    return wt2003_query_status();
}

audio_status_t audio_wt2003_set_volume(uint8_t volume)
{
    return wt2003_set_volume(volume);
}

audio_status_t audio_wt2003_play(uint16_t file_id)
{
    return wt2003_play_external_index(file_id);
}

audio_status_t audio_wt2003_stop(void)
{
    return wt2003_stop();
}

uint8_t audio_wt2003_busy(void)
{
    return wt2003_get_busy_pin();
}

audio_wt2003_report_t audio_wt2003_report(void)
{
    return wt2003_report();
}

const char *audio_wt2003_status_name(audio_status_t status)
{
    switch (status) {
    case AUDIO_STATUS_OK: return "ok";
    case AUDIO_STATUS_HW_BLOCKED: return "hw_blocked";
    case AUDIO_STATUS_HW_NOT_TESTED: return "hw_not_tested";
    case AUDIO_STATUS_UART_READY: return "uart_ready";
    case AUDIO_STATUS_TIMEOUT: return "timeout";
    case AUDIO_STATUS_PROTOCOL_ERROR: return "protocol_error";
    case AUDIO_STATUS_BUSY: return "busy";
    case AUDIO_STATUS_QUEUE_FULL: return "queue_full";
    case AUDIO_STATUS_BAD_ARGUMENT: return "bad_argument";
    case AUDIO_STATUS_NOT_READY: return "not_ready";
    case AUDIO_STATUS_DISABLED: return "disabled";
    default: return "unknown";
    }
}

const char *audio_wt2003_hw_state_name(audio_hw_state_t state)
{
    switch (state) {
    case AUDIO_HW_NOT_TESTED: return "HW_NOT_TESTED";
    case AUDIO_HW_BLOCKED: return "HW_BLOCKED";
    case AUDIO_HW_UART_READY: return "UART_READY";
    case AUDIO_HW_ERROR: return "HW_ERROR";
    default: return "UNKNOWN";
    }
}

const char *audio_wt2003_command_name(audio_command_t command)
{
    switch (command) {
    case AUDIO_CMD_NONE: return "none";
    case AUDIO_CMD_PING: return "ping";
    case AUDIO_CMD_QUERY_VERSION: return "query_version";
    case AUDIO_CMD_QUERY_VOLUME: return "query_volume";
    case AUDIO_CMD_QUERY_STATUS: return "query_status";
    case AUDIO_CMD_QUERY_EXT_FLASH_COUNT: return "query_ext_flash_count";
    case AUDIO_CMD_QUERY_PERIPHERAL_STATUS: return "query_peripheral_status";
    case AUDIO_CMD_SET_VOLUME: return "set_volume";
    case AUDIO_CMD_PLAY: return "play_index";
    case AUDIO_CMD_PLAY_NAME: return "play_name";
    case AUDIO_CMD_STOP: return "stop";
    case AUDIO_CMD_PAUSE_RESUME: return "pause_resume";
    case AUDIO_CMD_NEXT: return "next";
    case AUDIO_CMD_PREV: return "prev";
    case AUDIO_CMD_SET_PLAYBACK_MODE: return "set_playback_mode";
    case AUDIO_CMD_SET_OUTPUT_MODE: return "set_output_mode";
    case AUDIO_CMD_SLEEP: return "sleep";
    case AUDIO_CMD_FORMAT_EXT_FLASH: return "format_ext_flash";
    case AUDIO_CMD_RAW: return "raw";
    default: return "unknown";
    }
}
