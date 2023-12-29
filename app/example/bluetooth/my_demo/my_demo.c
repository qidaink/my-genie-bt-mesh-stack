#include <stdio.h>
#include <aos/aos.h>

int application_start(int argc, char **argv)
{
    printf("++++++++++ BUILD_TIME:%s ++++++++++\r\n", __DATE__","__TIME__);
    while(1)
    {
        aos_msleep(1000);
    }
    return 0;
}

