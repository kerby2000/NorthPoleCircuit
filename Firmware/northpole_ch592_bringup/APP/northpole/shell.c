#include "shell.h"

#include "CONFIG.h"
#include "app_config.h"
#include "audio_wt2003.h"
#include "board.h"
#include "build_profile.h"
#include "fault.h"
#include "hall.h"
#include "i2c_bus.h"
#include "log.h"
#include "motor_drv8837.h"
#include "power_ip5209.h"
#include "rgb_ws2812.h"
#include "settings.h"
#include "touch.h"

#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__)
#define FW_WEAK __attribute__((weak))
#else
#define FW_WEAK
#endif

#define SHELL_MAX_ARGS 16u
#define SHELL_MAX_REGISTERED 8u

typedef struct {
    const shell_command_t *commands;
    size_t count;
} shell_command_group_t;

static shell_command_group_t registered[SHELL_MAX_REGISTERED];
static size_t registered_count;

FW_WEAK int shell_platform_read_line(char *buffer, size_t buffer_size)
{
    (void)buffer;
    (void)buffer_size;
    return 0;
}

static int cmd_help(int argc, char **argv);
static int cmd_version(int argc, char **argv);
static int cmd_status(int argc, char **argv);
static int cmd_faults(int argc, char **argv);
static int cmd_pins(int argc, char **argv);
static int cmd_safe(int argc, char **argv);
static int cmd_audio(int argc, char **argv);
static int cmd_motor(int argc, char **argv);
static int cmd_rgb(int argc, char **argv);
static int cmd_hall(int argc, char **argv);
static int cmd_touch(int argc, char **argv);
static int cmd_i2c(int argc, char **argv);
static int cmd_ip5209(int argc, char **argv);
static int cmd_settings(int argc, char **argv);
static int cmd_reset(int argc, char **argv);

static const shell_command_t builtin_commands[] = {
    {"help", "list commands", cmd_help},
    {"version", "print firmware identity", cmd_version},
    {"status", "print board status", cmd_status},
    {"faults", "print sticky fault mask", cmd_faults},
    {"pins", "pins verify", cmd_pins},
    {"safe", "safe check", cmd_safe},
    {"audio", "audio status|raw <hex...>|version|qvol|qstatus|qcount-ext|qperiph|busy|volume <0-31>|play-index <id>|play-name <name>|stop|pause|next|prev|mode <single|single-loop|all-loop|random>|output <spk|dac>|sleep <idle|deep>|format-ext-flash CONFIRM", cmd_audio},
    {"motor", "motor status|arm <seconds>|off|pwm <A|B|G> <forward|reverse|coast|brake> <duty_permille> <ms>", cmd_motor},
    {"rgb", "rgb off|one <idx> <r> <g> <b>|all <r> <g> <b>|chase <brightness>|order test|show", cmd_rgb},
    {"hall", "hall read", cmd_hall},
    {"touch", "touch raw", cmd_touch},
    {"i2c", "i2c scan|read <addr7> <reg>|write <addr7> <reg> <value>", cmd_i2c},
    {"ip5209", "ip5209 status|probe|read <reg>|write <reg> <value>", cmd_ip5209},
    {"settings", "settings show|reset|save|corrupt", cmd_settings},
    {"reset", "force safe outputs and software reset MCU", cmd_reset},
};

void shell_init(void)
{
    registered_count = 0;
    LOG_INFO("diagnostic shell ready\r\n");
}

void shell_register(const shell_command_t *commands, size_t count)
{
    if (registered_count >= SHELL_MAX_REGISTERED) {
        return;
    }
    registered[registered_count].commands = commands;
    registered[registered_count].count = count;
    registered_count++;
}

void shell_poll(void)
{
    char line[96];
    if (shell_platform_read_line(line, sizeof(line)) > 0) {
        (void)shell_execute_line(line);
    }
}

static int tokenize(char *line, char **argv, size_t max_args)
{
    int argc = 0;
    char *token = strtok(line, " \t\r\n");
    while (token && argc < (int)max_args) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    return argc;
}

