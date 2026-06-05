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
#include "timebase.h"
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
static void format_bits8(uint8_t value, char out[10]);
static void log_ip5209_reg8(const char *name, uint8_t reg, uint8_t value);
static void log_ip5209_verbose_dump(const power_ip5209_status_t *status,
                                    const power_ip5209_diagnostics_t *diag);
static int ip5209_rmw_bit(const char *command,
                          const char *reg_name,
                          uint8_t reg,
                          uint8_t mask,
                          uint8_t set_bit);
#if APP_MOTOR_PWM_BACKEND_ENABLE
extern void northpole_ch592_motor_pwm_debug(uint8_t *initialized,
                                            uint32_t *pwm_hz,
                                            uint32_t *timer_cycle_ticks,
                                            uint16_t *pwmx_cycle_ticks,
                                            uint8_t *pwmx_clock_div,
                                            uint16_t *duty_50_permille_ticks);
#endif

FW_WEAK void northpole_diag_force_gpio_output(board_output_id_t output, uint8_t high)
{
    board_output_write(output, high);
}

FW_WEAK void northpole_motor_pwm_diag_apply(motor_driver_id_t driver,
                                            motor_drv8837_mode_t mode,
                                            uint16_t duty_permille)
{
    (void)driver;
    (void)mode;
    (void)duty_permille;
}

