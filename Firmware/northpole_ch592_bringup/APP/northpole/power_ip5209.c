#include "power_ip5209.h"

#include "app_config.h"
#include "board.h"
#include "i2c_bus.h"

#if defined(__GNUC__)
#define FW_WEAK __attribute__((weak))
#else
#define FW_WEAK
#endif

FW_WEAK int power_ip5209_platform_read_register(uint8_t reg, uint8_t *value)
{
    (void)reg;
    (void)value;
    return -1;
}

FW_WEAK int power_ip5209_platform_write_register(uint8_t reg, uint8_t value)
{
    (void)reg;
    (void)value;
    return -1;
}

static power_ip5209_status_t cached_status;

#define IP5209_REG_00                   0x00u
#define IP5209_REG_SYS_CTL0             0x01u
#define IP5209_REG_SYS_CTL1             0x02u
#define IP5209_REG_SYS_CTL2             0x0cu
#define IP5209_REG_SYS_CTL3             0x03u
#define IP5209_REG_SYS_CTL4             0x04u
#define IP5209_REG_SYS_CTL5             0x07u
#define IP5209_REG_CHARGER_CTL1         0x22u
#define IP5209_REG_CHARGER_CTL2         0x24u
#define IP5209_REG_CHARGE_CURRENT_CTL   0x25u
#define IP5209_REG_CHG_DIG_CTL4         0x26u
#define IP5209_REG_MFP_CTL0             0x51u
#define IP5209_REG_MFP_CTL1             0x52u
#define IP5209_REG_GPIO_INEN            0x53u
#define IP5209_REG_GPIO_OUTEN           0x54u
#define IP5209_REG_GPIO_DATA            0x55u
#define IP5209_REG_BATVADC_L            0xa2u
#define IP5209_REG_BATVADC_H            0xa3u
#define IP5209_REG_BATIADC_L            0xa4u
#define IP5209_REG_BATIADC_H            0xa5u
#define IP5209_REG_BATOCV_L             0xa8u
#define IP5209_REG_BATOCV_H             0xa9u
#define IP5209_REG_READ70               0x70u
#define IP5209_REG_READ0                0x71u
#define IP5209_REG_READ1                0x72u
#define IP5209_REG_READ2                0x77u

#define IP5209_SYS01_CHARGER_ENABLE     0x02u
#define IP5209_SYS01_BOOST_ENABLE       0x04u
#define IP5209_SYS01_WLED_ENABLE        0x08u
#define IP5209_SYS01_WLED_DETECT_ENABLE 0x10u
#define IP5209_SYS02_LOAD_POWERUP       0x01u
#define IP5209_SYS02_LIGHT_LOAD_OFF     0x02u
#define IP5209_SYS03_BUTTON_SHUTDOWN    0x20u
#define IP5209_SYS04_VIN_PULLOUT_BOOST  0x20u
#define IP5209_SYS07_SHUTDOWN_LONG_KEY  0x01u
#define IP5209_SYS07_WLED_DOUBLE_KEY    0x02u
#define IP5209_SYS07_NTC_DISABLE        0x40u
#define IP5209_CHG26_BAT_TYPE_VSET_PIN  0x40u

static int read_reg(uint8_t reg, uint8_t *value);
static int read_adc_pair(uint8_t reg_low, uint8_t reg_high, int16_t *raw);
static int16_t decode_signed_adc14(uint8_t low, uint8_t high);
static int32_t adc_to_mv(int16_t raw);
static int32_t current_adc_to_ma(int16_t raw);

void power_ip5209_init(void)
{
    i2c_bus_init(I2C_BUS_DEFAULT_HZ);
    (void)power_ip5209_probe();
    cached_status.charging = 0;
    cached_status.boost_enabled = 0;
}

int power_ip5209_probe(void)
{
    int rc = i2c_bus_probe(APP_IP5209_I2C_ADDR, I2C_BUS_DEFAULT_TIMEOUT_MS);
    cached_status.i2c_present = rc == 0 ? 1u : 0u;
    cached_status.int_level = board_input_read(BOARD_INPUT_IP5209_INT);
    return rc;
}

power_ip5209_status_t power_ip5209_status(void)
{
    power_ip5209_status_t status;
    (void)power_ip5209_probe();
    if (cached_status.i2c_present) {
        uint8_t value = 0;
        if (read_reg(IP5209_REG_SYS_CTL0, &value) == 0) {
            cached_status.boost_enabled = (value & IP5209_SYS01_BOOST_ENABLE) ? 1u : 0u;
        }
        if (read_reg(IP5209_REG_READ0, &value) == 0) {
            uint8_t charge_state = (uint8_t)((value >> 5) & 0x07u);
            cached_status.charging = (charge_state >= 1u && charge_state <= 4u) ? 1u : 0u;
        }
    }
    status = cached_status;
    status.int_level = board_input_read(BOARD_INPUT_IP5209_INT);
    return status;
}

