#include <stdio.h>
#include <aos/aos.h>
#include <hal/soc/pwm.h>

static int32_t led_pwm_init(void);
static int32_t set_led_channel_pwm(pwm_dev_t *pwm_num, float cycle);
static int32_t set_led_color(uint8_t color, float level);

int application_start(int argc, char **argv)
{
    int i = 0;
    int j = 0;
    float cycle = 0.0f;
    printf("++++++++++ BUILD_TIME:%s ++++++++++\r\n", __DATE__","__TIME__);
    led_pwm_init();
    while(1)
    {
        for(j=0; j<100; j++)
        {
            cycle += 0.01f;
            set_led_color(i, cycle);
            aos_msleep(20);
        }
        
        for(j=0; j<100; j++)
        {
            cycle -= 0.01f;
            set_led_color(i, cycle);
            aos_msleep(20);
        }

        if(i++ > 5)
        {
            i = 0;
        }
        printf("++++++++++ color change! ++++++++++\r\n");
        aos_msleep(1000);
    }
    return 0;
}

#include "my_driver.c"
