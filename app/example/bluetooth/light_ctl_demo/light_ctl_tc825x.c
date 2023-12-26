#include <hal/soc/gpio.h>
#include <hal/soc/pwm.h>
#include "drivers/8258/gpio_8258.h"
#include "vendor/common/alios_app_config.h"

#ifdef CONIFG_LIGHT_HONGYAN
#define WARM_PIN            TC825X_GET_PIN_NUM(GPIO_PB0)
#define COLD_PIN            TC825X_GET_PIN_NUM(GPIO_PB1)
#else
#define WARM_PIN            TC825X_GET_PIN_NUM(PWM_R)
#define COLD_PIN            TC825X_GET_PIN_NUM(PWM_G)
#endif
#define  LIGHT_FREQ       32000

static pwm_dev_t light_led_c;
static pwm_dev_t light_led_w;

uint16_t duty_list[] = {
    #include "duty_list.h"
};

#define BLUE_LED_PWM
#define LED0_PIN TC825X_GET_PIN_NUM(GPIO_PB4) // RED！！PC1
#define LED1_PIN TC825X_GET_PIN_NUM(GPIO_PB5) // GREEN！！PC1
#define LED2_PIN TC825X_GET_PIN_NUM(GPIO_PC1) // BLUE！！PC1

static gpio_dev_t led_dev[3] = {0};
#ifdef BLUE_LED_PWM
static pwm_dev_t gpio_pwm = {0};
#endif
static void _led_init(void)
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
    return;
}

void _pwm_init(void)
{
    int32_t ret = 0;
#ifdef BLUE_LED_PWM
    gpio_pwm.port = LED2_PIN;
    gpio_pwm.config.freq = 1000;
    gpio_pwm.config.duty_cycle = 0.00;
    gpio_pwm.priv = NULL;

    ret = hal_pwm_init(&gpio_pwm);
    if(ret){
        printf("hal_pwm_init fail,ret:%d\r\n",ret);
        return;
    }

    hal_pwm_start(&gpio_pwm);
#endif
    printf("++++++++++ pwm init! ++++++++++\n");

    return;
}
//temperature 800~20000
//ligntness 655~65535
//return duty 0-100
static void _get_led_duty(uint8_t *p_duty, uint16_t actual, uint16_t temperature)
{
    uint8_t cold = 0;
    uint8_t warm = 0;

    if(temperature > LIGHT_CTL_TEMP_MAX) {
        temperature = LIGHT_CTL_TEMP_MAX;
    }
    if(temperature < LIGHT_CTL_TEMP_MIN) {
        temperature = LIGHT_CTL_TEMP_MIN;
    }

    //0-100
    cold = (temperature - LIGHT_CTL_TEMP_MIN) * 100 / (LIGHT_CTL_TEMP_MAX - LIGHT_CTL_TEMP_MIN);
    warm = 100 - cold;

    //0-100
    p_duty[LED_COLD_CHANNEL] = (actual * cold) / 65500;
    p_duty[LED_WARM_CHANNEL] = (actual * warm) / 65500;
    if(p_duty[LED_COLD_CHANNEL] == 0 && p_duty[LED_WARM_CHANNEL] == 0) {
        if(temperature > (LIGHT_CTL_TEMP_MAX - LIGHT_CTL_TEMP_MIN)>>1) {
            p_duty[LED_COLD_CHANNEL] = 1;
        } else {
            p_duty[LED_WARM_CHANNEL] = 1;
        }
    }

    //LIGHT_DBG("%d %d [%d %d] [%d %d]", actual, temperature, warm, cold, p_duty[LED_COLD_CHANNEL], p_duty[LED_WARM_CHANNEL]);

}

static int _set_pwm_duty(uint8_t channel, uint8_t duty)
{
    int err = -1;
    pwm_config_t pwm_cfg;

    if(duty > 100) {
        LIGHT_DBG("invaild");
        return -1;
    }

    pwm_cfg.freq = LIGHT_FREQ;
    pwm_cfg.duty_cycle = (float)duty_list[duty]/duty_list[100];

    if (channel == LED_COLD_CHANNEL) {
        err = hal_pwm_para_chg(&light_led_c, pwm_cfg);
        if (err) {
            LIGHT_DBG("cold err %d", err);
            return -1;
        }

    } else if (channel == LED_WARM_CHANNEL) {
        err = hal_pwm_para_chg(&light_led_w, pwm_cfg);
        if (err) {
            LIGHT_DBG("warm err %d", err);
            return -1;
        }
    }
    return 0;
}

static void _led_set(uint8_t onoff, uint16_t actual, uint16_t temperature)
{
    // 
    if(onoff)
    {
        hal_gpio_output_high(&led_dev[0]);
    }
    else
    {
        hal_gpio_output_low(&led_dev[0]);
    }
#ifdef BLUE_LED_PWM
    // 
    pwm_config_t para = {0.000, 1000};
    para.duty_cycle = (float)actual/65536;
    hal_pwm_para_chg(&gpio_pwm, para);
#endif
    return;
}