int power_ip5209_read_diagnostics(power_ip5209_diagnostics_t *diag)
{
    int rc;
    int16_t raw;

    if (!diag) {
        return -1;
    }

    for (uint16_t i = 0; i < sizeof(*diag); ++i) {
        ((uint8_t *)diag)[i] = 0;
    }

    rc = power_ip5209_probe();
    if (rc) {
        return rc;
    }

    rc = read_reg(IP5209_REG_00, &diag->reg00);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_SYS_CTL0, &diag->sys_ctl0);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_SYS_CTL1, &diag->sys_ctl1);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_SYS_CTL2, &diag->sys_ctl2);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_SYS_CTL3, &diag->sys_ctl3);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_SYS_CTL4, &diag->sys_ctl4);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_SYS_CTL5, &diag->sys_ctl5);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_CHARGER_CTL1, &diag->charger_ctl1);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_CHARGER_CTL2, &diag->charger_ctl2);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_CHARGE_CURRENT_CTL, &diag->charge_current_ctl);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_CHG_DIG_CTL4, &diag->chg_dig_ctl4);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_MFP_CTL0, &diag->mfp_ctl0);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_MFP_CTL1, &diag->mfp_ctl1);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_GPIO_INEN, &diag->gpio_inen);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_GPIO_OUTEN, &diag->gpio_outen);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_GPIO_DATA, &diag->gpio_data);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_READ70, &diag->read70);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_READ0, &diag->read0);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_READ1, &diag->read1);
    if (rc) { return rc; }
    rc = read_reg(IP5209_REG_READ2, &diag->read2);
    if (rc) { return rc; }

    (void)read_reg(IP5209_REG_BATVADC_L, &diag->batvadc_l);
    (void)read_reg(IP5209_REG_BATVADC_H, &diag->batvadc_h);
    (void)read_reg(IP5209_REG_BATIADC_L, &diag->batiadc_l);
    (void)read_reg(IP5209_REG_BATIADC_H, &diag->batiadc_h);
    (void)read_reg(IP5209_REG_BATOCV_L, &diag->batocv_l);
    (void)read_reg(IP5209_REG_BATOCV_H, &diag->batocv_h);

    diag->charger_enable_config = (diag->sys_ctl0 & IP5209_SYS01_CHARGER_ENABLE) ? 1u : 0u;
    diag->boost_enable_config = (diag->sys_ctl0 & IP5209_SYS01_BOOST_ENABLE) ? 1u : 0u;
    diag->wled_enable_config = (diag->sys_ctl0 & IP5209_SYS01_WLED_ENABLE) ? 1u : 0u;
    diag->wled_detect_config = (diag->sys_ctl0 & IP5209_SYS01_WLED_DETECT_ENABLE) ? 1u : 0u;
    diag->load_powerup_enable = (diag->sys_ctl1 & IP5209_SYS02_LOAD_POWERUP) ? 1u : 0u;
    diag->light_load_shutdown_enable = (diag->sys_ctl1 & IP5209_SYS02_LIGHT_LOAD_OFF) ? 1u : 0u;
    diag->light_load_shutdown_threshold_n = (uint8_t)(diag->sys_ctl2 >> 3);
    diag->light_load_shutdown_threshold_ma =
        (uint16_t)diag->light_load_shutdown_threshold_n * 12u;
    diag->long_press_time_sel = (uint8_t)(diag->sys_ctl3 >> 6);
    diag->button_shutdown_enable = (diag->sys_ctl3 & IP5209_SYS03_BUTTON_SHUTDOWN) ? 1u : 0u;
    diag->shutdown_time_sel = (uint8_t)(diag->sys_ctl4 >> 6);
    diag->vin_pullout_boost_enable = (diag->sys_ctl4 & IP5209_SYS04_VIN_PULLOUT_BOOST) ? 1u : 0u;
    diag->ntc_disabled = (diag->sys_ctl5 & IP5209_SYS07_NTC_DISABLE) ? 1u : 0u;
    diag->wled_double_press_mode = (diag->sys_ctl5 & IP5209_SYS07_WLED_DOUBLE_KEY) ? 1u : 0u;
    diag->shutdown_long_press_mode = (diag->sys_ctl5 & IP5209_SYS07_SHUTDOWN_LONG_KEY) ? 1u : 0u;
    diag->charge_uv_loop_sel = (uint8_t)((diag->charger_ctl1 >> 2) & 0x03u);
    diag->battery_type_sel = (uint8_t)((diag->charger_ctl2 >> 5) & 0x03u);
    diag->charge_voltage_offset_sel = (uint8_t)((diag->charger_ctl2 >> 1) & 0x03u);
    diag->battery_type_from_vset_pin = (diag->chg_dig_ctl4 & IP5209_CHG26_BAT_TYPE_VSET_PIN) ? 1u : 0u;
    diag->charge_current_setting = (uint8_t)(diag->charge_current_ctl & 0x1fu);
    diag->charge_current_setting_ma =
        (uint16_t)(((uint32_t)diag->charge_current_setting * 100u) & 0xffffu);
    diag->mfp_light_sel = (uint8_t)((diag->mfp_ctl0 >> 4) & 0x03u);
    diag->mfp_l4_sel = (uint8_t)((diag->mfp_ctl0 >> 2) & 0x03u);
    diag->mfp_l3_sel = (uint8_t)(diag->mfp_ctl0 & 0x03u);
    diag->mfp_vset_sel = (uint8_t)((diag->mfp_ctl1 >> 2) & 0x03u);
    diag->mfp_rset_sel = (uint8_t)(diag->mfp_ctl1 & 0x03u);

    diag->charge_state = (uint8_t)((diag->read0 >> 5) & 0x07u);
    diag->chgop = (diag->read0 & 0x10u) ? 1u : 0u;
    diag->charge_done = (diag->read0 & 0x08u) ? 1u : 0u;
    diag->charge_cv_timeout = (diag->read0 & 0x04u) ? 1u : 0u;
    diag->charge_timeout = (diag->read0 & 0x02u) ? 1u : 0u;
    diag->charge_trickle_timeout = (diag->read0 & 0x01u) ? 1u : 0u;
    diag->light_led_present = (diag->read1 & 0x80u) ? 1u : 0u;
    diag->light_load = (diag->read1 & 0x40u) ? 1u : 0u;
    diag->vin_overvoltage = (diag->read1 & 0x20u) ? 1u : 0u;
    diag->key_pressed = (diag->read2 & 0x08u) ? 1u : 0u;
    diag->key_long = (diag->read2 & 0x02u) ? 1u : 0u;
    diag->key_short = (diag->read2 & 0x01u) ? 1u : 0u;

    if (read_adc_pair(IP5209_REG_BATVADC_L, IP5209_REG_BATVADC_H, &raw) == 0) {
        diag->battery_mv = adc_to_mv(raw);
        diag->battery_mv_valid = 1u;
    }
    if (read_adc_pair(IP5209_REG_BATIADC_L, IP5209_REG_BATIADC_H, &raw) == 0) {
        diag->battery_current_ma = current_adc_to_ma(raw);
        diag->battery_current_ma_valid = 1u;
    }
    if (read_adc_pair(IP5209_REG_BATOCV_L, IP5209_REG_BATOCV_H, &raw) == 0) {
        diag->battery_ocv_mv = adc_to_mv(raw);
        diag->battery_ocv_mv_valid = 1u;
    }

    diag->valid = 1u;
    return 0;
}

