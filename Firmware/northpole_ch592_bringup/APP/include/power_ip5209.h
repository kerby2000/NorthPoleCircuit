#ifndef POWER_IP5209_H
#define POWER_IP5209_H

#include <stdint.h>

typedef struct {
    uint8_t i2c_present;
    uint8_t int_level;
    uint8_t charging;
    uint8_t boost_enabled;
} power_ip5209_status_t;

typedef struct {
    uint8_t valid;
    uint8_t reg00;
    uint8_t sys_ctl0;
    uint8_t sys_ctl1;
    uint8_t sys_ctl2;
    uint8_t sys_ctl3;
    uint8_t sys_ctl4;
    uint8_t sys_ctl5;
    uint8_t charger_ctl1;
    uint8_t charger_ctl2;
    uint8_t charge_current_ctl;
    uint8_t chg_dig_ctl4;
    uint8_t mfp_ctl0;
    uint8_t mfp_ctl1;
    uint8_t gpio_inen;
    uint8_t gpio_outen;
    uint8_t gpio_data;
    uint8_t read70;
    uint8_t read0;
    uint8_t read1;
    uint8_t read2;
    uint8_t batvadc_l;
    uint8_t batvadc_h;
    uint8_t batiadc_l;
    uint8_t batiadc_h;
    uint8_t batocv_l;
    uint8_t batocv_h;
    uint8_t charger_enable_config;
    uint8_t boost_enable_config;
    uint8_t wled_enable_config;
    uint8_t wled_detect_config;
    uint8_t load_powerup_enable;
    uint8_t light_load_shutdown_enable;
    uint8_t light_load_shutdown_threshold_n;
    uint16_t light_load_shutdown_threshold_ma;
    uint8_t long_press_time_sel;
    uint8_t shutdown_time_sel;
    uint8_t button_shutdown_enable;
    uint8_t vin_pullout_boost_enable;
    uint8_t ntc_disabled;
    uint8_t wled_double_press_mode;
    uint8_t shutdown_long_press_mode;
    uint8_t charge_uv_loop_sel;
    uint8_t battery_type_sel;
    uint8_t charge_voltage_offset_sel;
    uint8_t battery_type_from_vset_pin;
    uint8_t charge_current_setting;
    uint16_t charge_current_setting_ma;
    uint8_t mfp_light_sel;
    uint8_t mfp_l4_sel;
    uint8_t mfp_l3_sel;
    uint8_t mfp_vset_sel;
    uint8_t mfp_rset_sel;
    uint8_t charge_state;
    uint8_t chgop;
    uint8_t charge_done;
    uint8_t charge_cv_timeout;
    uint8_t charge_timeout;
    uint8_t charge_trickle_timeout;
    uint8_t light_led_present;
    uint8_t light_load;
    uint8_t vin_overvoltage;
    uint8_t key_pressed;
    uint8_t key_long;
    uint8_t key_short;
    uint8_t battery_mv_valid;
    uint8_t battery_current_ma_valid;
    uint8_t battery_ocv_mv_valid;
    int32_t battery_mv;
    int32_t battery_current_ma;
    int32_t battery_ocv_mv;
} power_ip5209_diagnostics_t;

void power_ip5209_init(void);
power_ip5209_status_t power_ip5209_status(void);
int power_ip5209_probe(void);
int power_ip5209_read_diagnostics(power_ip5209_diagnostics_t *diag);
int power_ip5209_read_register(uint8_t reg, uint8_t *value);
int power_ip5209_write_register(uint8_t reg, uint8_t value);
const char *power_ip5209_charge_state_name(uint8_t state);

#endif /* POWER_IP5209_H */