static const shell_command_t builtin_commands[] = {
    {"help", "list commands", cmd_help},
    {"version", "print firmware identity", cmd_version},
    {"status", "print board status", cmd_status},
    {"faults", "print sticky fault mask", cmd_faults},
    {"pins", "pins verify", cmd_pins},
    {"safe", "safe check", cmd_safe},
    {"audio", "audio status|raw <hex...>|version|qvol|qstatus|qcount-ext|qperiph|busy|volume <0-31>|play-index <id>|play-name <name>|stop|pause|next|prev|mode <single|single-loop|all-loop|random>|output <spk|dac>|sleep <idle|deep>|format-ext-flash CONFIRM", cmd_audio},
    {"motor", "motor status|arm <seconds>|off|pwm-debug|pwm <A|B|G> <forward|reverse|coast|brake> <duty_permille> <ms>|sine-demo <speed_hz> <amplitude_permille> <ms> [AB|A|B|G|all]|sine-phase <phase_0_31> <amplitude_permille> <ms> [AB|A|B|G|all]|sine-diag-inputs <speed_hz> <ms> [AB|A|B|G|all]|sine-pwm-inputs <speed_hz> <amplitude_permille> <ms> [AB|A|B|G|all]|sine-scope-plot <speed_hz> <amplitude_permille> <ms> [AB|A|B|G|all]|sine-scope-plot-us <slot_us> <amplitude_permille> <steps> [AB|A|B|G|all]|sine-scope-run-us <slot_us> <amplitude_permille> <ms> [AB|A|B|G|all]|sine-scope-clkdiv <fast_clkdiv> <amplitude_permille> <ms> [AB|A|B|G|all]|diag-inputs <A|B|G> <forward|reverse|coast|brake> <ms>", cmd_motor},
    {"rgb", "rgb off|idle-low|one <idx> <r> <g> <b>|all <r> <g> <b>|chase <brightness>|order test|show", cmd_rgb},
    {"hall", "hall read", cmd_hall},
    {"touch", "touch raw", cmd_touch},
    {"i2c", "i2c lines|release-debug|scan|read <addr7> <reg>|write <addr7> <reg> <value>", cmd_i2c},
    {"ip5209", "ip5209 status|dump|probe|read <reg>|write <reg> <value>|boost <on|off>|light-load <enable|disable>|ntc <enable|disable>", cmd_ip5209},
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

static void format_bits8(uint8_t value, char out[10])
{
    for (uint8_t i = 0; i < 4u; ++i) {
        out[i] = (value & (uint8_t)(0x80u >> i)) ? '1' : '0';
    }
    out[4] = ' ';
    for (uint8_t i = 4u; i < 8u; ++i) {
        out[i + 1u] = (value & (uint8_t)(0x80u >> i)) ? '1' : '0';
    }
    out[9] = '\0';
}

static void log_ip5209_reg8(const char *name, uint8_t reg, uint8_t value)
{
    char bits[10];
    format_bits8(value, bits);
    LOG_INFO("%s[0x%02x]=0x%02x, 0b%s\r\n",
             name,
             (unsigned)reg,
             (unsigned)value,
             bits);
}

static void log_ip5209_verbose_dump(const power_ip5209_status_t *status,
                                    const power_ip5209_diagnostics_t *diag)
{
    LOG_INFO("addr7=0x%02x present=%u int_pin=%u charging=%u boost_cfg=%u\r\n",
             (unsigned)APP_IP5209_I2C_ADDR,
             (unsigned)status->i2c_present,
             (unsigned)status->int_level,
             (unsigned)status->charging,
             (unsigned)status->boost_enabled);

    log_ip5209_reg8("REG00", 0x00u, diag->reg00);
    LOG_INFO("  raw/undocumented control/status byte from local bring-up\r\n");

    log_ip5209_reg8("SYS_CTL0", 0x01u, diag->sys_ctl0);
    LOG_INFO("  bit4 flashlight_detect_en=%u\r\n", (unsigned)diag->wled_detect_config);
    LOG_INFO("  bit3 light_en=%u\r\n", (unsigned)diag->wled_enable_config);
    LOG_INFO("  bit2 boost_en=%u  // config bit, not measured VOUT\r\n", (unsigned)diag->boost_enable_config);
    LOG_INFO("  bit1 charger_en=%u\r\n", (unsigned)diag->charger_enable_config);
    LOG_INFO("  bit7_5_reserved=0x%02x bit0_reserved=%u\r\n",
             (unsigned)((diag->sys_ctl0 >> 5) & 0x07u),
             (unsigned)(diag->sys_ctl0 & 0x01u));

    log_ip5209_reg8("SYS_CTL1", 0x02u, diag->sys_ctl1);
    LOG_INFO("  bit1 light_load_shutdown_en=%u\r\n", (unsigned)diag->light_load_shutdown_enable);
    LOG_INFO("  bit0 load_insert_auto_power_on=%u\r\n", (unsigned)diag->load_powerup_enable);
    LOG_INFO("  bit7_2_reserved=0x%02x\r\n", (unsigned)((diag->sys_ctl1 >> 2) & 0x3fu));

    log_ip5209_reg8("SYS_CTL2", 0x0cu, diag->sys_ctl2);
    LOG_INFO("  bits7_3 light_load_shutdown_threshold_n=%u approx_ma=%u\r\n",
             (unsigned)diag->light_load_shutdown_threshold_n,
             (unsigned)diag->light_load_shutdown_threshold_ma);
    LOG_INFO("  bits2_0_reserved=0x%02x\r\n", (unsigned)(diag->sys_ctl2 & 0x07u));

    log_ip5209_reg8("SYS_CTL3", 0x03u, diag->sys_ctl3);
    LOG_INFO("  bits7_6 long_press_time_sel=%u  // 0=1s,1=2s,2=3s,3=4s\r\n",
             (unsigned)diag->long_press_time_sel);
    LOG_INFO("  bit5 double_short_shutdown_en=%u\r\n", (unsigned)diag->button_shutdown_enable);
    LOG_INFO("  bits4_0_reserved=0x%02x\r\n", (unsigned)(diag->sys_ctl3 & 0x1fu));

    log_ip5209_reg8("SYS_CTL4", 0x04u, diag->sys_ctl4);
    LOG_INFO("  bits7_6 shutdown_time_sel=%u  // 0=8s,1=16s,2=32s,3=64s\r\n",
             (unsigned)diag->shutdown_time_sel);
    LOG_INFO("  bit5 vin_pullout_boost_en=%u\r\n", (unsigned)diag->vin_pullout_boost_enable);
    LOG_INFO("  bits4_0_reserved=0x%02x\r\n", (unsigned)(diag->sys_ctl4 & 0x1fu));

    log_ip5209_reg8("SYS_CTL5", 0x07u, diag->sys_ctl5);
    LOG_INFO("  bit6 ntc_disabled=%u  // 0=NTC enabled, 1=NTC disabled\r\n", (unsigned)diag->ntc_disabled);
    LOG_INFO("  bit1 wled_key_double_press=%u\r\n", (unsigned)diag->wled_double_press_mode);
    LOG_INFO("  bit0 shutdown_key_long_press=%u\r\n", (unsigned)diag->shutdown_long_press_mode);
    LOG_INFO("  bit7_reserved=%u bits5_2_reserved=0x%02x\r\n",
             (unsigned)((diag->sys_ctl5 >> 7) & 0x01u),
             (unsigned)((diag->sys_ctl5 >> 2) & 0x0fu));

    log_ip5209_reg8("Charger_CTL1", 0x22u, diag->charger_ctl1);
    LOG_INFO("  bits3_2 charge_vout_uv_loop_sel=%u  // 0=4.53V,1=4.63V,2=4.73V,3=4.83V\r\n",
             (unsigned)diag->charge_uv_loop_sel);
    LOG_INFO("  reserved=0x%02x\r\n",
             (unsigned)(diag->charger_ctl1 & (uint8_t)~0x0cu));

    log_ip5209_reg8("Charger_CTL2", 0x24u, diag->charger_ctl2);
    LOG_INFO("  bits6_5 battery_type_sel=%u  // 0=4.2V,1=4.3V,2=4.35V,3=reserved\r\n",
             (unsigned)diag->battery_type_sel);
    LOG_INFO("  bits2_1 charge_voltage_offset_sel=%u  // 0=0mV,1=14mV,2=28mV,3=42mV\r\n",
             (unsigned)diag->charge_voltage_offset_sel);
    LOG_INFO("  reserved=0x%02x\r\n",
             (unsigned)(diag->charger_ctl2 & (uint8_t)~0x66u));

    log_ip5209_reg8("Charge_Current_CTL", 0x25u, diag->charge_current_ctl);
    LOG_INFO("  bits4_0 charge_current_code=0x%02x approx_ma=%u\r\n",
             (unsigned)diag->charge_current_setting,
             (unsigned)diag->charge_current_setting_ma);
    LOG_INFO("  bits7_5_reserved=0x%02x\r\n", (unsigned)((diag->charge_current_ctl >> 5) & 0x07u));

    log_ip5209_reg8("CHG_DIG_CTL4", 0x26u, diag->chg_dig_ctl4);
    LOG_INFO("  bit6 battery_type_from_vset_pin=%u  // 0=register 0x24, 1=external VSET pin\r\n",
             (unsigned)diag->battery_type_from_vset_pin);
    LOG_INFO("  reserved=0x%02x\r\n",
             (unsigned)(diag->chg_dig_ctl4 & (uint8_t)~0x40u));

    log_ip5209_reg8("MFP_CTL0", 0x51u, diag->mfp_ctl0);
    LOG_INFO("  bits5_4 light_sel=%u  // 0=WLED,1=GPIO2,2=VREF,3=reserved\r\n",
             (unsigned)diag->mfp_light_sel);
    LOG_INFO("  bits3_2 l4_sel=%u  // 0=L4,1=GPIO1\r\n", (unsigned)diag->mfp_l4_sel);
    LOG_INFO("  bits1_0 l3_sel=%u  // 0=L3,1=GPIO0\r\n", (unsigned)diag->mfp_l3_sel);
    LOG_INFO("  bits7_6_reserved=0x%02x\r\n", (unsigned)((diag->mfp_ctl0 >> 6) & 0x03u));

    log_ip5209_reg8("MFP_CTL1", 0x52u, diag->mfp_ctl1);
    LOG_INFO("  bits3_2 vset_sel=%u  // 0=VSET pin,1=GPIO4\r\n", (unsigned)diag->mfp_vset_sel);
    LOG_INFO("  bits1_0 rset_sel=%u  // 0=RSET pin,1=GPIO3\r\n", (unsigned)diag->mfp_rset_sel);
    LOG_INFO("  bits7_4_reserved=0x%02x\r\n", (unsigned)((diag->mfp_ctl1 >> 4) & 0x0fu));

    log_ip5209_reg8("GPIO_INEN", 0x53u, diag->gpio_inen);
    LOG_INFO("  bits4_0 gpio_input_enable=0x%02x\r\n", (unsigned)(diag->gpio_inen & 0x1fu));
    log_ip5209_reg8("GPIO_OUTEN", 0x54u, diag->gpio_outen);
    LOG_INFO("  bits4_0 gpio_output_enable=0x%02x\r\n", (unsigned)(diag->gpio_outen & 0x1fu));
    log_ip5209_reg8("GPIO_DATA", 0x55u, diag->gpio_data);
    LOG_INFO("  bits4_0 gpio_data=0x%02x\r\n", (unsigned)(diag->gpio_data & 0x1fu));

    log_ip5209_reg8("READ70", 0x70u, diag->read70);
    LOG_INFO("  raw read/status byte; not fully decoded yet\r\n");

    log_ip5209_reg8("READ0", 0x71u, diag->read0);
    LOG_INFO("  bits7_5 charge_state=%u/%s\r\n",
             (unsigned)diag->charge_state,
             power_ip5209_charge_state_name(diag->charge_state));
    LOG_INFO("  bit4 chgop=%u\r\n", (unsigned)diag->chgop);
    LOG_INFO("  bit3 charge_done=%u\r\n", (unsigned)diag->charge_done);
    LOG_INFO("  bit2 cv_timeout=%u bit1 charge_timeout=%u bit0 trickle_timeout=%u\r\n",
             (unsigned)diag->charge_cv_timeout,
             (unsigned)diag->charge_timeout,
             (unsigned)diag->charge_trickle_timeout);

    log_ip5209_reg8("READ1", 0x72u, diag->read1);
    LOG_INFO("  bit7 light_led_present=%u\r\n", (unsigned)diag->light_led_present);
    LOG_INFO("  bit6 light_load=%u  // 0=>75mA, 1=<75mA\r\n", (unsigned)diag->light_load);
    LOG_INFO("  bit5 vin_overvoltage=%u\r\n", (unsigned)diag->vin_overvoltage);
    LOG_INFO("  bits4_0_reserved=0x%02x\r\n", (unsigned)(diag->read1 & 0x1fu));

    log_ip5209_reg8("READ2", 0x77u, diag->read2);
    LOG_INFO("  bit3 key_pressed=%u\r\n", (unsigned)diag->key_pressed);
    LOG_INFO("  bit1 key_long=%u\r\n", (unsigned)diag->key_long);
    LOG_INFO("  bit0 key_short=%u\r\n", (unsigned)diag->key_short);
    LOG_INFO("  reserved=0x%02x\r\n",
             (unsigned)(diag->read2 & (uint8_t)~0x0bu));

    log_ip5209_reg8("BATVADC_L", 0xa2u, diag->batvadc_l);
    log_ip5209_reg8("BATVADC_H", 0xa3u, diag->batvadc_h);
    LOG_INFO("  BATV=%ldmV valid=%u\r\n",
             (long)diag->battery_mv,
             (unsigned)diag->battery_mv_valid);
    log_ip5209_reg8("BATIADC_L", 0xa4u, diag->batiadc_l);
    log_ip5209_reg8("BATIADC_H", 0xa5u, diag->batiadc_h);
    LOG_INFO("  BATI=%ldmA valid=%u\r\n",
             (long)diag->battery_current_ma,
             (unsigned)diag->battery_current_ma_valid);
    log_ip5209_reg8("BATOCV_L", 0xa8u, diag->batocv_l);
    log_ip5209_reg8("BATOCV_H", 0xa9u, diag->batocv_h);
    LOG_INFO("  BATOCV=%ldmV valid=%u\r\n",
             (long)diag->battery_ocv_mv,
             (unsigned)diag->battery_ocv_mv_valid);
}

static int ip5209_rmw_bit(const char *command,
                          const char *reg_name,
                          uint8_t reg,
                          uint8_t mask,
                          uint8_t set_bit)
{
    uint8_t old_value;
    uint8_t new_value;
    uint8_t readback = 0;
    int rc;
    int verify_rc;

    rc = power_ip5209_read_register(reg, &old_value);
    if (rc) {
        LOG_INFO("ip5209 %s read %s[0x%02x] rc=%d\r\n",
                 command,
                 reg_name,
                 (unsigned)reg,
                 rc);
        return rc;
    }

    new_value = set_bit ? (uint8_t)(old_value | mask) : (uint8_t)(old_value & (uint8_t)~mask);
    rc = power_ip5209_write_register(reg, new_value);
    verify_rc = power_ip5209_read_register(reg, &readback);

    LOG_INFO("ip5209 %s %s[0x%02x] mask=0x%02x old=0x%02x new=0x%02x write_rc=%d readback_rc=%d readback=0x%02x\r\n",
             command,
             reg_name,
             (unsigned)reg,
             (unsigned)mask,
             (unsigned)old_value,
             (unsigned)new_value,
             rc,
             verify_rc,
             (unsigned)readback);

    if (rc) {
        return rc;
    }
    return verify_rc;
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
    }
#if !APP_TARGET_ENABLE_AUDIO
    LOG_WARN("audio hardware commands disabled in this target ladder build\r\n");
    return -1;
#endif
    if (strcmp(argv[1], "raw") == 0 && argc >= 3) {
#if APP_DEV_BOARD_BRINGUP_APP_SMOKE
        LOG_WARN("audio hardware commands disabled in dev-board smoke build\r\n");
        return -1;
#endif
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

static board_output_id_t motor_diag_in1_pin(motor_driver_id_t driver)
{
    static const board_output_id_t pins[MOTOR_DRV_COUNT] = {
        BOARD_OUTPUT_PWM_A1,
        BOARD_OUTPUT_PWM_B1,
        BOARD_OUTPUT_PWM_G1,
    };

    return pins[driver];
}

static board_output_id_t motor_diag_in2_pin(motor_driver_id_t driver)
{
    static const board_output_id_t pins[MOTOR_DRV_COUNT] = {
        BOARD_OUTPUT_PWM_A2,
        BOARD_OUTPUT_PWM_B2,
        BOARD_OUTPUT_PWM_G2,
    };

    return pins[driver];
}

static void motor_diag_mode_levels(motor_drv8837_mode_t mode, uint8_t *in1, uint8_t *in2)
{
    switch (mode) {
    case MOTOR_DRV_FORWARD:
        *in1 = 1u;
        *in2 = 0u;
        break;
    case MOTOR_DRV_REVERSE:
        *in1 = 0u;
        *in2 = 1u;
        break;
    case MOTOR_DRV_BRAKE:
        *in1 = 1u;
        *in2 = 1u;
        break;
    case MOTOR_DRV_COAST:
    default:
        *in1 = 0u;
        *in2 = 0u;
        break;
    }
}

static int16_t sine_demo_sample(uint8_t phase)
{
    static const int16_t sine_q15[32] = {
        0, 6393, 12540, 18204, 23170, 27245, 30273, 32137,
        32767, 32137, 30273, 27245, 23170, 18204, 12540, 6393,
        0, -6393, -12540, -18204, -23170, -27245, -30273, -32137,
        -32767, -32137, -30273, -27245, -23170, -18204, -12540, -6393,
    };

    return sine_q15[phase & 31u];
}

static uint16_t sine_demo_duty_limited(int16_t sample, uint16_t amplitude_permille, uint16_t limit_permille)
{
    uint32_t magnitude = sample < 0 ? (uint32_t)(-sample) : (uint32_t)sample;
    uint32_t duty = (magnitude * amplitude_permille + 16383u) / 32767u;

    if (duty > limit_permille) {
        duty = limit_permille;
    }
    return (uint16_t)duty;
}

static uint16_t sine_demo_duty(int16_t sample, uint16_t amplitude_permille)
{
    return sine_demo_duty_limited(sample, amplitude_permille, APP_MOTOR_PWM_MAX_DUTY_PERMILLE);
}

static int sine_demo_apply_driver(motor_driver_id_t driver,
                                  int16_t sample,
                                  uint16_t amplitude_permille,
                                  uint32_t step_hold_ms)
{
    uint16_t duty = sine_demo_duty(sample, amplitude_permille);
    motor_drv8837_mode_t mode;

    if (duty == 0u) {
        mode = MOTOR_DRV_COAST;
    } else if (sample >= 0) {
        mode = MOTOR_DRV_FORWARD;
    } else {
        mode = MOTOR_DRV_REVERSE;
    }

    return motor_drv8837_command_for(driver, mode, duty, step_hold_ms);
}

static motor_drv8837_mode_t sine_demo_mode_for_sample(int16_t sample, uint16_t duty)
{
    if (duty == 0u) {
        return MOTOR_DRV_COAST;
    }
    if (sample >= 0) {
        return MOTOR_DRV_FORWARD;
    }
    return MOTOR_DRV_REVERSE;
}

static void sine_diag_all_inputs_low(void)
{
    for (uint8_t i = 0u; i < MOTOR_DRV_COUNT; ++i) {
        motor_driver_id_t driver = (motor_driver_id_t)i;
        northpole_diag_force_gpio_output(motor_diag_in1_pin(driver), 0u);
        northpole_diag_force_gpio_output(motor_diag_in2_pin(driver), 0u);
    }
}

static void sine_diag_apply_driver(motor_driver_id_t driver, int16_t sample)
{
    uint8_t in1 = 0u;
    uint8_t in2 = 0u;

    if (sample > 0) {
        in1 = 1u;
    } else if (sample < 0) {
        in2 = 1u;
    }

    northpole_diag_force_gpio_output(motor_diag_in1_pin(driver), in1);
    northpole_diag_force_gpio_output(motor_diag_in2_pin(driver), in2);
}

static void sine_pwm_diag_apply_driver(motor_driver_id_t driver,
                                       int16_t sample,
                                       uint16_t amplitude_permille)
{
    uint16_t duty = sine_demo_duty_limited(sample, amplitude_permille, 1000u);
    motor_drv8837_mode_t mode = sine_demo_mode_for_sample(sample, duty);

    northpole_motor_pwm_diag_apply(driver, mode, duty);
}

typedef struct {
    board_output_id_t pin;
    uint32_t off_after_ms;
    uint8_t active;
} sine_scope_plot_event_t;

static uint32_t sine_scope_plot_high_ms(uint32_t slot_ms, uint16_t duty_permille)
{
    uint32_t high_ms = (slot_ms * (uint32_t)duty_permille + 500u) / 1000u;

    if (duty_permille > 0u && high_ms == 0u) {
        high_ms = 1u;
    }
    if (high_ms > slot_ms) {
        high_ms = slot_ms;
    }
    return high_ms;
}

static void sine_scope_plot_prepare_driver(motor_driver_id_t driver,
                                           int16_t sample,
                                           uint16_t amplitude_permille,
                                           uint32_t slot_ms,
                                           sine_scope_plot_event_t *events,
                                           uint8_t *event_count)
{
    board_output_id_t in1_pin = motor_diag_in1_pin(driver);
    board_output_id_t in2_pin = motor_diag_in2_pin(driver);
    uint16_t duty = sine_demo_duty_limited(sample, amplitude_permille, 1000u);
    uint32_t high_ms = sine_scope_plot_high_ms(slot_ms, duty);
    board_output_id_t active_pin;

    northpole_diag_force_gpio_output(in1_pin, 0u);
    northpole_diag_force_gpio_output(in2_pin, 0u);

    if (high_ms == 0u) {
        return;
    }

    active_pin = sample >= 0 ? in1_pin : in2_pin;
    northpole_diag_force_gpio_output(active_pin, 1u);
    if (*event_count < 6u) {
        events[*event_count].pin = active_pin;
        events[*event_count].off_after_ms = high_ms;
        events[*event_count].active = 1u;
        ++(*event_count);
    }
}

static void sine_scope_plot_run_slot(uint8_t phase,
                                     uint16_t amplitude_permille,
                                     uint32_t slot_ms,
                                     uint8_t target_flags)
{
    sine_scope_plot_event_t events[6];
    uint8_t event_count = 0u;
    uint32_t elapsed_ms = 0u;

    for (uint8_t i = 0u; i < 6u; ++i) {
        events[i].pin = BOARD_OUTPUT_PWM_A1;
        events[i].off_after_ms = 0u;
        events[i].active = 0u;
    }

    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
    if (target_flags & 0x01u) {
        sine_scope_plot_prepare_driver(MOTOR_DRV_A,
                                       sine_demo_sample(phase),
                                       amplitude_permille,
                                       slot_ms,
                                       events,
                                       &event_count);
    }
    if (target_flags & 0x02u) {
        sine_scope_plot_prepare_driver(MOTOR_DRV_B,
                                       sine_demo_sample((uint8_t)(phase + 8u)),
                                       amplitude_permille,
                                       slot_ms,
                                       events,
                                       &event_count);
    }
    if (target_flags & 0x04u) {
        sine_scope_plot_prepare_driver(MOTOR_DRV_G,
                                       sine_demo_sample(phase),
                                       amplitude_permille,
                                       slot_ms,
                                       events,
                                       &event_count);
    }

    while (elapsed_ms < slot_ms) {
        uint32_t next_ms = slot_ms;
        uint8_t any_active = 0u;

        for (uint8_t i = 0u; i < event_count; ++i) {
            if (events[i].active && events[i].off_after_ms > elapsed_ms) {
                any_active = 1u;
                if (events[i].off_after_ms < next_ms) {
                    next_ms = events[i].off_after_ms;
                }
            }
        }

        if (!any_active) {
            timebase_delay_ms(slot_ms - elapsed_ms);
            elapsed_ms = slot_ms;
            break;
        }

        if (next_ms > elapsed_ms) {
            timebase_delay_ms(next_ms - elapsed_ms);
            elapsed_ms = next_ms;
        }

        for (uint8_t i = 0u; i < event_count; ++i) {
            if (events[i].active && events[i].off_after_ms <= elapsed_ms) {
                northpole_diag_force_gpio_output(events[i].pin, 0u);
                events[i].active = 0u;
            }
        }
    }

    sine_diag_all_inputs_low();
    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
}

typedef struct {
    board_output_id_t pin;
    uint32_t off_after_us;
    uint8_t active;
} sine_scope_plot_us_event_t;

static uint32_t sine_scope_plot_high_us(uint32_t slot_us, uint16_t duty_permille)
{
    uint32_t high_us = (slot_us * (uint32_t)duty_permille + 500u) / 1000u;

    if (duty_permille > 0u && high_us == 0u) {
        high_us = 1u;
    }
    if (high_us > slot_us) {
        high_us = slot_us;
    }
    return high_us;
}

static void sine_scope_plot_prepare_driver_us(motor_driver_id_t driver,
                                              int16_t sample,
                                              uint16_t amplitude_permille,
                                              uint32_t slot_us,
                                              sine_scope_plot_us_event_t *events,
                                              uint8_t *event_count)
{
    board_output_id_t in1_pin = motor_diag_in1_pin(driver);
    board_output_id_t in2_pin = motor_diag_in2_pin(driver);
    uint16_t duty = sine_demo_duty_limited(sample, amplitude_permille, 1000u);
    uint32_t high_us = sine_scope_plot_high_us(slot_us, duty);
    board_output_id_t active_pin;

    northpole_diag_force_gpio_output(in1_pin, 0u);
    northpole_diag_force_gpio_output(in2_pin, 0u);

    if (high_us == 0u) {
        return;
    }

    active_pin = sample >= 0 ? in1_pin : in2_pin;
    northpole_diag_force_gpio_output(active_pin, 1u);
    if (*event_count < 6u) {
        events[*event_count].pin = active_pin;
        events[*event_count].off_after_us = high_us;
        events[*event_count].active = 1u;
        ++(*event_count);
    }
}

static void sine_scope_plot_run_slot_us(uint8_t phase,
                                        uint16_t amplitude_permille,
                                        uint32_t slot_us,
                                        uint8_t target_flags)
{
    sine_scope_plot_us_event_t events[6];
    uint8_t event_count = 0u;
    uint32_t elapsed_us = 0u;

    for (uint8_t i = 0u; i < 6u; ++i) {
        events[i].pin = BOARD_OUTPUT_PWM_A1;
        events[i].off_after_us = 0u;
        events[i].active = 0u;
    }

    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
    if (target_flags & 0x01u) {
        sine_scope_plot_prepare_driver_us(MOTOR_DRV_A,
                                          sine_demo_sample(phase),
                                          amplitude_permille,
                                          slot_us,
                                          events,
                                          &event_count);
    }
    if (target_flags & 0x02u) {
        sine_scope_plot_prepare_driver_us(MOTOR_DRV_B,
                                          sine_demo_sample((uint8_t)(phase + 8u)),
                                          amplitude_permille,
                                          slot_us,
                                          events,
                                          &event_count);
    }
    if (target_flags & 0x04u) {
        sine_scope_plot_prepare_driver_us(MOTOR_DRV_G,
                                          sine_demo_sample(phase),
                                          amplitude_permille,
                                          slot_us,
                                          events,
                                          &event_count);
    }

    while (elapsed_us < slot_us) {
        uint32_t next_us = slot_us;
        uint8_t any_active = 0u;

        for (uint8_t i = 0u; i < event_count; ++i) {
            if (events[i].active && events[i].off_after_us > elapsed_us) {
                any_active = 1u;
                if (events[i].off_after_us < next_us) {
                    next_us = events[i].off_after_us;
                }
            }
        }

        if (!any_active) {
            timebase_delay_us(slot_us - elapsed_us);
            elapsed_us = slot_us;
            break;
        }

        if (next_us > elapsed_us) {
            timebase_delay_us(next_us - elapsed_us);
            elapsed_us = next_us;
        }

        for (uint8_t i = 0u; i < event_count; ++i) {
            if (events[i].active && events[i].off_after_us <= elapsed_us) {
                northpole_diag_force_gpio_output(events[i].pin, 0u);
                events[i].active = 0u;
            }
        }
    }

    sine_diag_all_inputs_low();
    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
}

static uint8_t sine_demo_target_flags(const char *target)
{
    if (target == NULL || strcmp(target, "AB") == 0 || strcmp(target, "ab") == 0) {
        return 0x03u;
    }
    if (strcmp(target, "A") == 0 || strcmp(target, "a") == 0) {
        return 0x01u;
    }
    if (strcmp(target, "B") == 0 || strcmp(target, "b") == 0) {
        return 0x02u;
    }
    if (strcmp(target, "G") == 0 || strcmp(target, "g") == 0) {
        return 0x04u;
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "ALL") == 0) {
        return 0x07u;
    }
    return 0u;
}

static int run_motor_sine_demo(uint32_t speed_hz,
                               uint16_t amplitude_permille,
                               uint32_t duration_ms,
                               uint8_t target_flags)
{
    const uint32_t samples_per_cycle = 32u;
    uint32_t step_ms;
    uint32_t start_ms;
    uint32_t deadline_ms;
    uint32_t step_count = 0u;
    int last_rc = 0;

    if (speed_hz == 0u || speed_hz > 50u || duration_ms == 0u || target_flags == 0u) {
        return -1;
    }
    if (amplitude_permille > APP_MOTOR_PWM_MAX_DUTY_PERMILLE) {
        amplitude_permille = APP_MOTOR_PWM_MAX_DUTY_PERMILLE;
    }
    if (duration_ms > APP_BRINGUP_MOTOR_ARM_MAX_MS) {
        duration_ms = APP_BRINGUP_MOTOR_ARM_MAX_MS;
    }

    step_ms = 1000u / (speed_hz * samples_per_cycle);
    if (step_ms == 0u) {
        step_ms = 1u;
    }

    motor_drv8837_arm(duration_ms + 50u);
    start_ms = timebase_ms();
    deadline_ms = start_ms + duration_ms;

    LOG_INFO("motor sine-demo speed_hz=%lu amplitude=%u duration_ms=%lu step_ms=%lu targets=%s%s%s\r\n",
             (unsigned long)speed_hz,
             (unsigned)amplitude_permille,
             (unsigned long)duration_ms,
             (unsigned long)step_ms,
             (target_flags & 0x01u) ? "A" : "",
             (target_flags & 0x02u) ? "B" : "",
             (target_flags & 0x04u) ? "G" : "");

    while ((int32_t)(timebase_ms() - deadline_ms) < 0) {
        uint8_t phase = (uint8_t)(step_count & 31u);
        uint32_t hold_ms = step_ms + 10u;

        if (target_flags & 0x01u) {
            last_rc = sine_demo_apply_driver(MOTOR_DRV_A,
                                             sine_demo_sample(phase),
                                             amplitude_permille,
                                             hold_ms);
        }
        if (target_flags & 0x02u) {
            last_rc = sine_demo_apply_driver(MOTOR_DRV_B,
                                             sine_demo_sample((uint8_t)(phase + 8u)),
                                             amplitude_permille,
                                             hold_ms);
        }
        if (target_flags & 0x04u) {
            last_rc = sine_demo_apply_driver(MOTOR_DRV_G,
                                             sine_demo_sample(phase),
                                             amplitude_permille,
                                             hold_ms);
        }

        if (last_rc != 0) {
            break;
        }
        timebase_delay_ms(step_ms);
        ++step_count;
    }

    motor_drv8837_off();
    LOG_INFO("motor sine-demo done steps=%lu rc=%d sleep=%u\r\n",
             (unsigned long)step_count,
             last_rc,
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP));
    return last_rc;
}

