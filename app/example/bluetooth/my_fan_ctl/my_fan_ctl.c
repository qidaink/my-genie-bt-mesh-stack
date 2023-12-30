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

#define LED_PIN            TC825X_GET_PIN_NUM(PWM_R)

static pwm_dev_t ina,inb;
#define INA_PIN             TC825X_GET_PIN_NUM(GPIO_PD2)
#define INB_PIN             TC825X_GET_PIN_NUM(GPIO_PD3)

#define DEFAULT_CYCLE       0.0
#define PWM_FREQ            250000

typedef enum {
    FOREWARD,   // 正转
    REVERSAL,   // 翻转
    STOP        // 停止
}fan_status;    // 风扇状态

static uint8_t fan_ctrl(fan_status status, float speed)
{
    if(speed > 1 || speed < 0)
    {
        printf("speed error\r\n");
        return 1;
    }
    switch(status)
    {
        case FOREWARD: //正转
            hal_pwm_stop(&ina);
            hal_pwm_stop(&inb);
            // INA 高电平 INB 低电平
            ina.config.duty_cycle = speed;      //0.0   占空比 0%
            hal_pwm_para_chg(&ina,ina.config);
            inb.config.duty_cycle = 0;          //0.0   占空比 0%
            hal_pwm_para_chg(&inb,inb.config);
            hal_pwm_start(&ina);
            hal_pwm_start(&inb);
            break;
        case REVERSAL: //翻转
            hal_pwm_stop(&ina);
            hal_pwm_stop(&inb);
            // INA 低电平 INB 高电平
            ina.config.duty_cycle = 0;         //0.0   占空比 0%
            hal_pwm_para_chg(&ina,ina.config);
            inb.config.duty_cycle = speed; 
            hal_pwm_para_chg(&inb,inb.config);                
            hal_pwm_start(&inb);
            hal_pwm_start(&ina);    
            break;
        case STOP:
            hal_pwm_stop(&ina);
            hal_pwm_stop(&inb);
            break;
        default:
            printf("status error\r\n");
            return 1;
    }
    return 0;
}

static u8_t fan_init(void)
{
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
    printk("%s:%d\n", __func__, __LINE__);
    return 0;
}

int application_start(int argc, char **argv)
{
    printk("BUILD_TIME:%s\n", __DATE__","__TIME__);
    fan_init();
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
    return 0;
}

