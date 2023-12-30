/** =====================================================
 * Copyright © hk. 2022-2025. All rights reserved.
 * File name  : my_driver.c
 * Author     : 上上签
 * Date       : 2023-12-29
 * Version    : 
 * Description: 
 * ======================================================
 */

#include <stdio.h>
#include <aos/aos.h>
#include <hal/soc/gpio.h>
#include <hal/soc/pwm.h>
#include "drivers/8258/gpio_8258.h"

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

static pwm_dev_t led_pwm_dev[3] = {{0}};   // 创建全局pwm配置结构体
int32_t led_pwm_init(void)
{
    int32_t ret = 0;

    pwm_dev_t  *p_pwm_dev = led_pwm_dev;
    //配置pwm
    p_pwm_dev[RED_LED].port = LED0_PIN;
    p_pwm_dev[RED_LED].config.duty_cycle = 0;
    p_pwm_dev[RED_LED].config.freq = 20000;
    ret = hal_pwm_init(&p_pwm_dev[RED_LED]);
    if (ret != 0) {
        printf("p_pwm_dev[%d] pwm init error !\n", RED_LED);
    }

    p_pwm_dev[GREEN_LED].port = LED1_PIN;
    p_pwm_dev[GREEN_LED].config.duty_cycle = 0;
    p_pwm_dev[GREEN_LED].config.freq = 20000;
    ret = hal_pwm_init(&p_pwm_dev[GREEN_LED]);
    if (ret != 0) {
        printf("p_pwm_dev[%d] pwm init error !\n", GREEN_LED);
    }

    p_pwm_dev[BLUE_LED].port = LED2_PIN;
    p_pwm_dev[BLUE_LED].config.duty_cycle = 0;
    p_pwm_dev[BLUE_LED].config.freq = 20000;
    ret = hal_pwm_init(&p_pwm_dev[BLUE_LED]);
    if (ret != 0) {
        printf("p_pwm_dev[BLUE] pwm init error !\n", BLUE_LED);
    }
    printf("++++++++++ led_pwm_init ++++++++++\r\n");

    return ret;
}

// 调节灯光亮度
int32_t set_led_off(uint16_t led)
{
    int32_t ret = 0;
    pwm_config_t new_config = {0};
    pwm_dev_t  *p_pwm_dev = led_pwm_dev;

    new_config.duty_cycle = 0.0f;
    new_config.freq = 20000;

    hal_pwm_para_chg(&p_pwm_dev[led], new_config);
    hal_pwm_stop(&p_pwm_dev[led]);

    return ret;
}

// 调节灯光亮度
int32_t set_led_on(uint16_t led)
{
    int32_t ret = 0;
    pwm_config_t new_config = {0};
    pwm_dev_t  *p_pwm_dev = led_pwm_dev;

    new_config.duty_cycle = 1.0f;
    new_config.freq = 20000;

    hal_pwm_para_chg(&p_pwm_dev[led], new_config);
    hal_pwm_start(&p_pwm_dev[led]);

    return ret;
}

// 调节灯光亮度
int32_t set_led_channel_pwm(pwm_dev_t *pwm_num, float cycle)
{
    int32_t ret = 0;
    pwm_config_t new_config = {0};

    new_config.duty_cycle = cycle;
    new_config.freq = 20000;

    hal_pwm_para_chg(pwm_num, new_config);
    hal_pwm_start(pwm_num);

    return ret;
}

static int32_t set_led_color(uint8_t color, float level)
{
    int32_t ret = 0;
    pwm_dev_t  *p_pwm_dev = led_pwm_dev;
    switch(color)
    {
        case RED:
            set_led_channel_pwm(&p_pwm_dev[RED_LED], level);
            set_led_channel_pwm(&p_pwm_dev[GREEN_LED], 0);
            set_led_channel_pwm(&p_pwm_dev[BLUE_LED], 0);
            break;
        case GREEN:
            set_led_channel_pwm(&p_pwm_dev[RED_LED], 0);
            set_led_channel_pwm(&p_pwm_dev[GREEN_LED], level);
            set_led_channel_pwm(&p_pwm_dev[BLUE_LED], 0);
            break;
        case BLUE:
            set_led_channel_pwm(&p_pwm_dev[RED_LED], 0);
            set_led_channel_pwm(&p_pwm_dev[GREEN_LED], 0);
            set_led_channel_pwm(&p_pwm_dev[BLUE_LED], level);
            break;
        case WHITE:
            set_led_channel_pwm(&p_pwm_dev[RED_LED], level);
            set_led_channel_pwm(&p_pwm_dev[GREEN_LED], level);
            set_led_channel_pwm(&p_pwm_dev[BLUE_LED], level);
            break;
        case YELLOW:
            set_led_channel_pwm(&p_pwm_dev[RED_LED], level);
            set_led_channel_pwm(&p_pwm_dev[GREEN_LED], level);
            set_led_channel_pwm(&p_pwm_dev[BLUE_LED], 0);
            break;
        case VIOLET:
            set_led_channel_pwm(&p_pwm_dev[RED_LED], level);
            set_led_channel_pwm(&p_pwm_dev[GREEN_LED], level/2);
            set_led_channel_pwm(&p_pwm_dev[BLUE_LED], level);
            break;
        default:
            set_led_channel_pwm(&p_pwm_dev[RED], level);
            set_led_channel_pwm(&p_pwm_dev[GREEN], level);
            set_led_channel_pwm(&p_pwm_dev[BLUE], level);
            break;
    }
    return ret;
}