int power_ip5209_read_register(uint8_t reg, uint8_t *value)
{
    return power_ip5209_platform_read_register(reg, value);
}

int power_ip5209_write_register(uint8_t reg, uint8_t value)
{
    return power_ip5209_platform_write_register(reg, value);
}

const char *power_ip5209_charge_state_name(uint8_t state)
{
    switch (state) {
    case 0u: return "idle";
    case 1u: return "trickle";
    case 2u: return "constant_current";
    case 3u: return "constant_voltage";
    case 4u: return "cv_stop_check";
    case 5u: return "full";
    case 6u: return "timeout";
    default: return "reserved";
    }
}

static int read_reg(uint8_t reg, uint8_t *value)
{
    return power_ip5209_platform_read_register(reg, value);
}

static int read_adc_pair(uint8_t reg_low, uint8_t reg_high, int16_t *raw)
{
    uint8_t low;
    uint8_t high;
    int rc;

    if (!raw) {
        return -1;
    }

    rc = read_reg(reg_low, &low);
    if (rc) {
        return rc;
    }
    rc = read_reg(reg_high, &high);
    if (rc) {
        return rc;
    }

    *raw = decode_signed_adc14(low, high);
    return 0;
}

static int16_t decode_signed_adc14(uint8_t low, uint8_t high)
{
    uint16_t magnitude = (uint16_t)low | ((uint16_t)(high & 0x1fu) << 8);
    if (high & 0x20u) {
        magnitude = (uint16_t)((((uint16_t)(~low) & 0xffu) |
                                (((uint16_t)(~high) & 0x1fu) << 8)) + 1u);
        return (int16_t)-(int16_t)magnitude;
    }
    return (int16_t)magnitude;
}

static int32_t adc_to_mv(int16_t raw)
{
    return 2600 + ((int32_t)raw * 26855) / 100000;
}

static int32_t current_adc_to_ma(int16_t raw)
{
    return ((int32_t)raw * 745985) / 1000000;
}
