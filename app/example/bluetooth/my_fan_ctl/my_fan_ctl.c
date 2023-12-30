/* helloworld.c - helloworld */

/*
 * Copyright (C) 2018-2020 Alibaba Group Holding Limited
 */

#define BT_DBG_ENABLED 1
#include "common/log.h"


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <aos/aos.h>
#include <aos/kernel.h>

#include <misc/printk.h>

#include <hal/soc/gpio.h>
#include <hal/soc/pwm.h>


#include "drivers/8258/gpio_8258.h"
#include "vendor/common/alios_app_config.h"

#define LED_PIN             TC825X_GET_PIN_NUM(GPIO_PB4)

#define INA_PIN             TC825X_GET_PIN_NUM(GPIO_PD2)
#define INB_PIN             TC825X_GET_PIN_NUM(GPIO_PD3)

#define KEY_SW1             TC825X_GET_PIN_NUM(GPIO_PC3)
#define KEY_SW2             TC825X_GET_PIN_NUM(GPIO_PC4)

#define DEFAULT_CYCLE       0.0
#define PWM_FREQ            250000

typedef enum {
    FOREWARD,   // 正转
    REVERSAL,   // 翻转
    STOP        // 停止
}fan_status;    // 风扇状态

static fan_status current_status = FOREWARD;	//初始化为正转
static float current_speed = DEFAULT_CYCLE;		//占空比默认为0

static gpio_dev_t light_led;
static pwm_dev_t ina,inb;
static gpio_dev_t key_sw1,key_sw2;

static uint8_t fan_ctrl(fan_status status, float speed)
{
    //要判断风扇的速度是不是在正确范围
    if(speed < 0 || speed > 1)
    {
        printf("speed error\r\n");
        return 1;
    }

    //判断风扇方向
    switch(status)
    {
        case FOREWARD:      //比如说正转是
            ina.config.duty_cycle = 0;  
            inb.config.duty_cycle = speed;      //占空比  参数在0~1之间
            //int32_t hal_pwm_para_chg(pwm_dev_t *pwm, pwm_config_t para)
            hal_pwm_para_chg(&ina,ina.config);
            hal_pwm_para_chg(&inb,inb.config);
            printf("风扇正转\r\n");
            break;
        case REVERSAL:
            ina.config.duty_cycle = speed;  
            inb.config.duty_cycle = 0;      //占空比  参数在0~1之间
            //int32_t hal_pwm_para_chg(pwm_dev_t *pwm, pwm_config_t para)
            hal_pwm_para_chg(&ina,ina.config);
            hal_pwm_para_chg(&inb,inb.config);
            printf("风扇反转\r\n");
            break;
        case STOP:
            ina.config.duty_cycle = 0;  
            inb.config.duty_cycle = 0;      //占空比  参数在0~1之间
            //int32_t hal_pwm_para_chg(pwm_dev_t *pwm, pwm_config_t para)
            hal_pwm_para_chg(&ina,ina.config);
            hal_pwm_para_chg(&inb,inb.config);
            printf("风扇停止\r\n");
            break;    

        default: 
            printf("状态出错\r\n");
            return 1;
    }
    return 0;
}

static u8_t fan_init(void)
{
    light_led.port = LED_PIN; /* gpio port config */
    light_led.config = OUTPUT_PUSH_PULL;/* set as output mode */
    hal_gpio_init(&light_led); /* configure GPIO with the given settings */

    ina.port = INA_PIN;                         //PC2
    ina.config.duty_cycle = DEFAULT_CYCLE;    //0.0   占空比 0%
    ina.config.freq = PWM_FREQ;
    hal_pwm_init(&ina);
    hal_pwm_start(&ina);

    inb.port = INB_PIN;                         //PC3
    inb.config.duty_cycle = DEFAULT_CYCLE;    //0.0   占空比 0%
    inb.config.freq = PWM_FREQ;
    hal_pwm_init(&inb);
    hal_pwm_start(&inb);
    printf("风扇初始化完成\r\n");
    return 0;
}

void key_irq_handler(void *arg)
{
    gpio_dev_t *tmp = (gpio_dev_t *)arg; 
    switch(tmp->port)
    {
        case KEY_SW1:       // 按键1是风速变大
            current_speed += 0.1;
            if(current_speed > 1)
                current_speed = 1;
            printf("风速变大\r\n");
            break;
        case KEY_SW2:       // 按键1是风速变小
            current_speed -= 0.1;
            if(current_speed < 0)
                current_speed = 0;
            printf("风速变小\r\n");
            break;    

        default:
            printf("中断不识别\r\n");
            return ;

    }
    fan_ctrl(current_status, current_speed);

    return ;
}

void key_init(void)
{
    //按键sw1初始化
    key_sw1.port = KEY_SW1;
    key_sw1.config = INPUT_PULL_UP;
    hal_gpio_init(&key_sw1);
    hal_gpio_enable_irq(&key_sw1, IRQ_TRIGGER_FALLING_EDGE, key_irq_handler,&key_sw1);

    //按键sw2初始化
    key_sw2.port = KEY_SW2;
    key_sw2.config = INPUT_PULL_UP;
    hal_gpio_init(&key_sw2);
    hal_gpio_enable_irq(&key_sw2, IRQ_TRIGGER_FALLING_EDGE, key_irq_handler,&key_sw2);
    printf("按键初始化完成\r\n");
}

int application_start(int argc, char **argv)
{
    printk("BUILD_TIME:%s\n", __DATE__","__TIME__);
    fan_init();
    key_init();
#if 0
    aos_msleep(5000);
    while(1)
    {
        printf("正转3秒\r\n");
        fan_ctrl(FOREWARD, 0.3);
        aos_msleep(3000);

        printf("暂停3秒\r\n");
        fan_ctrl(STOP, 0);
        aos_msleep(3000);

        printf("反转3秒\r\n");
        fan_ctrl(REVERSAL, 0.3);
        aos_msleep(3000);

        printf("暂停3秒\r\n");
        fan_ctrl(STOP, 0);
        aos_msleep(3000);
    }
#endif
    while(1)
    {
        hal_gpio_output_toggle(&light_led);
        aos_msleep(1000);
    }
    return 0;
}