static const shell_command_t *find_command(const char *name)
{
    for (size_t i = 0; i < sizeof(builtin_commands) / sizeof(builtin_commands[0]); ++i) {
        if (strcmp(name, builtin_commands[i].name) == 0) {
            return &builtin_commands[i];
        }
    }
    for (size_t group = 0; group < registered_count; ++group) {
        for (size_t i = 0; i < registered[group].count; ++i) {
            if (strcmp(name, registered[group].commands[i].name) == 0) {
                return &registered[group].commands[i];
            }
        }
    }
    return NULL;
}

int shell_execute_line(char *line)
{
    char *argv[SHELL_MAX_ARGS];
    int argc = tokenize(line, argv, SHELL_MAX_ARGS);
    const shell_command_t *command;

    if (argc == 0) {
        return 0;
    }

    command = find_command(argv[0]);
    if (!command) {
        LOG_WARN("unknown command: %s\r\n", argv[0]);
        return -1;
    }
    return command->handler(argc, argv);
}

static int cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    for (size_t i = 0; i < sizeof(builtin_commands) / sizeof(builtin_commands[0]); ++i) {
        LOG_INFO("%-8s %s\r\n", builtin_commands[i].name, builtin_commands[i].help);
    }
    return 0;
}

static int cmd_version(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LOG_INFO("version=%s profile=%s board=%s git=%s built=%s\r\n",
             APP_FIRMWARE_VERSION,
             BUILD_PROFILE_NAME,
             APP_BOARD_REVISION,
             APP_GIT_COMMIT,
             APP_BUILD_DATE);
    LOG_INFO("pcb_sha=%s sch_sha=%s audio_uart_connected=%u\r\n",
             BOARD_AUTOGEN_PCB_SHA256,
             BOARD_AUTOGEN_SCH_SHA256,
             (unsigned)BOARD_AUTOGEN_AUDIO_UART_CONNECTED);
    return 0;
}

static int cmd_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LOG_INFO("faults=0x%08lx motor_armed=%u audio_hw_blocked=%u rgb_count=%u\r\n",
             (unsigned long)fault_snapshot(),
             (unsigned)motor_drv8837_is_armed(),
             (unsigned)AUDIO_WT2003_HW_BLOCKED,
             (unsigned)APP_RGB_LED_COUNT);
    return 0;
}

static int cmd_faults(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LOG_INFO("fault_mask=0x%08lx\r\n", (unsigned long)fault_snapshot());
    for (uint8_t i = 1; i < FAULT_COUNT; ++i) {
        if (fault_is_set((fault_code_t)i)) {
            LOG_INFO("fault %u %s\r\n", (unsigned)i, fault_name((fault_code_t)i));
        }
    }
    return 0;
}

static int cmd_pins(int argc, char **argv)
{
    size_t count = 0;
    const board_pin_t *const *pins;

    if (argc >= 2 && strcmp(argv[1], "verify") != 0) {
        LOG_INFO("pins verify\r\n");
        return 0;
    }

    pins = board_all_pins(&count);
    LOG_INFO("firmware pin verification\r\n");
    LOG_INFO("name,pad,function,net,direction,active,safe\r\n");
    for (size_t i = 0; i < count; ++i) {
        LOG_INFO("%s,%u,%s,%s,%s,%s,%s\r\n",
                 pins[i]->role,
                 (unsigned)pins[i]->u2_pad,
                 pins[i]->pin_function,
                 pins[i]->net,
                 board_direction_name(pins[i]->direction),
                 board_active_name(pins[i]->active),
                 board_safe_state_name(pins[i]->safe_state));
    }
    return 0;
}

static void print_safe_pin(const char *label, const board_pin_t *pin)
{
    LOG_INFO("%-18s pad=%u func=%s net=%s dir=%s active=%s expected_safe=%s\r\n",
             label,
             (unsigned)pin->u2_pad,
             pin->pin_function,
             pin->net,
             board_direction_name(pin->direction),
             board_active_name(pin->active),
             board_safe_state_name(pin->safe_state));
}