static int run_motor_sine_phase(uint8_t phase,
                                uint16_t amplitude_permille,
                                uint32_t duration_ms,
                                uint8_t target_flags)
{
    int16_t sample_a;
    int16_t sample_b;
    int16_t sample_g;
    uint16_t duty_a;
    uint16_t duty_b;
    uint16_t duty_g;
    int last_rc = 0;

    if (duration_ms == 0u || target_flags == 0u) {
        return -1;
    }
    if (amplitude_permille > APP_MOTOR_PWM_MAX_DUTY_PERMILLE) {
        amplitude_permille = APP_MOTOR_PWM_MAX_DUTY_PERMILLE;
    }
    if (duration_ms > APP_BRINGUP_MOTOR_ARM_MAX_MS) {
        duration_ms = APP_BRINGUP_MOTOR_ARM_MAX_MS;
    }

    phase &= 31u;
    sample_a = sine_demo_sample(phase);
    sample_b = sine_demo_sample((uint8_t)(phase + 8u));
    sample_g = sine_demo_sample(phase);
    duty_a = sine_demo_duty(sample_a, amplitude_permille);
    duty_b = sine_demo_duty(sample_b, amplitude_permille);
    duty_g = sine_demo_duty(sample_g, amplitude_permille);

    motor_drv8837_arm(duration_ms + 50u);

    LOG_INFO("motor sine-phase phase=%u amplitude=%u duration_ms=%lu targets=%s%s%s\r\n",
             (unsigned)phase,
             (unsigned)amplitude_permille,
             (unsigned long)duration_ms,
             (target_flags & 0x01u) ? "A" : "",
             (target_flags & 0x02u) ? "B" : "",
             (target_flags & 0x04u) ? "G" : "");

    if (target_flags & 0x01u) {
        last_rc = sine_demo_apply_driver(MOTOR_DRV_A, sample_a, amplitude_permille, duration_ms + 10u);
        LOG_INFO("motor sine-phase A sample=%d mode=%s duty=%u rc=%d\r\n",
                 (int)sample_a,
                 motor_drv8837_mode_name(sine_demo_mode_for_sample(sample_a, duty_a)),
                 (unsigned)duty_a,
                 last_rc);
    }
    if (target_flags & 0x02u) {
        last_rc = sine_demo_apply_driver(MOTOR_DRV_B, sample_b, amplitude_permille, duration_ms + 10u);
        LOG_INFO("motor sine-phase B sample=%d mode=%s duty=%u rc=%d\r\n",
                 (int)sample_b,
                 motor_drv8837_mode_name(sine_demo_mode_for_sample(sample_b, duty_b)),
                 (unsigned)duty_b,
                 last_rc);
    }
    if (target_flags & 0x04u) {
        last_rc = sine_demo_apply_driver(MOTOR_DRV_G, sample_g, amplitude_permille, duration_ms + 10u);
        LOG_INFO("motor sine-phase G sample=%d mode=%s duty=%u rc=%d\r\n",
                 (int)sample_g,
                 motor_drv8837_mode_name(sine_demo_mode_for_sample(sample_g, duty_g)),
                 (unsigned)duty_g,
                 last_rc);
    }

    timebase_delay_ms(duration_ms);
    motor_drv8837_off();
    LOG_INFO("motor sine-phase done rc=%d sleep=%u\r\n",
             last_rc,
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP));
    return last_rc;
}

