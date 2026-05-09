#ifndef MOTOR_DRV8837_H
#define MOTOR_DRV8837_H

#include <stdint.h>

typedef enum {
    MOTOR_DRV_A = 0,
    MOTOR_DRV_B,
    MOTOR_DRV_G,
    MOTOR_DRV_COUNT,
} motor_driver_id_t;

typedef enum {
    MOTOR_DRV_COAST = 0,
    MOTOR_DRV_FORWARD,
    MOTOR_DRV_REVERSE,
    MOTOR_DRV_BRAKE,
} motor_drv8837_mode_t;

typedef struct {
    motor_drv8837_mode_t mode;
    uint16_t duty_permille;
    uint32_t expires_ms;
} motor_drv8837_state_t;

void motor_drv8837_init(void);
void motor_drv8837_poll(void);
void motor_drv8837_arm(uint32_t duration_ms);
void motor_drv8837_disarm(void);
void motor_drv8837_off(void);
uint8_t motor_drv8837_is_armed(void);
uint32_t motor_drv8837_arm_remaining_ms(void);
int motor_drv8837_command(motor_driver_id_t driver, motor_drv8837_mode_t mode, uint16_t duty_permille);
int motor_drv8837_command_for(motor_driver_id_t driver,
                              motor_drv8837_mode_t mode,
                              uint16_t duty_permille,
                              uint32_t duration_ms);
void motor_drv8837_all_coast(void);
motor_drv8837_state_t motor_drv8837_get_state(motor_driver_id_t driver);
const char *motor_drv8837_driver_name(motor_driver_id_t driver);
const char *motor_drv8837_mode_name(motor_drv8837_mode_t mode);

#endif /* MOTOR_DRV8837_H */