static int cmd_safe(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (argc >= 2 && strcmp(argv[1], "check") != 0) {
        LOG_INFO("safe check\r\n");
        return 0;
    }

    LOG_INFO("safe-state expectations; no pins toggled\r\n");
    print_safe_pin("DRV8837 A IN1", board_output_pin(BOARD_OUTPUT_PWM_A1));
    print_safe_pin("DRV8837 A IN2", board_output_pin(BOARD_OUTPUT_PWM_A2));
    print_safe_pin("DRV8837 B IN1", board_output_pin(BOARD_OUTPUT_PWM_B1));
    print_safe_pin("DRV8837 B IN2", board_output_pin(BOARD_OUTPUT_PWM_B2));
    print_safe_pin("DRV8837 G IN1", board_output_pin(BOARD_OUTPUT_PWM_G1));
    print_safe_pin("DRV8837 G IN2", board_output_pin(BOARD_OUTPUT_PWM_G2));
    print_safe_pin("MOTOR_SLEEP", board_output_pin(BOARD_OUTPUT_MOTOR_SLEEP));
    print_safe_pin("RGB LED data", board_output_pin(BOARD_OUTPUT_RGB_DATA));
    LOG_INFO("%-18s idle high when UART enabled; safe input before audio backend opens UART\r\n", "WT2003 UART TX");
    print_safe_pin("WT2003 BUSY", board_input_pin(BOARD_INPUT_AUDIO_BUSY));
    print_safe_pin("Hall 1", board_input_pin(BOARD_INPUT_HALL1));
    print_safe_pin("Hall 2", board_input_pin(BOARD_INPUT_HALL2));
    print_safe_pin("Touch SPD-", board_input_pin(BOARD_INPUT_TOUCH_SPD_MINUS));
    print_safe_pin("Touch RUN", board_input_pin(BOARD_INPUT_TOUCH_RUN));
    print_safe_pin("Touch SPD+", board_input_pin(BOARD_INPUT_TOUCH_SPD_PLUS));
    print_safe_pin("Touch MUSIC", board_input_pin(BOARD_INPUT_TOUCH_MUSIC));
    return 0;
}

static void print_hex_bytes(const uint8_t *data, uint8_t len)
{
    for (uint8_t i = 0; i < len; ++i) {
        LOG_INFO(" %02x", (unsigned)data[i]);
    }
    LOG_INFO("\r\n");
}

static int parse_u8_arg(const char *arg, uint8_t *value)
{
    char *end = NULL;
    unsigned long parsed = strtoul(arg, &end, 0);

    if (!arg || end == arg || *end != '\0' || parsed > 0xffu) {
        return -1;
    }
    *value = (uint8_t)parsed;
    return 0;
}

static int parse_hex_byte_arg(const char *arg, uint8_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (!arg) {
        return -1;
    }
    if (arg[0] == '0' && (arg[1] == 'x' || arg[1] == 'X')) {
        arg += 2;
    }
    parsed = strtoul(arg, &end, 16);
    if (end == arg || *end != '\0' || parsed > 0xffu) {
        return -1;
    }
    *value = (uint8_t)parsed;
    return 0;
}

static int parse_playback_mode_arg(const char *arg, wt2003_playback_mode_t *mode)
{
    if (strcmp(arg, "single") == 0) {
        *mode = WT2003_PLAYBACK_SINGLE;
        return 0;
    }
    if (strcmp(arg, "single-loop") == 0) {
        *mode = WT2003_PLAYBACK_SINGLE_LOOP;
        return 0;
    }
    if (strcmp(arg, "all-loop") == 0) {
        *mode = WT2003_PLAYBACK_ALL_LOOP;
        return 0;
    }
    if (strcmp(arg, "random") == 0) {
        *mode = WT2003_PLAYBACK_RANDOM;
        return 0;
    }
    return -1;
}