static int run_motor_sine_diag_inputs(uint32_t speed_hz,
                                      uint32_t duration_ms,
                                      uint8_t target_flags)
{
    const uint32_t samples_per_cycle = 32u;
    uint32_t step_ms;
    uint32_t start_ms;
    uint32_t deadline_ms;
    uint32_t step_count = 0u;

    if (speed_hz == 0u || speed_hz > 50u || duration_ms == 0u || target_flags == 0u) {
        return -1;
    }
    if (duration_ms > 10000UL) {
        duration_ms = 10000UL;
    }

    step_ms = 1000u / (speed_hz * samples_per_cycle);
    if (step_ms == 0u) {
        step_ms = 1u;
    }

    motor_drv8837_off();
    board_init_safe_pins();
    sine_diag_all_inputs_low();
    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);

    start_ms = timebase_ms();
    deadline_ms = start_ms + duration_ms;

    LOG_INFO("motor sine-diag-inputs speed_hz=%lu duration_ms=%lu step_ms=%lu targets=%s%s%s sleep=%u\r\n",
             (unsigned long)speed_hz,
             (unsigned long)duration_ms,
             (unsigned long)step_ms,
             (target_flags & 0x01u) ? "A" : "",
             (target_flags & 0x02u) ? "B" : "",
             (target_flags & 0x04u) ? "G" : "",
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP));

    while ((int32_t)(timebase_ms() - deadline_ms) < 0) {
        uint8_t phase = (uint8_t)(step_count & 31u);

        northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
        if (target_flags & 0x01u) {
            sine_diag_apply_driver(MOTOR_DRV_A, sine_demo_sample(phase));
        }
        if (target_flags & 0x02u) {
            sine_diag_apply_driver(MOTOR_DRV_B, sine_demo_sample((uint8_t)(phase + 8u)));
        }
        if (target_flags & 0x04u) {
            sine_diag_apply_driver(MOTOR_DRV_G, sine_demo_sample(phase));
        }

        timebase_delay_ms(step_ms);
        ++step_count;
    }

    sine_diag_all_inputs_low();
    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
    LOG_INFO("motor sine-diag-inputs done steps=%lu sleep=%u A1=%u A2=%u B1=%u B2=%u G1=%u G2=%u\r\n",
             (unsigned long)step_count,
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_A1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_A2),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_B1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_B2),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_G1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_G2));
    return 0;
}

