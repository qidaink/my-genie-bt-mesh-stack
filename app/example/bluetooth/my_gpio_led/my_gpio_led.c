#include <stdio.h>
#include <aos/aos.h>
#include <hal/soc/gpio.h>
#include "drivers/8258/gpio_8258.h"

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

    led_dev[2].port = LED2_PIN; /* gpio port config */
    led_dev[2].config = OUTPUT_PUSH_PULL;/* set as output mode */
    ret = hal_gpio_init(&led_dev[2]); /* configure GPIO with the given settings */
    if (ret != 0) {
        printf("gpio init error !\n");
    }

    return 0;
}
int application_start(int argc, char **argv)
{
    printf("BUILD_TIME:%s\n", __DATE__","__TIME__);
    sys_led_init();
    while(1)
    {
        hal_gpio_output_toggle(&led_dev[0]);
        hal_gpio_output_toggle(&led_dev[1]);
        hal_gpio_output_toggle(&led_dev[2]);
        aos_msleep(1000);
    }
    return 0;
}

