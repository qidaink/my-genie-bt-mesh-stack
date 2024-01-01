#include <stdio.h>
#include <aos/aos.h>
#include <hal/soc/gpio.h>
#include <hal/soc/pwm.h>

#include "drivers/8258/gpio_8258.h"

#include "my_driver.h"

static FAN_DEV fan_dev = {FAN_FOREWARD, 0, 0.0f, 25000};

static gpio_dev_t light_led = {0};
static pwm_dev_t fan_ina = {0};
static pwm_dev_t fan_inb = {0};
static gpio_dev_t key_sw1 = {0};
static gpio_dev_t key_sw2 = {0};

int32_t led_init(void)
{
    int32_t ret = 0;
    light_led.port = GREEN_LED_PIN; /* gpio port config */
    light_led.config = OUTPUT_PUSH_PULL;/* set as output mode */
    ret = hal_gpio_init(&light_led); /* configure GPIO with the given settings */
    if(ret != 0)
    {
        printf("hal_gpio_init fail!ret=%d\n", ret);
        return ret;
    }
    printf("++++++++++ led init! ++++++++++\n");
    return 0;
}

int32_t led_test(void)
{
    led_init();
    while(1)
    {
        hal_gpio_output_toggle(&light_led);
        aos_msleep(1000);
    }
}

int32_t fan_init(void)
{
    int32_t ret = 0;
    fan_ina.port = FAN_INA_PIN;
    fan_ina.config.duty_cycle = fan_dev.init_cycle;     // 0.0 占空比 0%
    fan_ina.config.freq = fan_dev.freq;
    ret = hal_pwm_init(&fan_ina);
    if(ret != 0)
    {
        printf("hal_pwm_init fail!ret=%d\n", ret);
        return ret;
    }
    ret = hal_pwm_start(&fan_ina);
    if(ret != 0)
    {
        printf("hal_pwm_start fail!ret=%d\n", ret);
        return ret;
    }

    fan_inb.port = FAN_INB_PIN;
    fan_inb.config.duty_cycle = fan_dev.init_cycle;     // 0.0   占空比 0%
    fan_inb.config.freq = fan_dev.freq;
    ret =  hal_pwm_init(&fan_inb);
    if(ret != 0)
    {
        printf("hal_pwm_start fail!ret=%d\n", ret);
        return ret;
    }
    ret = hal_pwm_start(&fan_inb);
    if(ret != 0)
    {
        printf("hal_pwm_start fail!ret=%d\n", ret);
        return ret;
    }
    printf("++++++++++ fan init! ++++++++++\n");
    return ret;
}

int32_t fan_ctrl(uint8_t status, uint32_t speed)
{
    float target_speed = 0.0f;
    //要判断风扇的速度是不是在正确范围
    target_speed = (float)speed/0xffff;

    if(target_speed < 0 || target_speed > 1)
    {
        printf("speed error!speed=%d\r\n", speed);
        return -1;
    }

    //判断风扇方向
    switch(status)
    {
        case FAN_FOREWARD:      //比如说正转是
            fan_ina.config.duty_cycle = 0;  
            fan_inb.config.duty_cycle = target_speed;      //占空比  参数在0~1之间
            //int32_t hal_pwm_para_chg(pwm_dev_t *pwm, pwm_config_t para)
            hal_pwm_para_chg(&fan_ina, fan_ina.config);
            hal_pwm_para_chg(&fan_inb, fan_inb.config);
            //printf("风扇正转\r\n");
            break;
        case FAN_REVERSAL:
            fan_ina.config.duty_cycle = target_speed;  
            fan_inb.config.duty_cycle = 0;      //占空比  参数在0~1之间
            //int32_t hal_pwm_para_chg(pwm_dev_t *pwm, pwm_config_t para)
            hal_pwm_para_chg(&fan_ina,fan_ina.config);
            hal_pwm_para_chg(&fan_inb,fan_inb.config);
            //printf("风扇反转\r\n");
            break;
        case FAN_STOP:
            fan_ina.config.duty_cycle = 0;  
            fan_inb.config.duty_cycle = 0;      //占空比  参数在0~1之间
            //int32_t hal_pwm_para_chg(pwm_dev_t *pwm, pwm_config_t para)
            hal_pwm_para_chg(&fan_ina,fan_ina.config);
            hal_pwm_para_chg(&fan_inb,fan_inb.config);
            //printf("风扇停止\r\n");
            break;    
        default: 
            printf("状态出错\r\n");
            return -1;
    }
    return 0;
}

int32_t fan_test(void)
{
    fan_init();

    printf("正转5秒...\r\n");
    fan_ctrl(FAN_FOREWARD, 13107);
    aos_msleep(5000);

    printf("暂停5秒...\r\n");
    fan_ctrl(FAN_STOP, 0);
    aos_msleep(5000);

    printf("反转5秒...\r\n");
    fan_ctrl(FAN_REVERSAL, 13107);
    aos_msleep(5000);

    printf("风扇停止...\r\n");
    fan_ctrl(FAN_STOP, 0);

    return 0;
}

void key_irq_handler(void *arg)
{
    gpio_dev_t *tmp = (gpio_dev_t *)arg;
    switch (tmp->port)
    {
        case KEY_SW1: // 按键1是风速变大
            fan_dev.speed += 10000;
            if (fan_dev.speed > 60000)
            {
                fan_dev.speed = 60000;
            }
            printf("风速变大 fan_dev.speed=%d\r\n", fan_dev.speed);
            break;
        case KEY_SW2: // 按键1是风速变小
            fan_dev.speed -= 10000;
            if (fan_dev.speed < 0)
            {
                fan_dev.speed = 0;
            }
            printf("风速变小 fan_dev.speed=%d\r\n", fan_dev.speed);
            break;
        default:
            printf("中断不识别\r\n");
            break;
    }
    // 风扇控制
    fan_ctrl(fan_dev.dir, fan_dev.speed);

    return;
}

// 注意需要 开启platform/mcu/tc32_825x/vendor/common/alios_app_config.h中的GPIO_IRQ_ENABLE
// 之前已经开过了
int32_t key_init(void)
{
    int32_t ret = 0;
    //按键sw1初始化
    key_sw1.port = KEY_SW1;
    key_sw1.config = INPUT_PULL_UP;
    ret = hal_gpio_init(&key_sw1);
    if(ret != 0)
    {
        printf("hal_gpio_init fail!ret=%d\n", ret);
        return ret;
    }
    ret = hal_gpio_enable_irq(&key_sw1, IRQ_TRIGGER_FALLING_EDGE, key_irq_handler,&key_sw1);
    if(ret != 0)
    {
        printf("hal_gpio_enable_irq fail!ret=%d\n", ret);
        return ret;
    }
    //按键sw2初始化
    key_sw2.port = KEY_SW2;
    key_sw2.config = INPUT_PULL_UP;
    ret = hal_gpio_init(&key_sw2);
    if(ret != 0)
    {
        printf("hal_gpio_init fail!ret=%d\n", ret);
        return ret;
    }
    ret = hal_gpio_enable_irq(&key_sw2, IRQ_TRIGGER_FALLING_EDGE, key_irq_handler,&key_sw2);
    if(ret != 0)
    {
        printf("hal_gpio_enable_irq fail!ret=%d\n", ret);
        return ret;
    }
    printf("++++++++++ key init! ++++++++++\n");
    return ret;
}