static int run_motor_sine_pwm_inputs(uint32_t speed_hz,
                                     uint16_t amplitude_permille,
                                     uint32_t duration_ms,
                                     uint8_t target_flags)
{
    const uint32_t samples_per_cycle = 32u;
    uint32_t step_ms;
    uint32_t start_ms;
    uint32_t deadline_ms;
    uint32_t step_count = 0u;

    if (speed_hz == 0u || speed_hz > 50u || duration_ms == 0u || target_flags == 0u) {
        return -1;
    }
    if (amplitude_permille > 1000u) {
        amplitude_permille = 1000u;
    }
    if (duration_ms > 10000UL) {
        duration_ms = 10000UL;
    }

    step_ms = 1000u / (speed_hz * samples_per_cycle);
    if (step_ms == 0u) {
        step_ms = 1u;
    }

    motor_drv8837_off();
    board_init_safe_pins();
    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);

    start_ms = timebase_ms();
    deadline_ms = start_ms + duration_ms;

    LOG_INFO("motor sine-pwm-inputs speed_hz=%lu amplitude=%u duration_ms=%lu step_ms=%lu targets=%s%s%s sleep=%u scope_only=1\r\n",
             (unsigned long)speed_hz,
             (unsigned)amplitude_permille,
             (unsigned long)duration_ms,
             (unsigned long)step_ms,
             (target_flags & 0x01u) ? "A" : "",
             (target_flags & 0x02u) ? "B" : "",
             (target_flags & 0x04u) ? "G" : "",
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP));

    while ((int32_t)(timebase_ms() - deadline_ms) < 0) {
        uint8_t phase = (uint8_t)(step_count & 31u);

        northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
        if (target_flags & 0x01u) {
            sine_pwm_diag_apply_driver(MOTOR_DRV_A,
                                       sine_demo_sample(phase),
                                       amplitude_permille);
        }
        if (target_flags & 0x02u) {
            sine_pwm_diag_apply_driver(MOTOR_DRV_B,
                                       sine_demo_sample((uint8_t)(phase + 8u)),
                                       amplitude_permille);
        }
        if (target_flags & 0x04u) {
            sine_pwm_diag_apply_driver(MOTOR_DRV_G,
                                       sine_demo_sample(phase),
                                       amplitude_permille);
        }
        northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);

        timebase_delay_ms(step_ms);
        ++step_count;
    }

    northpole_motor_pwm_diag_apply(MOTOR_DRV_A, MOTOR_DRV_COAST, 0u);
    northpole_motor_pwm_diag_apply(MOTOR_DRV_B, MOTOR_DRV_COAST, 0u);
    northpole_motor_pwm_diag_apply(MOTOR_DRV_G, MOTOR_DRV_COAST, 0u);
    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
    LOG_INFO("motor sine-pwm-inputs done steps=%lu sleep=%u\r\n",
             (unsigned long)step_count,
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP));
    return 0;
}

static int run_motor_sine_scope_plot(uint32_t speed_hz,
                                     uint16_t amplitude_permille,
                                     uint32_t duration_ms,
                                     uint8_t target_flags)
{
    const uint32_t samples_per_cycle = 32u;
    uint32_t slot_ms;
    uint32_t start_ms;
    uint32_t deadline_ms;
    uint32_t step_count = 0u;

    if (speed_hz == 0u || speed_hz > 10u || duration_ms == 0u || target_flags == 0u) {
        return -1;
    }
    if (amplitude_permille > 1000u) {
        amplitude_permille = 1000u;
    }
    if (duration_ms > 10000UL) {
        duration_ms = 10000UL;
    }

    slot_ms = 1000u / (speed_hz * samples_per_cycle);
    if (slot_ms == 0u) {
        slot_ms = 1u;
    }

    motor_drv8837_off();
    board_init_safe_pins();
    sine_diag_all_inputs_low();
    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);

    start_ms = timebase_ms();
    deadline_ms = start_ms + duration_ms;

    LOG_INFO("motor sine-scope-plot speed_hz=%lu amplitude=%u duration_ms=%lu slot_ms=%lu targets=%s%s%s sleep=%u visualized_pwm=1\r\n",
             (unsigned long)speed_hz,
             (unsigned)amplitude_permille,
             (unsigned long)duration_ms,
             (unsigned long)slot_ms,
             (target_flags & 0x01u) ? "A" : "",
             (target_flags & 0x02u) ? "B" : "",
             (target_flags & 0x04u) ? "G" : "",
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP));

    while ((int32_t)(timebase_ms() - deadline_ms) < 0) {
        uint8_t phase = (uint8_t)(step_count & 31u);

        sine_scope_plot_run_slot(phase, amplitude_permille, slot_ms, target_flags);
        ++step_count;
    }

    sine_diag_all_inputs_low();
    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
    LOG_INFO("motor sine-scope-plot done steps=%lu sleep=%u A1=%u A2=%u B1=%u B2=%u G1=%u G2=%u\r\n",
             (unsigned long)step_count,
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_A1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_A2),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_B1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_B2),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_G1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_G2));
    return 0;
}

static int run_motor_sine_scope_plot_us(uint32_t slot_us,
                                        uint16_t amplitude_permille,
                                        uint32_t steps,
                                        uint8_t target_flags)
{
    uint32_t step_count = 0u;
    uint64_t total_us;

    if (slot_us < 2u || slot_us > 1000000UL || steps == 0u || target_flags == 0u) {
        return -1;
    }
    if (steps > 4096UL) {
        steps = 4096UL;
    }
    total_us = (uint64_t)slot_us * (uint64_t)steps;
    if (total_us > 10000000ULL) {
        return -1;
    }
    if (amplitude_permille > 1000u) {
        amplitude_permille = 1000u;
    }

    motor_drv8837_off();
    board_init_safe_pins();
    sine_diag_all_inputs_low();
    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);

    LOG_INFO("motor sine-scope-plot-us slot_us=%lu amplitude=%u steps=%lu electrical_cycle_us=%lu targets=%s%s%s sleep=%u visualized_pwm=1\r\n",
             (unsigned long)slot_us,
             (unsigned)amplitude_permille,
             (unsigned long)steps,
             (unsigned long)(slot_us * 32u),
             (target_flags & 0x01u) ? "A" : "",
             (target_flags & 0x02u) ? "B" : "",
             (target_flags & 0x04u) ? "G" : "",
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP));

    while (step_count < steps) {
        uint8_t phase = (uint8_t)(step_count & 31u);

        sine_scope_plot_run_slot_us(phase, amplitude_permille, slot_us, target_flags);
        ++step_count;
    }

    sine_diag_all_inputs_low();
    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
    LOG_INFO("motor sine-scope-plot-us done steps=%lu sleep=%u A1=%u A2=%u B1=%u B2=%u G1=%u G2=%u\r\n",
             (unsigned long)step_count,
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_A1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_A2),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_B1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_B2),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_G1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_G2));
    return 0;
}

