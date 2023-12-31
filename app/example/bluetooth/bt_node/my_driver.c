#include <stdio.h>
#include <aos/aos.h>
#include <hal/soc/gpio.h>
#include <hal/soc/pwm.h>

#include "drivers/8258/gpio_8258.h"

#include "my_driver.h"

static gpio_dev_t light_led = {0};

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