static int cmd_audio(int argc, char **argv)
{
    audio_status_t status;
    if (argc < 2) {
        LOG_INFO("audio status|raw <hex...>|version|qvol|qstatus|qcount-ext|qperiph|busy|volume <0-31>|play-index <id>|play-name <name>|stop|pause|next|prev|mode <single|single-loop|all-loop|random>|output <spk|dac>|sleep <idle|deep>|format-ext-flash CONFIRM\r\n");
        return 0;
    }
    if (strcmp(argv[1], "status") == 0) {
        audio_wt2003_report_t report = audio_wt2003_report();
        LOG_INFO("audio validation=HARDWARE_VALIDATION_PENDING hw=%s uart_ready=%u busy=%u pending=%u queue=%u ready_after_ms=%lu next_tx_ms=%lu\r\n",
                 audio_wt2003_hw_state_name(report.hw_state),
                 (unsigned)report.uart_ready,
                 (unsigned)report.busy_state,
                 (unsigned)report.pending,
                 (unsigned)report.queue_depth,
                 (unsigned long)report.ready_after_ms,
                 (unsigned long)report.next_tx_allowed_ms);
        LOG_INFO("audio last_command=%s wt_cmd=0x%02x value=%u last_error=%s deadline_ms=%lu result=0x%02x %s\r\n",
                 audio_wt2003_command_name(report.last_command),
                 (unsigned)report.last_wt_command,
                 (unsigned)report.last_value,
                 audio_wt2003_status_name(report.last_error),
                 (unsigned long)report.deadline_ms,
                 (unsigned)report.last_result_code,
                 wt2003_result_code_to_string(report.last_wt_command, report.last_result_code));
        LOG_INFO("audio counters tx=%lu rx=%lu checksum=%lu framing=%lu timeout=%lu unsolicited=%lu\r\n",
                 (unsigned long)report.tx_count,
                 (unsigned long)report.rx_count,
                 (unsigned long)report.checksum_errors,
                 (unsigned long)report.framing_errors,
                 (unsigned long)report.timeout_errors,
                 (unsigned long)report.unsolicited_count);
        LOG_INFO("audio version=%s volume=%u playback_status=0x%02x peripheral=0x%02x ext_count=%u\r\n",
                 report.version[0] ? report.version : "unknown",
                 (unsigned)report.volume,
                 (unsigned)report.playback_status,
                 (unsigned)report.peripheral_status,
                 (unsigned)report.external_flash_count);
        LOG_INFO("audio last_tx:");
        print_hex_bytes(report.last_tx, report.last_tx_len);
        LOG_INFO("audio last_rx:");
        print_hex_bytes(report.last_rx, report.last_rx_len);
        return 0;
    } else if (strcmp(argv[1], "raw") == 0 && argc >= 3) {
        uint8_t raw[WT2003_MAX_FRAME_SIZE];
        size_t raw_len = 0;

        for (int i = 2; i < argc; ++i) {
            if (raw_len >= sizeof(raw) || parse_hex_byte_arg(argv[i], &raw[raw_len]) < 0) {
                LOG_WARN("bad audio raw byte\r\n");
                return -1;
            }
            raw_len++;
        }
        status = wt2003_send_raw(raw, raw_len);
    } else if (strcmp(argv[1], "version") == 0) {
        status = wt2003_query_version();
    } else if (strcmp(argv[1], "qvol") == 0) {
        status = wt2003_query_volume();
    } else if (strcmp(argv[1], "qstatus") == 0 || strcmp(argv[1], "ping") == 0) {
        status = wt2003_query_status();
    } else if (strcmp(argv[1], "qcount-ext") == 0) {
        status = wt2003_query_external_flash_count();
    } else if (strcmp(argv[1], "qperiph") == 0) {
        status = wt2003_query_peripheral_status();
    } else if (strcmp(argv[1], "busy") == 0) {
        LOG_INFO("audio busy=%u\r\n", (unsigned)wt2003_get_busy_pin());
        return 0;
    } else if (strcmp(argv[1], "volume") == 0 && argc >= 3) {
        uint8_t volume;

        if (parse_u8_arg(argv[2], &volume) < 0 || volume > 31u) {
            LOG_WARN("volume must be 0-31\r\n");
            return -1;
        }
        status = wt2003_set_volume(volume);
    } else if ((strcmp(argv[1], "play-index") == 0 || strcmp(argv[1], "play") == 0) && argc >= 3) {
        status = wt2003_play_external_index((uint16_t)strtoul(argv[2], NULL, 0));
    } else if (strcmp(argv[1], "play-name") == 0 && argc >= 3) {
        status = wt2003_play_external_name(argv[2]);
    } else if (strcmp(argv[1], "stop") == 0) {
        status = wt2003_stop();
    } else if (strcmp(argv[1], "pause") == 0) {
        status = wt2003_pause_resume();
    } else if (strcmp(argv[1], "next") == 0) {
        status = wt2003_next();
    } else if (strcmp(argv[1], "prev") == 0) {
        status = wt2003_prev();
    } else if (strcmp(argv[1], "mode") == 0 && argc >= 3) {
        wt2003_playback_mode_t mode;

        if (parse_playback_mode_arg(argv[2], &mode) < 0) {
            LOG_WARN("mode must be single, single-loop, all-loop, or random\r\n");
            return -1;
        }
        status = wt2003_set_playback_mode(mode);
    } else if (strcmp(argv[1], "output") == 0 && argc >= 3) {
        if (strcmp(argv[2], "spk") == 0) {
            status = wt2003_set_output_spk();
        } else if (strcmp(argv[2], "dac") == 0) {
            status = wt2003_set_output_dac();
        } else {
            LOG_WARN("output must be spk or dac\r\n");
            return -1;
        }
    } else if (strcmp(argv[1], "sleep") == 0 && argc >= 3) {
        if (strcmp(argv[2], "idle") == 0) {
            status = wt2003_enter_idle_sleep();
        } else if (strcmp(argv[2], "deep") == 0) {
            status = wt2003_enter_deep_sleep();
        } else {
            LOG_WARN("sleep must be idle or deep\r\n");
            return -1;
        }
    } else if (strcmp(argv[1], "format-ext-flash") == 0) {
        if (argc < 3 || strcmp(argv[2], "CONFIRM") != 0) {
            LOG_WARN("audio format-ext-flash CONFIRM required; compile flag APP_AUDIO_ALLOW_FORMAT_COMMAND must also be 1\r\n");
            return -1;
        }
        status = wt2003_format_external_flash_for_bringup();
    } else {
        LOG_WARN("bad audio command\r\n");
        return -1;
    }
    LOG_INFO("audio status=%s\r\n", audio_wt2003_status_name(status));
    return status == AUDIO_STATUS_OK ? 0 : -1;
}

