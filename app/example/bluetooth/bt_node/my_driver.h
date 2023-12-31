#ifndef __MY_DRIVER_H__
#define __MY_DRIVER_H__

#define GREEN_LED_PIN             TC825X_GET_PIN_NUM(GPIO_PB5)

int32_t led_init(void);
int32_t led_test(void);

#endif /* __MY_DRIVER_H__ */
