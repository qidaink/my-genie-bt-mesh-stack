#ifndef __MY_DRIVER_H__
#define __MY_DRIVER_H__

//查阅芯片手册可知，这三个引脚既可以当做普通GPIO，也可以当做pwm输出口
#define LED0_PIN TC825X_GET_PIN_NUM(GPIO_PB4) // RED   —— PC1
#define LED1_PIN TC825X_GET_PIN_NUM(GPIO_PB5) // GREEN —— PC1
#define LED2_PIN TC825X_GET_PIN_NUM(GPIO_PC1) // BLUE  —— PC1

//调节颜色
typedef enum __COLOR_{
    RED = 0,
    GREEN,
    BLUE,
    WHITE,
    YELLOW,
    VIOLET,
}_COLOR;

//调节颜色
typedef enum __LED_{
    RED_LED = 0,
    GREEN_LED = 1,
    BLUE_LED = 2,
}_LED;


// my_driver.c 中的函数声明
int32_t led_pwm_init(void);
int32_t set_led_channel_pwm(pwm_dev_t *pwm_num, float cycle);
int32_t set_led_color(uint8_t color, float level);
int32_t set_led_on(uint16_t led);
int32_t set_led_off(uint16_t led);

#endif /* __MY_DRIVER_H__ */
