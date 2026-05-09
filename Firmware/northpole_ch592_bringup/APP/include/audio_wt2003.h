#ifndef AUDIO_WT2003_H
#define AUDIO_WT2003_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board_pins_autogen_notes.h"

#define AUDIO_WT2003_HW_BLOCKED BOARD_AUTOGEN_AUDIO_HW_BLOCKED

#define WT2003_FRAME_START 0x7Eu
#define WT2003_FRAME_END 0xEFu
#define WT2003_MAX_PARAMS 32u
#define WT2003_MAX_FRAME_SIZE (1u + 1u + 1u + WT2003_MAX_PARAMS + 1u + 1u)
#define WT2003_RESULT_UNKNOWN 0xffu

typedef enum {
    AUDIO_STATUS_OK = 0,
    AUDIO_STATUS_HW_BLOCKED,
    AUDIO_STATUS_HW_NOT_TESTED,
    AUDIO_STATUS_UART_READY,
    AUDIO_STATUS_TIMEOUT,
    AUDIO_STATUS_PROTOCOL_ERROR,
    AUDIO_STATUS_BUSY,
    AUDIO_STATUS_QUEUE_FULL,
    AUDIO_STATUS_BAD_ARGUMENT,
    AUDIO_STATUS_NOT_READY,
    AUDIO_STATUS_DISABLED,
} audio_status_t;

typedef enum {
    AUDIO_HW_NOT_TESTED = 0,
    AUDIO_HW_BLOCKED,
    AUDIO_HW_UART_READY,
    AUDIO_HW_ERROR,
} audio_hw_state_t;

typedef enum {
    AUDIO_CMD_NONE = 0,
    AUDIO_CMD_PING,
    AUDIO_CMD_QUERY_VERSION,
    AUDIO_CMD_QUERY_VOLUME,
    AUDIO_CMD_QUERY_STATUS,
    AUDIO_CMD_QUERY_EXT_FLASH_COUNT,
    AUDIO_CMD_QUERY_PERIPHERAL_STATUS,
    AUDIO_CMD_SET_VOLUME,
    AUDIO_CMD_PLAY,
    AUDIO_CMD_PLAY_NAME,
    AUDIO_CMD_STOP,
    AUDIO_CMD_PAUSE_RESUME,
    AUDIO_CMD_NEXT,
    AUDIO_CMD_PREV,
    AUDIO_CMD_SET_PLAYBACK_MODE,
    AUDIO_CMD_SET_OUTPUT_MODE,
    AUDIO_CMD_SLEEP,
    AUDIO_CMD_FORMAT_EXT_FLASH,
    AUDIO_CMD_RAW,
} audio_command_t;

typedef enum {
    WT2003_PARSE_WAIT_START = 0,
    WT2003_PARSE_LENGTH,
    WT2003_PARSE_PAYLOAD,
    WT2003_PARSE_END,
} wt2003_parse_state_t;

typedef enum {
    WT2003_PARSE_NONE = 0,
    WT2003_PARSE_FRAME,
    WT2003_PARSE_CHECKSUM_ERROR,
    WT2003_PARSE_FRAMING_ERROR,
    WT2003_PARSE_LENGTH_ERROR,
    WT2003_PARSE_TIMEOUT,
} wt2003_parse_result_t;

typedef struct {
    wt2003_parse_state_t state;
    uint8_t length;
    uint8_t payload_pos;
    uint8_t payload[WT2003_MAX_PARAMS + 2u];
    uint32_t started_ms;
} wt2003_parser_t;

typedef struct {
    uint8_t command;
    uint8_t params[WT2003_MAX_PARAMS];
    size_t param_len;
    uint8_t checksum;
} wt2003_frame_t;

typedef enum {
    WT2003_CMD_PLAY_ONCHIP_PACKAGE_INDEX = 0x95,
    WT2003_CMD_PLAY_ONCHIP_PACKAGE_NAME  = 0x92,
    WT2003_CMD_PLAY_EXT_PACKAGE_INDEX    = 0x94,
    WT2003_CMD_PLAY_EXT_PACKAGE_NAME     = 0x91,
    WT2003_CMD_FORMAT_EXT_FLASH          = 0xDF,
    WT2003_CMD_LOW_POWER                 = 0xB8,
    WT2003_CMD_DRIVE_LETTER              = 0x9C,
    WT2003_CMD_PLAY_ONCHIP_ROOT_INDEX    = 0x9D,
    WT2003_CMD_PLAY_ONCHIP_ROOT_NAME     = 0x9E,
    WT2003_CMD_PLAY_FIXED_INDEX          = 0x9F,
    WT2003_CMD_PLAY_EXT_ROOT_INDEX       = 0xA0,
    WT2003_CMD_PLAY_EXT_ROOT_NAME        = 0xA1,
    WT2003_CMD_PAUSE                     = 0xAA,
    WT2003_CMD_STOP                      = 0xAB,
    WT2003_CMD_NEXT                      = 0xAC,
    WT2003_CMD_PREV                      = 0xAD,
    WT2003_CMD_VOLUME                    = 0xAE,
    WT2003_CMD_PLAYBACK_MODE             = 0xAF,
    WT2003_CMD_INSERT_PLAY               = 0xB1,
    WT2003_CMD_OUTPUT_MODE               = 0xB6,
    WT2003_CMD_QUERY_VERSION             = 0xC0,
    WT2003_CMD_QUERY_VOLUME              = 0xC1,
    WT2003_CMD_QUERY_STATUS              = 0xC2,
    WT2003_CMD_QUERY_EXT_FLASH_COUNT     = 0xC3,
    WT2003_CMD_QUERY_PERIPHERAL_STATUS   = 0xCA,
    WT2003_CMD_QUERY_CURRENT_NAME        = 0xCB,
    WT2003_CMD_SWITCH_BAUD               = 0xFB,
} wt2003_command_t;

