#ifndef __MY_DRIVER_H__
#define __MY_DRIVER_H__

#include <aos/aos.h>

#define GREEN_LED_PIN  TC825X_GET_PIN_NUM(GPIO_PB5)
#define FAN_INA_PIN    TC825X_GET_PIN_NUM(GPIO_PD2)
#define FAN_INB_PIN    TC825X_GET_PIN_NUM(GPIO_PD3)
#define KEY_SW1        TC825X_GET_PIN_NUM(GPIO_PC3)
#define KEY_SW2        TC825X_GET_PIN_NUM(GPIO_PC4)
enum{
    FAN_FOREWARD = 0,   // 正转
    FAN_REVERSAL = 1,   // 翻转
    FAN_STOP     = 2,   // 停止
};                      // 风扇状态

typedef struct __FAN_DEV_ATTR_{
    uint8_t dir;
    int32_t speed;
    uint32_t init_cycle;
    uint32_t freq;
}FAN_DEV;

int32_t led_init(void);
int32_t led_test(void);

int32_t fan_init(void);
int32_t fan_ctrl(uint8_t status, uint32_t speed);
int32_t fan_test(void);

int32_t key_init(void);
#endif /* __MY_DRIVER_H__ */