static int parse_driver(const char *arg, motor_driver_id_t *driver)
{
    if (strcmp(arg, "a") == 0 || strcmp(arg, "A") == 0) {
        *driver = MOTOR_DRV_A;
        return 0;
    }
    if (strcmp(arg, "b") == 0 || strcmp(arg, "B") == 0) {
        *driver = MOTOR_DRV_B;
        return 0;
    }
    if (strcmp(arg, "g") == 0 || strcmp(arg, "G") == 0) {
        *driver = MOTOR_DRV_G;
        return 0;
    }
    return -1;
}

static int parse_motor_mode(const char *arg, motor_drv8837_mode_t *mode)
{
    if (strcmp(arg, "fwd") == 0 || strcmp(arg, "forward") == 0) {
        *mode = MOTOR_DRV_FORWARD;
        return 0;
    }
    if (strcmp(arg, "rev") == 0 || strcmp(arg, "reverse") == 0) {
        *mode = MOTOR_DRV_REVERSE;
        return 0;
    }
    if (strcmp(arg, "brake") == 0) {
        *mode = MOTOR_DRV_BRAKE;
        return 0;
    }
    if (strcmp(arg, "coast") == 0 || strcmp(arg, "off") == 0) {
        *mode = MOTOR_DRV_COAST;
        return 0;
    }
    return -1;
}

static void print_motor_status(void)
{
    LOG_INFO("motor armed=%u remaining_ms=%lu sleep=%u sleep_idle_expected=0\r\n",
             (unsigned)motor_drv8837_is_armed(),
             (unsigned long)motor_drv8837_arm_remaining_ms(),
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP));
    for (uint8_t i = 0; i < MOTOR_DRV_COUNT; ++i) {
        motor_drv8837_state_t state = motor_drv8837_get_state((motor_driver_id_t)i);
        LOG_INFO("motor %s mode=%s duty=%u expires_ms=%lu\r\n",
                 motor_drv8837_driver_name((motor_driver_id_t)i),
                 motor_drv8837_mode_name(state.mode),
                 (unsigned)state.duty_permille,
                 (unsigned long)state.expires_ms);
    }
}