typedef enum {
    WT2003_PLAYBACK_SINGLE = 0,
    WT2003_PLAYBACK_SINGLE_LOOP = 1,
    WT2003_PLAYBACK_ALL_LOOP = 2,
    WT2003_PLAYBACK_RANDOM = 3,
} wt2003_playback_mode_t;

typedef enum {
    WT2003_OUTPUT_SPK = 0,
    WT2003_OUTPUT_DAC = 1,
} wt2003_output_mode_t;

typedef enum {
    WT2003_SLEEP_DEEP = 0,
    WT2003_SLEEP_IDLE = 1,
} wt2003_sleep_mode_t;

typedef struct {
    audio_hw_state_t hw_state;
    uint8_t busy_state;
    uint8_t uart_ready;
    uint8_t pending;
    uint8_t queue_depth;
    audio_command_t last_command;
    uint8_t last_wt_command;
    uint16_t last_value;
    audio_status_t last_error;
    uint32_t deadline_ms;
    uint32_t ready_after_ms;
    uint32_t next_tx_allowed_ms;
    uint8_t last_result_code;
    uint8_t last_tx[WT2003_MAX_FRAME_SIZE];
    uint8_t last_tx_len;
    uint8_t last_rx[WT2003_MAX_FRAME_SIZE];
    uint8_t last_rx_len;
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t checksum_errors;
    uint32_t framing_errors;
    uint32_t timeout_errors;
    uint32_t unsolicited_count;
    char version[24];
    uint8_t volume;
    uint8_t playback_status;
    uint8_t peripheral_status;
    uint16_t external_flash_count;
} audio_wt2003_report_t;

bool wt2003_build_frame(uint8_t cmd,
                        const uint8_t *params,
                        size_t param_len,
                        uint8_t *out,
                        size_t out_cap,
                        size_t *out_len);
void wt2003_parser_init(wt2003_parser_t *parser);
wt2003_parse_result_t wt2003_parse_byte(wt2003_parser_t *parser,
                                        uint8_t byte,
                                        uint32_t now_ms,
                                        wt2003_frame_t *out);
wt2003_parse_result_t wt2003_parser_tick(wt2003_parser_t *parser,
                                         uint32_t now_ms,
                                         uint32_t timeout_ms);

void wt2003_init(void);
void wt2003_tick(void);
bool wt2003_is_ready(void);
uint8_t wt2003_get_busy_pin(void);
audio_status_t wt2003_query_version(void);
audio_status_t wt2003_query_volume(void);
audio_status_t wt2003_query_status(void);
audio_status_t wt2003_query_external_flash_count(void);
audio_status_t wt2003_query_peripheral_status(void);
audio_status_t wt2003_play_external_index(uint16_t index);
audio_status_t wt2003_play_external_name(const char *name_no_ext);
audio_status_t wt2003_stop(void);
audio_status_t wt2003_pause_resume(void);
audio_status_t wt2003_next(void);
audio_status_t wt2003_prev(void);
audio_status_t wt2003_set_volume(uint8_t volume_0_31);
audio_status_t wt2003_set_playback_mode(wt2003_playback_mode_t mode);
audio_status_t wt2003_set_output_spk(void);
audio_status_t wt2003_set_output_dac(void);
audio_status_t wt2003_enter_idle_sleep(void);
audio_status_t wt2003_enter_deep_sleep(void);
audio_status_t wt2003_format_external_flash_for_bringup(void);
audio_status_t wt2003_send_raw(const uint8_t *data, size_t len);
audio_wt2003_report_t wt2003_report(void);
const char *wt2003_result_code_to_string(uint8_t cmd, uint8_t code);

int audio_wt2003_platform_send(const uint8_t *data, uint8_t len);
int audio_wt2003_platform_read_byte(uint8_t *byte);

void audio_wt2003_init(void);
void audio_wt2003_poll(void);
audio_status_t audio_wt2003_ping(void);
audio_status_t audio_wt2003_set_volume(uint8_t volume);
audio_status_t audio_wt2003_play(uint16_t file_id);
audio_status_t audio_wt2003_stop(void);
uint8_t audio_wt2003_busy(void);
audio_wt2003_report_t audio_wt2003_report(void);
const char *audio_wt2003_status_name(audio_status_t status);
const char *audio_wt2003_hw_state_name(audio_hw_state_t state);
const char *audio_wt2003_command_name(audio_command_t command);

#endif /* AUDIO_WT2003_H */