static int run_motor_sine_scope_run_us(uint32_t slot_us,
                                       uint16_t amplitude_permille,
                                       uint32_t duration_ms,
                                       uint8_t target_flags)
{
    uint32_t start_ms;
    uint32_t deadline_ms;
    uint32_t step_count = 0u;
    uint64_t cycle_us;
    uint64_t electrical_hz_x1000;

    if (slot_us < 1u || slot_us > 1000000UL || duration_ms == 0u || target_flags == 0u) {
        return -1;
    }
    if (duration_ms > 60000UL) {
        duration_ms = 60000UL;
    }
    if (amplitude_permille > 1000u) {
        amplitude_permille = 1000u;
    }

    cycle_us = (uint64_t)slot_us * 32ULL;
    electrical_hz_x1000 = cycle_us > 0u ? 1000000000ULL / cycle_us : 0ULL;

    motor_drv8837_off();
    board_init_safe_pins();
    sine_diag_all_inputs_low();
    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);

    start_ms = timebase_ms();
    deadline_ms = start_ms + duration_ms;

    LOG_INFO("motor sine-scope-run-us slot_us=%lu amplitude=%u duration_ms=%lu electrical_cycle_us=%lu electrical_hz_x1000=%lu targets=%s%s%s sleep=%u visualized_pwm=1 continuous=1\r\n",
             (unsigned long)slot_us,
             (unsigned)amplitude_permille,
             (unsigned long)duration_ms,
             (unsigned long)cycle_us,
             (unsigned long)electrical_hz_x1000,
             (target_flags & 0x01u) ? "A" : "",
             (target_flags & 0x02u) ? "B" : "",
             (target_flags & 0x04u) ? "G" : "",
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP));
    if (slot_us < 4u) {
        LOG_WARN("motor sine-scope-run-us timing below 4us is diagnostic only; GPIO/write overhead will dominate\r\n");
    }

    while ((int32_t)(timebase_ms() - deadline_ms) < 0) {
        uint8_t phase = (uint8_t)(step_count & 31u);

        sine_scope_plot_run_slot_us(phase, amplitude_permille, slot_us, target_flags);
        ++step_count;
    }

    sine_diag_all_inputs_low();
    northpole_diag_force_gpio_output(BOARD_OUTPUT_MOTOR_SLEEP, 0u);
    LOG_INFO("motor sine-scope-run-us done steps=%lu sleep=%u A1=%u A2=%u B1=%u B2=%u G1=%u G2=%u\r\n",
             (unsigned long)step_count,
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_A1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_A2),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_B1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_B2),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_G1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_G2));
    return 0;
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

static void print_motor_pwm_debug(void)
{
    LOG_INFO("motor pwm-debug backend=%u sleep=%u A1=%u A2=%u B1=%u B2=%u G1=%u G2=%u\r\n",
             (unsigned)APP_MOTOR_PWM_BACKEND_ENABLE,
             (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_A1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_A2),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_B1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_B2),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_G1),
             (unsigned)board_output_last_state(BOARD_OUTPUT_PWM_G2));
#if APP_MOTOR_PWM_BACKEND_ENABLE
    {
        uint8_t initialized = 0u;
        uint8_t pwmx_clock_div = 0u;
        uint16_t pwmx_cycle_ticks = 0u;
        uint16_t duty_50_permille_ticks = 0u;
        uint32_t pwm_hz = 0u;
        uint32_t timer_cycle_ticks = 0u;

        northpole_ch592_motor_pwm_debug(&initialized,
                                        &pwm_hz,
                                        &timer_cycle_ticks,
                                        &pwmx_cycle_ticks,
                                        &pwmx_clock_div,
                                        &duty_50_permille_ticks);
        LOG_INFO("pwm expected init=%u hz=%lu timer_cycle=%lu pwmx_div=%u pwmx_cycle=%u duty_50permille=%u\r\n",
                 (unsigned)initialized,
                 (unsigned long)pwm_hz,
                 (unsigned long)timer_cycle_ticks,
                 (unsigned)pwmx_clock_div,
                 (unsigned)pwmx_cycle_ticks,
                 (unsigned)duty_50_permille_ticks);
    }
    LOG_INFO("pwmx regs clk_div=0x%02x out_en=0x%02x polar=0x%02x config=0x%02x cycle=0x%08lx pwm4=0x%04x pwm5=0x%04x pwm7=0x%04x pwm9=0x%04x\r\n",
             (unsigned)R8_PWM_CLOCK_DIV,
             (unsigned)R8_PWM_OUT_EN,
             (unsigned)R8_PWM_POLAR,
             (unsigned)R8_PWM_CONFIG,
             (unsigned long)R32_PWM_REG_CYCLE,
             (unsigned)R16_PWM4_DATA,
             (unsigned)R16_PWM5_DATA,
             (unsigned)R16_PWM7_DATA,
             (unsigned)R16_PWM9_DATA);
    LOG_INFO("tmr1 regs ctrl=0x%02x pwm_mod=0x%02x fifo_count=%u count=0x%08lx end=0x%08lx fifo=0x%08lx\r\n",
             (unsigned)R8_TMR1_CTRL_MOD,
             (unsigned)R8_TMR1_PWM_MOD,
             (unsigned)R8_TMR1_FIFO_COUNT,
             (unsigned long)R32_TMR1_COUNT,
             (unsigned long)R32_TMR1_CNT_END,
             (unsigned long)R32_TMR1_FIFO);
    LOG_INFO("tmr2 regs ctrl=0x%02x pwm_mod=0x%02x fifo_count=%u count=0x%08lx end=0x%08lx fifo=0x%08lx\r\n",
             (unsigned)R8_TMR2_CTRL_MOD,
             (unsigned)R8_TMR2_PWM_MOD,
             (unsigned)R8_TMR2_FIFO_COUNT,
             (unsigned long)R32_TMR2_COUNT,
             (unsigned long)R32_TMR2_CNT_END,
             (unsigned long)R32_TMR2_FIFO);
#else
    LOG_INFO("pwm regs unavailable: APP_MOTOR_PWM_BACKEND_ENABLE=0\r\n");
#endif
}