static int cmd_motor(int argc, char **argv)
{
    if (argc < 2) {
        LOG_INFO("motor status|arm <seconds>|off|pwm <A|B|G> <forward|reverse|coast|brake> <duty_permille> <ms>\r\n");
        return 0;
    }
    if (strcmp(argv[1], "status") == 0) {
        print_motor_status();
        return 0;
    }
    if (strcmp(argv[1], "arm") == 0 && argc >= 3) {
        unsigned long seconds = strtoul(argv[2], NULL, 0);
        uint32_t duration_ms = seconds > 4294967UL ? 0xffffffffUL : (uint32_t)(seconds * 1000UL);
        motor_drv8837_arm(duration_ms);
        LOG_INFO("motor armed=%u requested_seconds=%lu remaining_ms=%lu sleep=%u\r\n",
                 (unsigned)motor_drv8837_is_armed(),
                 seconds,
                 (unsigned long)motor_drv8837_arm_remaining_ms(),
                 (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP));
        return 0;
    }
    if (strcmp(argv[1], "off") == 0) {
        motor_drv8837_off();
        LOG_INFO("motor off sleep=%u\r\n", (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP));
        return 0;
    }
    if (strcmp(argv[1], "pwm") == 0 && argc >= 6) {
        motor_driver_id_t driver;
        motor_drv8837_mode_t mode;
        uint16_t duty = (uint16_t)strtoul(argv[4], NULL, 0);
        uint32_t duration_ms = (uint32_t)strtoul(argv[5], NULL, 0);
        int rc;

        if (parse_driver(argv[2], &driver) < 0 || parse_motor_mode(argv[3], &mode) < 0) {
            LOG_WARN("bad motor command\r\n");
            return -1;
        }

        rc = motor_drv8837_command_for(driver, mode, duty, duration_ms);
        LOG_INFO("motor %s mode=%s duty=%u requested_ms=%lu rc=%d\r\n",
                 motor_drv8837_driver_name(driver),
                 motor_drv8837_mode_name(mode),
                 (unsigned)duty,
                 (unsigned long)duration_ms,
                 rc);
        return rc;
    }
    LOG_WARN("bad motor command\r\n");
    return -1;
}

static int cmd_rgb(int argc, char **argv)
{
    static uint8_t chase_index;

    if (argc < 2) {
        LOG_INFO("rgb off|one <idx> <r> <g> <b>|all <r> <g> <b>|chase <brightness>|order test|show\r\n");
        return 0;
    }
    if (strcmp(argv[1], "off") == 0) {
        rgb_ws2812_clear();
        return 0;
    }
    if (strcmp(argv[1], "show") == 0) {
        rgb_ws2812_show();
        return 0;
    }
    if (strcmp(argv[1], "one") == 0 && argc >= 6) {
        rgb_color_t color;
        color.r = (uint8_t)strtoul(argv[3], NULL, 0);
        color.g = (uint8_t)strtoul(argv[4], NULL, 0);
        color.b = (uint8_t)strtoul(argv[5], NULL, 0);
        rgb_ws2812_set((uint8_t)strtoul(argv[2], NULL, 0), color);
        rgb_ws2812_show();
        return 0;
    }
    if (strcmp(argv[1], "all") == 0 && argc >= 5) {
        rgb_color_t color;
        color.r = (uint8_t)strtoul(argv[2], NULL, 0);
        color.g = (uint8_t)strtoul(argv[3], NULL, 0);
        color.b = (uint8_t)strtoul(argv[4], NULL, 0);
        for (uint8_t i = 0; i < APP_RGB_LED_COUNT; ++i) {
            rgb_ws2812_set(i, color);
        }
        rgb_ws2812_show();
        return 0;
    }
    if (strcmp(argv[1], "chase") == 0 && argc >= 3) {
        rgb_color_t off = {0, 0, 0};
        rgb_color_t on = {0, 255, 80};
        rgb_ws2812_set_brightness((uint8_t)strtoul(argv[2], NULL, 0));
        for (uint8_t i = 0; i < APP_RGB_LED_COUNT; ++i) {
            rgb_ws2812_set(i, off);
        }
        rgb_ws2812_set(chase_index, on);
        rgb_ws2812_show();
        chase_index = (uint8_t)((chase_index + 1u) % APP_RGB_LED_COUNT);
        LOG_INFO("rgb chase index=%u brightness=%u\r\n",
                 (unsigned)chase_index,
                 (unsigned)rgb_ws2812_get_brightness());
        return 0;
    }
    if (strcmp(argv[1], "order") == 0 && argc >= 3 && strcmp(argv[2], "test") == 0) {
        rgb_color_t red = {255, 0, 0};
        rgb_color_t green = {0, 255, 0};
        rgb_color_t blue = {0, 0, 255};
        for (uint8_t i = 0; i < APP_RGB_LED_COUNT; ++i) {
            if ((i % 3u) == 0u) {
                rgb_ws2812_set(i, red);
            } else if ((i % 3u) == 1u) {
                rgb_ws2812_set(i, green);
            } else {
                rgb_ws2812_set(i, blue);
            }
        }
        rgb_ws2812_show();
        LOG_INFO("rgb order test expected repeating red,green,blue\r\n");
        return 0;
    }
    LOG_WARN("bad rgb command\r\n");
    return -1;
}

