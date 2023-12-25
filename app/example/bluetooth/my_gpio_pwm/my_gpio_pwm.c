#include <stdio.h>
#include <aos/aos.h>
#include <hal/soc/gpio.h>
#include <hal/soc/pwm.h>
#include "drivers/8258/gpio_8258.h"

#define BLUE_LED_PWM
#define LED0_PIN TC825X_GET_PIN_NUM(GPIO_PB4) // RED——PC1
#define LED1_PIN TC825X_GET_PIN_NUM(GPIO_PB5) // GREEN——PC1
#define LED2_PIN TC825X_GET_PIN_NUM(GPIO_PC1) // BLUE——PC1

static gpio_dev_t led_dev[3] = {0};

int32_t sys_led_init(void)
{
    int32_t ret = 0;
    led_dev[0].port = LED0_PIN; /* gpio port config */
    led_dev[0].config = OUTPUT_PUSH_PULL;/* set as output mode */
    ret = hal_gpio_init(&led_dev[0]); /* configure GPIO with the given settings */
    if (ret != 0) {
        printf("gpio init error !\n");
    }

    led_dev[1].port = LED1_PIN; /* gpio port config */
    led_dev[1].config = OUTPUT_PUSH_PULL;/* set as output mode */
    ret = hal_gpio_init(&led_dev[1]); /* configure GPIO with the given settings */
    if (ret != 0) {
        printf("gpio init error !\n");
    }
#ifndef BLUE_LED_PWM
    led_dev[2].port = LED2_PIN; /* gpio port config */
    led_dev[2].config = OUTPUT_PUSH_PULL;/* set as output mode */
    ret = hal_gpio_init(&led_dev[2]); /* configure GPIO with the given settings */
    if (ret != 0) {
        printf("gpio init error !\n");
    }
#endif
    printf("++++++++++ led init! ++++++++++\n");
    return ret;
}

#ifdef BLUE_LED_PWM
void hal_pwm_app_dynamic_out(void)
{
    int cnt;
    int32_t ret;
    pwm_dev_t pwm = {0};

    printf("++++++++++ hal_pwm_app_dynamic_out start! ++++++++++\r\n");

    pwm.port = LED2_PIN;
    pwm.config.freq = 1000;
    pwm.config.duty_cycle = 0.00;
    pwm.priv = NULL;
    
    ret = hal_pwm_init(&pwm);
    if(ret){
        printf("hal_pwm_init fail,ret:%d\r\n",ret);
        return;
    }

    hal_pwm_start(&pwm);
    

    cnt = 10;
    while (cnt--) 
    {
        printf("duty_cycle count up   cnt=%d\r\n", cnt);
        pwm_config_t para = {0.000, 1000};
        for (int i = 0; i < 100; i++) {
            para.duty_cycle += 0.01;
            aos_msleep(10);
            hal_pwm_para_chg(&pwm, para);
        }

        printf("duty_cycle count down cnt=%d\r\n", cnt);

        para.duty_cycle = 1.0;
        para.freq = 1000;
        for (int i = 0; i < 100; i++) 
        {
            para.duty_cycle -= 0.01;
            aos_msleep(10);
            hal_pwm_para_chg(&pwm, para);
        }
    }

    hal_pwm_stop(&pwm);

    hal_pwm_finalize(&pwm);

    printf("++++++++++ hal_pwm_app_dynamic_out end! ++++++++++\r\n");
}
#endif

int application_start(int argc, char **argv)
{
    printf("BUILD_TIME:%s\n", __DATE__","__TIME__);
    sys_led_init();
    #ifdef BLUE_LED_PWM
    hal_pwm_app_dynamic_out();
    #endif
    while(1)
    {
        hal_gpio_output_toggle(&led_dev[0]);
        aos_msleep(1000);
    }
    return 0;
}