static int cmd_motor(int argc, char **argv)
{
    if (argc < 2) {
        LOG_INFO("motor status|arm <seconds>|off|pwm-debug|pwm <A|B|G> <forward|reverse|coast|brake> <duty_permille> <ms>|sine-demo <speed_hz> <amplitude_permille> <ms> [AB|A|B|G|all]|sine-phase <phase_0_31> <amplitude_permille> <ms> [AB|A|B|G|all]|sine-diag-inputs <speed_hz> <ms> [AB|A|B|G|all]|sine-pwm-inputs <speed_hz> <amplitude_permille> <ms> [AB|A|B|G|all]|sine-scope-plot <speed_hz> <amplitude_permille> <ms> [AB|A|B|G|all]|sine-scope-plot-us <slot_us> <amplitude_permille> <steps> [AB|A|B|G|all]|sine-scope-run-us <slot_us> <amplitude_permille> <ms> [AB|A|B|G|all]|sine-scope-clkdiv <fast_clkdiv> <amplitude_permille> <ms> [AB|A|B|G|all]|diag-inputs <A|B|G> <forward|reverse|coast|brake> <ms>\r\n");
        return 0;
    }
    if (strcmp(argv[1], "status") == 0) {
        print_motor_status();
        return 0;
    }
    if (strcmp(argv[1], "pwm-debug") == 0) {
        print_motor_pwm_debug();
        return 0;
    }
#if APP_DEV_BOARD_BRINGUP_APP_SMOKE
    LOG_WARN("motor commands disabled in dev-board smoke build\r\n");
    return -1;
#endif
#if !APP_TARGET_ENABLE_MOTOR
    LOG_WARN("motor commands disabled in this target ladder build\r\n");
    return -1;
#endif
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
    if (strcmp(argv[1], "diag-inputs") == 0 && argc >= 5) {
        motor_driver_id_t driver;
        motor_drv8837_mode_t mode;
        board_output_id_t in1_pin;
        board_output_id_t in2_pin;
        uint8_t in1;
        uint8_t in2;
        uint32_t duration_ms = (uint32_t)strtoul(argv[4], NULL, 0);

        if (parse_driver(argv[2], &driver) < 0 || parse_motor_mode(argv[3], &mode) < 0) {
            LOG_WARN("bad motor diag-inputs command\r\n");
            return -1;
        }

        if (duration_ms > 10000UL) {
            duration_ms = 10000UL;
        }

        in1_pin = motor_diag_in1_pin(driver);
        in2_pin = motor_diag_in2_pin(driver);
        motor_diag_mode_levels(mode, &in1, &in2);

        motor_drv8837_off();
        board_init_safe_pins();
        board_output_write(BOARD_OUTPUT_MOTOR_SLEEP, 1);
        board_output_write(in1_pin, in1);
        board_output_write(in2_pin, in2);

        LOG_INFO("motor diag-inputs %s mode=%s in1=%u in2=%u sleep=%u hold_ms=%lu\r\n",
                 motor_drv8837_driver_name(driver),
                 motor_drv8837_mode_name(mode),
                 (unsigned)in1,
                 (unsigned)in2,
                 (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP),
                 (unsigned long)duration_ms);
        LOG_INFO("motor diag-readback %s in1=%u in2=%u sleep=%u\r\n",
                 motor_drv8837_driver_name(driver),
                 (unsigned)board_output_read(in1_pin),
                 (unsigned)board_output_read(in2_pin),
                 (unsigned)board_output_read(BOARD_OUTPUT_MOTOR_SLEEP));

        timebase_delay_ms(duration_ms);

        board_output_write(in1_pin, 0);
        board_output_write(in2_pin, 0);
        board_output_write(BOARD_OUTPUT_MOTOR_SLEEP, 0);

        LOG_INFO("motor diag-inputs done %s in1=0 in2=0 sleep=%u\r\n",
                 motor_drv8837_driver_name(driver),
                 (unsigned)board_output_last_state(BOARD_OUTPUT_MOTOR_SLEEP));
        return 0;
    }
    if (strcmp(argv[1], "sine-demo") == 0 && argc >= 5) {
        uint32_t speed_hz = (uint32_t)strtoul(argv[2], NULL, 0);
        uint16_t amplitude = (uint16_t)strtoul(argv[3], NULL, 0);
        uint32_t duration_ms = (uint32_t)strtoul(argv[4], NULL, 0);
        uint8_t target_flags = sine_demo_target_flags(argc >= 6 ? argv[5] : NULL);

        if (target_flags == 0u) {
            LOG_WARN("bad motor sine-demo target\r\n");
            return -1;
        }
        return run_motor_sine_demo(speed_hz, amplitude, duration_ms, target_flags);
    }
    if (strcmp(argv[1], "sine-phase") == 0 && argc >= 5) {
        uint8_t phase = (uint8_t)strtoul(argv[2], NULL, 0);
        uint16_t amplitude = (uint16_t)strtoul(argv[3], NULL, 0);
        uint32_t duration_ms = (uint32_t)strtoul(argv[4], NULL, 0);
        uint8_t target_flags = sine_demo_target_flags(argc >= 6 ? argv[5] : NULL);

        if (target_flags == 0u) {
            LOG_WARN("bad motor sine-phase target\r\n");
            return -1;
        }
        return run_motor_sine_phase(phase, amplitude, duration_ms, target_flags);
    }
    if (strcmp(argv[1], "sine-diag-inputs") == 0 && argc >= 4) {
        uint32_t speed_hz = (uint32_t)strtoul(argv[2], NULL, 0);
        uint32_t duration_ms = (uint32_t)strtoul(argv[3], NULL, 0);
        uint8_t target_flags = sine_demo_target_flags(argc >= 5 ? argv[4] : NULL);

        if (target_flags == 0u) {
            LOG_WARN("bad motor sine-diag-inputs target\r\n");
            return -1;
        }
        return run_motor_sine_diag_inputs(speed_hz, duration_ms, target_flags);
    }
    if (strcmp(argv[1], "sine-pwm-inputs") == 0 && argc >= 5) {
        uint32_t speed_hz = (uint32_t)strtoul(argv[2], NULL, 0);
        uint16_t amplitude = (uint16_t)strtoul(argv[3], NULL, 0);
        uint32_t duration_ms = (uint32_t)strtoul(argv[4], NULL, 0);
        uint8_t target_flags = sine_demo_target_flags(argc >= 6 ? argv[5] : NULL);

        if (target_flags == 0u) {
            LOG_WARN("bad motor sine-pwm-inputs target\r\n");
            return -1;
        }
        return run_motor_sine_pwm_inputs(speed_hz, amplitude, duration_ms, target_flags);
    }
    if (strcmp(argv[1], "sine-scope-plot") == 0 && argc >= 5) {
        uint32_t speed_hz = (uint32_t)strtoul(argv[2], NULL, 0);
        uint16_t amplitude = (uint16_t)strtoul(argv[3], NULL, 0);
        uint32_t duration_ms = (uint32_t)strtoul(argv[4], NULL, 0);
        uint8_t target_flags = sine_demo_target_flags(argc >= 6 ? argv[5] : NULL);

        if (target_flags == 0u) {
            LOG_WARN("bad motor sine-scope-plot target\r\n");
            return -1;
        }
        return run_motor_sine_scope_plot(speed_hz, amplitude, duration_ms, target_flags);
    }
    if (strcmp(argv[1], "sine-scope-plot-us") == 0 && argc >= 5) {
        uint32_t slot_us = (uint32_t)strtoul(argv[2], NULL, 0);
        uint16_t amplitude = (uint16_t)strtoul(argv[3], NULL, 0);
        uint32_t steps = (uint32_t)strtoul(argv[4], NULL, 0);
        uint8_t target_flags = sine_demo_target_flags(argc >= 6 ? argv[5] : NULL);

        if (target_flags == 0u) {
            LOG_WARN("bad motor sine-scope-plot-us target\r\n");
            return -1;
        }
        return run_motor_sine_scope_plot_us(slot_us, amplitude, steps, target_flags);
    }
    if (strcmp(argv[1], "sine-scope-run-us") == 0 && argc >= 5) {
        uint32_t slot_us = (uint32_t)strtoul(argv[2], NULL, 0);
        uint16_t amplitude = (uint16_t)strtoul(argv[3], NULL, 0);
        uint32_t duration_ms = (uint32_t)strtoul(argv[4], NULL, 0);
        uint8_t target_flags = sine_demo_target_flags(argc >= 6 ? argv[5] : NULL);

        if (target_flags == 0u) {
            LOG_WARN("bad motor sine-scope-run-us target\r\n");
            return -1;
        }
        return run_motor_sine_scope_run_us(slot_us, amplitude, duration_ms, target_flags);
    }
    if (strcmp(argv[1], "sine-scope-clkdiv") == 0 && argc >= 5) {
        uint32_t fast_clkdiv = (uint32_t)strtoul(argv[2], NULL, 0);
        uint16_t amplitude = (uint16_t)strtoul(argv[3], NULL, 0);
        uint32_t duration_ms = (uint32_t)strtoul(argv[4], NULL, 0);
        uint8_t target_flags = sine_demo_target_flags(argc >= 6 ? argv[5] : NULL);

        if (target_flags == 0u) {
            LOG_WARN("bad motor sine-scope-clkdiv target\r\n");
            return -1;
        }
        LOG_INFO("motor sine-scope-clkdiv fast_clkdiv=%lu maps_to_slot_us=%lu\r\n",
                 (unsigned long)fast_clkdiv,
                 (unsigned long)fast_clkdiv);
        return run_motor_sine_scope_run_us(fast_clkdiv, amplitude, duration_ms, target_flags);
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
        LOG_INFO("rgb off|idle-low|one <idx> <r> <g> <b>|all <r> <g> <b>|chase <brightness>|order test|show\r\n");
        return 0;
    }
#if APP_DEV_BOARD_BRINGUP_APP_SMOKE
    LOG_WARN("rgb commands disabled in dev-board smoke build\r\n");
    return -1;
#endif
#if !APP_TARGET_ENABLE_RGB
    LOG_WARN("rgb commands disabled in this target ladder build\r\n");
    return -1;
#endif
    if (strcmp(argv[1], "off") == 0) {
        rgb_ws2812_clear();
        LOG_INFO("rgb off frame sent\r\n");
        return 0;
    }
    if (strcmp(argv[1], "idle-low") == 0) {
        rgb_ws2812_force_idle_low();
        LOG_INFO("rgb data held low; no WS2812 frame sent\r\n");
        return 0;
    }
    if (strcmp(argv[1], "show") == 0) {
        rgb_ws2812_show();
        LOG_INFO("rgb frame resent\r\n");
        return 0;
    }
    if (strcmp(argv[1], "one") == 0 && argc >= 6) {
        rgb_color_t color;
        color.r = (uint8_t)strtoul(argv[3], NULL, 0);
        color.g = (uint8_t)strtoul(argv[4], NULL, 0);
        color.b = (uint8_t)strtoul(argv[5], NULL, 0);
        rgb_ws2812_set((uint8_t)strtoul(argv[2], NULL, 0), color);
        rgb_ws2812_show();
        LOG_INFO("rgb one sent\r\n");
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
        LOG_INFO("rgb all sent\r\n");
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
#if APP_DEV_BOARD_BRINGUP_APP_SMOKE
    LOG_WARN("hall input reads disabled in dev-board smoke build\r\n");
    return -1;
#endif
#if !APP_TARGET_ENABLE_HALL
    LOG_WARN("hall input reads disabled in this target ladder build\r\n");
    return -1;
#endif
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
#if APP_DEV_BOARD_BRINGUP_APP_SMOKE
    LOG_WARN("touch reads disabled in dev-board smoke build\r\n");
    return -1;
#endif
#if !APP_TARGET_ENABLE_TOUCH
    LOG_WARN("touch reads disabled in this target ladder build\r\n");
    return -1;
#endif
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
#if APP_DEV_BOARD_BRINGUP_APP_SMOKE
    (void)argc;
    (void)argv;
    LOG_WARN("i2c commands disabled in dev-board smoke build\r\n");
    return -1;
#endif
#if !APP_TARGET_ENABLE_POWER_IP5209
    (void)argc;
    (void)argv;
    LOG_WARN("i2c commands disabled in this target ladder build\r\n");
    return -1;
#endif
    if (argc >= 2 && strcmp(argv[1], "lines") == 0) {
        i2c_bus_status_t status = i2c_bus_status();
        i2c_bus_debug_t debug;
        int rc = i2c_bus_debug_snapshot(&debug);
        if (rc < 0) {
            LOG_INFO("i2c lines rc=%d initialized=%u bus_hz=%lu last_error=%u\r\n",
                     rc,
                     (unsigned)status.initialized,
                     (unsigned long)status.bus_hz,
                     (unsigned)status.last_error);
            return rc;
        }
        status = i2c_bus_status();
        LOG_INFO("i2c lines scl=%u sda=%u initialized=%u bus_hz=%lu last_error=%u\r\n",
                 (unsigned)debug.scl_level,
                 (unsigned)debug.sda_level,
                 (unsigned)status.initialized,
                 (unsigned long)status.bus_hz,
                 (unsigned)status.last_error);
        LOG_INFO("i2c regs ctrl1=0x%04x ctrl2=0x%04x star1=0x%04x star2=0x%04x ckcfgr=0x%04x\r\n",
                 (unsigned)debug.ctrl1,
                 (unsigned)debug.ctrl2,
                 (unsigned)debug.star1,
                 (unsigned)debug.star2,
                 (unsigned)debug.ckcfgr);
        LOG_INFO("i2c pins alt=0x%04x config2=0x%08lx\r\n",
                 (unsigned)debug.pin_alternate,
                 (unsigned long)debug.pin_config2);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "release-debug") == 0) {
        int rc = i2c_bus_release_debug_pins();
        i2c_bus_debug_t debug;
        LOG_INFO("i2c release-debug rc=%d; WCH-Link attach may require reset/download mode\r\n", rc);
        if (i2c_bus_debug_snapshot(&debug) == 0) {
            LOG_INFO("i2c lines scl=%u sda=%u alt=0x%04x config2=0x%08lx\r\n",
                     (unsigned)debug.scl_level,
                     (unsigned)debug.sda_level,
                     (unsigned)debug.pin_alternate,
                     (unsigned long)debug.pin_config2);
        }
        return rc;
    }
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
#if APP_DEV_BOARD_BRINGUP_APP_SMOKE
    (void)argc;
    (void)argv;
    LOG_WARN("ip5209 commands disabled in dev-board smoke build\r\n");
    return -1;
#endif
#if !APP_TARGET_ENABLE_POWER_IP5209
    (void)argc;
    (void)argv;
    LOG_WARN("ip5209 commands disabled in this target ladder build\r\n");
    return -1;
#endif
    uint8_t value = 0;
    int rc;

    if (argc < 2 || strcmp(argv[1], "status") == 0) {
        power_ip5209_status_t status = power_ip5209_status();
        power_ip5209_diagnostics_t diag;
        int diag_rc;
        LOG_INFO("ip5209 addr=0x%02x present=%u int=%u charging=%u boost_cfg=%u\r\n",
                 (unsigned)APP_IP5209_I2C_ADDR,
                 (unsigned)status.i2c_present,
                 (unsigned)status.int_level,
                 (unsigned)status.charging,
                 (unsigned)status.boost_enabled);
        diag_rc = power_ip5209_read_diagnostics(&diag);
        if (diag_rc == 0) {
            LOG_INFO("ip5209 cfg sys01=0x%02x charger=%u boost=%u wled=%u wled_detect=%u\r\n",
                     (unsigned)diag.sys_ctl0,
                     (unsigned)diag.charger_enable_config,
                     (unsigned)diag.boost_enable_config,
                     (unsigned)diag.wled_enable_config,
                     (unsigned)diag.wled_detect_config);
            LOG_INFO("ip5209 cfg sys02=0x%02x load_powerup=%u light_load_shutdown=%u sys04=0x%02x vin_pullout_boost=%u\r\n",
                     (unsigned)diag.sys_ctl1,
                     (unsigned)diag.load_powerup_enable,
                     (unsigned)diag.light_load_shutdown_enable,
                     (unsigned)diag.sys_ctl4,
                     (unsigned)diag.vin_pullout_boost_enable);
            LOG_INFO("ip5209 cfg sys07=0x%02x ntc_disabled=%u wled_key_double=%u shutdown_key_long=%u sys0c_reserved_low=0x%02x\r\n",
                     (unsigned)diag.sys_ctl5,
                     (unsigned)diag.ntc_disabled,
                     (unsigned)diag.wled_double_press_mode,
                     (unsigned)diag.shutdown_long_press_mode,
                     (unsigned)(diag.sys_ctl2 & 0x07u));
            LOG_INFO("ip5209 note boost_cfg is register 0x01 bit2, not measured VOUT\r\n");
            LOG_INFO("ip5209 run `ip5209 dump` for all raw registers and bit decode\r\n");
            LOG_INFO("ip5209 charge_state=%u/%s done=%u light_load=%u vin_ov=%u key=%u long=%u short=%u\r\n",
                     (unsigned)diag.charge_state,
                     power_ip5209_charge_state_name(diag.charge_state),
                     (unsigned)diag.charge_done,
                     (unsigned)diag.light_load,
                     (unsigned)diag.vin_overvoltage,
                     (unsigned)diag.key_pressed,
                     (unsigned)diag.key_long,
                     (unsigned)diag.key_short);
            LOG_INFO("ip5209 battery_mv=%ld battery_current_ma=%ld ocv_mv=%ld adc_valid=%u/%u/%u\r\n",
                     (long)diag.battery_mv,
                     (long)diag.battery_current_ma,
                     (long)diag.battery_ocv_mv,
                     (unsigned)diag.battery_mv_valid,
                     (unsigned)diag.battery_current_ma_valid,
                     (unsigned)diag.battery_ocv_mv_valid);
        } else {
            LOG_INFO("ip5209 diagnostics rc=%d\r\n", diag_rc);
        }
        return 0;
    }
    if (strcmp(argv[1], "dump") == 0) {
        power_ip5209_diagnostics_t diag;
        power_ip5209_status_t status;
        rc = power_ip5209_read_diagnostics(&diag);
        if (rc) {
            LOG_INFO("ip5209 dump rc=%d\r\n", rc);
            return rc;
        }
        status = power_ip5209_status();
        log_ip5209_verbose_dump(&status, &diag);
        return 0;
    }
    if (strcmp(argv[1], "probe") == 0) {
        rc = power_ip5209_probe();
        LOG_INFO("ip5209 probe addr=0x%02x rc=%d\r\n", (unsigned)APP_IP5209_I2C_ADDR, rc);
        return rc;
    }
    if (strcmp(argv[1], "boost") == 0 && argc >= 3) {
        if (strcmp(argv[2], "on") == 0) {
            return ip5209_rmw_bit("boost on", "SYS_CTL0.boost_en", 0x01u, 0x04u, 1u);
        }
        if (strcmp(argv[2], "off") == 0) {
            return ip5209_rmw_bit("boost off", "SYS_CTL0.boost_en", 0x01u, 0x04u, 0u);
        }
        LOG_INFO("ip5209 boost <on|off>\r\n");
        return -1;
    }
    if (strcmp(argv[1], "light-load") == 0 && argc >= 3) {
        if (strcmp(argv[2], "enable") == 0) {
            return ip5209_rmw_bit("light-load enable", "SYS_CTL1.light_load_shutdown_en", 0x02u, 0x02u, 1u);
        }
        if (strcmp(argv[2], "disable") == 0) {
            return ip5209_rmw_bit("light-load disable", "SYS_CTL1.light_load_shutdown_en", 0x02u, 0x02u, 0u);
        }
        LOG_INFO("ip5209 light-load <enable|disable>\r\n");
        return -1;
    }
    if (strcmp(argv[1], "ntc") == 0 && argc >= 3) {
        if (strcmp(argv[2], "enable") == 0) {
            return ip5209_rmw_bit("ntc enable", "SYS_CTL5.ntc_disabled", 0x07u, 0x40u, 0u);
        }
        if (strcmp(argv[2], "disable") == 0) {
            return ip5209_rmw_bit("ntc disable", "SYS_CTL5.ntc_disabled", 0x07u, 0x40u, 1u);
        }
        LOG_INFO("ip5209 ntc <enable|disable>\r\n");
        return -1;
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
    LOG_INFO("ip5209 status|dump|probe|read <reg>|write <reg> <value>|boost <on|off>|light-load <enable|disable>|ntc <enable|disable>\r\n");
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

#if APP_DEV_BOARD_BRINGUP_APP_SMOKE
    LOG_INFO("software reset requested; dev-board smoke build skips target safe-pin writes\r\n");
    SYS_ResetExecute();
    for (;;) {
    }
#else
#if APP_TARGET_ENABLE_MOTOR
    motor_drv8837_off();
#endif
#if APP_TARGET_ENABLE_RGB
    rgb_ws2812_clear();
#endif
#if APP_TARGET_ENABLE_AUDIO
    (void)audio_wt2003_stop();
#endif
#if APP_TARGET_ENABLE_SAFE_PINS
    board_init_safe_pins();
#endif
    LOG_INFO("software reset requested; safe pins forced\r\n");
    SYS_ResetExecute();
    for (;;) {
    }
#endif
}