static int cmd_hall(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    hall_poll();
    for (uint8_t i = 0; i < HALL_SENSOR_COUNT; ++i) {
        hall_state_t state = hall_get_state((hall_sensor_id_t)i);
        LOG_INFO("hall%u level=%u edges=%lu last_ms=%lu\r\n",
                 (unsigned)(i + 1u),
                 (unsigned)state.level,
                 (unsigned long)state.edge_count,
                 (unsigned long)state.last_edge_ms);
    }
    return 0;
}

static int cmd_touch(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    touch_poll();
    for (uint8_t i = 0; i < TOUCH_COUNT; ++i) {
        touch_state_t state = touch_get_state((touch_pad_id_t)i);
        LOG_INFO("touch %-6s raw=%u baseline=%u threshold=%u pressed=%u\r\n",
                 touch_name((touch_pad_id_t)i),
                 (unsigned)state.raw,
                 (unsigned)state.baseline,
                 (unsigned)state.threshold,
                 (unsigned)state.pressed);
    }
    return 0;
}

static int cmd_i2c(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "scan") == 0) {
        uint8_t addresses[I2C_BUS_MAX_SCAN_RESULTS];
        int found = i2c_bus_scan(addresses, I2C_BUS_MAX_SCAN_RESULTS, I2C_BUS_DEFAULT_TIMEOUT_MS);
        if (found < 0) {
            LOG_INFO("i2c scan rc=%d\r\n", found);
            return found;
        }
        LOG_INFO("i2c found=%d", found);
        for (uint8_t i = 0; i < (uint8_t)found && i < I2C_BUS_MAX_SCAN_RESULTS; ++i) {
            LOG_INFO(" 0x%02x", (unsigned)addresses[i]);
        }
        if (found > I2C_BUS_MAX_SCAN_RESULTS) {
            LOG_INFO(" ...");
        }
        LOG_INFO("\r\n");
        return 0;
    }
    if (strcmp(argv[1], "read") == 0 && argc >= 4) {
        uint8_t addr = (uint8_t)strtoul(argv[2], NULL, 0);
        uint8_t reg = (uint8_t)strtoul(argv[3], NULL, 0);
        uint8_t value = 0;
        int rc = i2c_bus_read_reg8(addr, reg, &value, I2C_BUS_DEFAULT_TIMEOUT_MS);
        LOG_INFO("i2c read addr=0x%02x reg=0x%02x rc=%d value=0x%02x\r\n",
                 (unsigned)addr,
                 (unsigned)reg,
                 rc,
                 (unsigned)value);
        return rc;
    }
    if (strcmp(argv[1], "write") == 0 && argc >= 5) {
        uint8_t addr = (uint8_t)strtoul(argv[2], NULL, 0);
        uint8_t reg = (uint8_t)strtoul(argv[3], NULL, 0);
        uint8_t value = (uint8_t)strtoul(argv[4], NULL, 0);
        int rc = i2c_bus_write_reg8(addr, reg, value, I2C_BUS_DEFAULT_TIMEOUT_MS);
        LOG_INFO("i2c write addr=0x%02x reg=0x%02x value=0x%02x rc=%d\r\n",
                 (unsigned)addr,
                 (unsigned)reg,
                 (unsigned)value,
                 rc);
        return rc;
    }
    LOG_WARN("bad i2c command\r\n");
    return -1;
}

