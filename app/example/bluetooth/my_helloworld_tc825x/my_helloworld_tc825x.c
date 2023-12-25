#include <stdio.h>
#include <aos/aos.h>

int application_start(int argc, char **argv)
{
    printf("BUILD_TIME:%s\n", __DATE__","__TIME__);

    while(1)
    {
        printf("Hello World\r\n");
        aos_msleep(1000);
    }
    return 0;
}