static int cmd_ip5209(int argc, char **argv)
{
    uint8_t value = 0;
    int rc;

    if (argc < 2 || strcmp(argv[1], "status") == 0) {
        power_ip5209_status_t status = power_ip5209_status();
        LOG_INFO("ip5209 addr=0x%02x present=%u int=%u charging=%u boost=%u battery=UNKNOWN\r\n",
                 (unsigned)APP_IP5209_I2C_ADDR,
                 (unsigned)status.i2c_present,
                 (unsigned)status.int_level,
                 (unsigned)status.charging,
                 (unsigned)status.boost_enabled);
        return 0;
    }
    if (strcmp(argv[1], "probe") == 0) {
        rc = i2c_bus_probe(APP_IP5209_I2C_ADDR, I2C_BUS_DEFAULT_TIMEOUT_MS);
        LOG_INFO("ip5209 probe addr=0x%02x rc=%d\r\n", (unsigned)APP_IP5209_I2C_ADDR, rc);
        return rc;
    }
    if (strcmp(argv[1], "read") == 0 && argc >= 3) {
        uint8_t reg = (uint8_t)strtoul(argv[2], NULL, 0);
        rc = power_ip5209_read_register(reg, &value);
        LOG_INFO("ip5209 read reg=0x%02x rc=%d value=0x%02x\r\n", (unsigned)reg, rc, (unsigned)value);
        return rc;
    }
    if (strcmp(argv[1], "write") == 0 && argc >= 4) {
        uint8_t reg = (uint8_t)strtoul(argv[2], NULL, 0);
        value = (uint8_t)strtoul(argv[3], NULL, 0);
        rc = power_ip5209_write_register(reg, value);
        LOG_INFO("ip5209 write reg=0x%02x value=0x%02x rc=%d\r\n",
                 (unsigned)reg,
                 (unsigned)value,
                 rc);
        return rc;
    }
    LOG_INFO("ip5209 status|probe|read <reg>|write <reg> <value>\r\n");
    return 0;
}

static int cmd_settings(int argc, char **argv)
{
    const settings_t *settings = settings_get();

    if (argc < 2 || strcmp(argv[1], "show") == 0) {
        LOG_INFO("settings valid=%u version=%u volume=%u brightness=%u scene=%u motor_limit=%u demo=%u crc=0x%04x flash=%u\r\n",
                 (unsigned)settings_valid(),
                 (unsigned)settings->version,
                 (unsigned)settings->volume,
                 (unsigned)settings->brightness,
                 (unsigned)settings->default_scene,
                 (unsigned)settings->motor_intensity_limit,
                 (unsigned)settings->demo_mode,
                 (unsigned)settings->crc,
                 (unsigned)APP_SETTINGS_FLASH_ENABLE);
        return 0;
    }
    if (strcmp(argv[1], "reset") == 0) {
        settings_factory_reset();
        LOG_INFO("settings factory defaults restored\r\n");
        return 0;
    }
    if (strcmp(argv[1], "save") == 0) {
        int rc = settings_save();
        LOG_INFO("settings save rc=%d flash=%u\r\n", rc, (unsigned)APP_SETTINGS_FLASH_ENABLE);
        return rc;
    }
    if (strcmp(argv[1], "corrupt") == 0) {
        settings_corrupt_for_test();
        LOG_INFO("settings intentionally corrupted; valid=%u motor output unchanged\r\n", (unsigned)settings_valid());
        return 0;
    }

    LOG_WARN("bad settings command\r\n");
    return -1;
}

static int cmd_reset(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    motor_drv8837_off();
    rgb_ws2812_clear();
    (void)audio_wt2003_stop();
    board_init_safe_pins();
    LOG_INFO("software reset requested; safe pins forced\r\n");
    SYS_ResetExecute();
    for (;;) {
    }
}
