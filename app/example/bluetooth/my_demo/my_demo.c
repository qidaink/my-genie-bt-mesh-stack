/* main.c - light demo */

/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include "common/log.h"


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <aos/aos.h>
#include <aos/kernel.h>

#include <misc/printk.h>
#include <misc/byteorder.h>
#include <hal/soc/gpio.h>
#include <hal/soc/pwm.h>

#include <bluetooth.h>
#include <soc.h>
#include <api/mesh.h>
#include "genie_app.h"

#include "drivers/8258/gpio_8258.h"
#include "vendor/common/alios_app_config.h"

#include "my_driver.h"

#ifndef CONFIG_INFO_DISABLE
#define LIGHT_DBG(fmt, ...)  printf("[%s]"fmt"\n", __func__, ##__VA_ARGS__)
#else
#define LIGHT_DBG(fmt, ...)
#endif

#define LIGHT_CTL_TEMP_MIN            (0x0320)    // 800
#define LIGHT_CTL_TEMP_MAX            (0x4E20)    // 20000

#define LED_FLASH_CYCLE MESH_TRNSATION_CYCLE

typedef struct{
    struct k_timer timer;
    uint16_t temp_cur;
    uint16_t temp_tar;
    uint16_t actual_start;
    uint16_t actual_cur;
    uint16_t actual_tar;
    uint32_t time_end;
} led_flash_t;

typedef struct __DEMO_DEV_{
    uint8_t led_color;
    uint16_t led_light;
} _DEMO_DEV_;

led_flash_t g_flash_para;
_DEMO_DEV_ my_dev = {0};
/* unprovision device beacon adv time */
#define MESH_PBADV_TIME 600 //unit:s

#define DEFAULT_MESH_GROUP1 0xC000
#define DEFAULT_MESH_GROUP2 0xCFFF

uint32_t get_mesh_pbadv_time(void)
{
    return MESH_PBADV_TIME*1000;    //ms
}

/* element configuration start */
#define MESH_ELEM_COUNT 1
#define MESH_ELEM_STATE_COUNT MESH_ELEM_COUNT

elem_state_t g_elem_state[MESH_ELEM_STATE_COUNT];
model_powerup_t g_powerup[MESH_ELEM_STATE_COUNT];

static struct bt_mesh_model element_models[] = {
    BT_MESH_MODEL_CFG_SRV(),
    BT_MESH_MODEL_HEALTH_SRV(),

    MESH_MODEL_GEN_ONOFF_SRV(&g_elem_state[0]), // 通用开关模型
    MESH_MODEL_LIGHTNESS_SRV(&g_elem_state[0]), // 通用亮度模型
};

static struct bt_mesh_model g_element_vendor_models[] = {
    MESH_MODEL_VENDOR_SRV(&g_elem_state[0]),
};

struct bt_mesh_elem elements[] = {
    BT_MESH_ELEM(0, element_models, g_element_vendor_models, 0),
};

uint8_t get_vendor_element_num(void)
{
    return MESH_ELEM_COUNT;
}
/* element configuration end */

void mesh_sub_init(u16_t *p_sub)
{
    memset(p_sub, 0, CONFIG_BT_MESH_MODEL_GROUP_COUNT<<1);

    p_sub[0] = DEFAULT_MESH_GROUP1;
    p_sub[1] = DEFAULT_MESH_GROUP2;
}

#ifdef CONFIG_GENIE_OTA
bool ota_check_reboot(void)
{
    // the device will reboot when it is off
    if(g_elem_state[0].state.onoff[T_CUR] == 0) {
        // save light para, always off
        g_powerup[0].last_onoff = 0;
        genie_flash_write_userdata(GFI_MESH_POWERUP, (uint8_t *)g_powerup, sizeof(g_powerup));
        BT_DBG("reboot!");
        return true;
    }
    BT_DBG("no reboot!");
    return false;
}
#endif

static void _init_light_para(void)
{
    uint8_t i = 0;
    E_GENIE_FLASH_ERRCODE ret;
    // init element state
    memset(g_elem_state, 0, sizeof(g_elem_state));
    elem_state_init(MESH_ELEM_STATE_COUNT, g_elem_state);

    // load light para
    ret = genie_flash_read_userdata(GFI_MESH_POWERUP, (uint8_t *)g_powerup, sizeof(g_powerup));
    
    if(ret == GENIE_FLASH_SUCCESS) {
        while(i < MESH_ELEM_STATE_COUNT) {
#ifdef CONFIG_GENIE_OTA
            // if the device reboot by ota, it must be off.
            if(g_powerup[0].last_onoff == 0) {
                g_elem_state[0].state.onoff[T_TAR] = 0;
                // load lightness
                if(g_powerup[0].last_actual) {
                    g_elem_state[0].state.actual[T_TAR] = g_powerup[0].last_actual;
                    g_elem_state[0].powerup.last_actual = g_powerup[0].last_actual;
                }
                // load temperature
                if(g_powerup[0].last_temp) {
                    g_elem_state[0].state.temp[T_TAR] = g_powerup[0].last_temp;
                    g_elem_state[0].powerup.last_temp = g_powerup[0].last_temp;
                }
                clear_trans_para(&g_elem_state[0]);
            } else
#endif
            {
                memcpy(&g_elem_state[0].powerup, &g_powerup[0], sizeof(model_powerup_t));
                // load lightness
                if(g_powerup[0].last_actual) {
                    g_elem_state[0].state.actual[T_TAR] = g_powerup[0].last_actual;
                }
                // load temperature
                if(g_powerup[0].last_temp) {
                    g_elem_state[0].state.temp[T_TAR] = g_powerup[0].last_temp;
                }
                LIGHT_DBG("l:%d t:%d", g_powerup[0].last_actual, g_powerup[0].last_temp);
                if(g_elem_state[0].state.onoff[T_TAR] == 1) {
                    g_elem_state[0].state.trans_start_time = k_uptime_get() + g_elem_state[0].state.delay * 5;
                    g_elem_state[0].state.trans_end_time = g_elem_state[0].state.trans_start_time + get_transition_time(g_elem_state[0].state.trans);
                }
            }
            g_elem_state[0].state.temp[T_CUR] = g_elem_state[0].state.temp[T_TAR];

            i++;
        }
    }

}

static void _reset_light_para(void)
{
    uint8_t i = 0;
    while(i < MESH_ELEM_STATE_COUNT) {
        g_elem_state[i].state.onoff[T_CUR] = GEN_ONOFF_DEFAULT;
        g_elem_state[i].state.actual[T_CUR] = LIGHTNESS_DEFAULT;
        g_elem_state[i].state.temp[T_CUR] = CTL_TEMP_DEFAULT;
        g_elem_state[i].state.onoff[T_TAR] = GEN_ONOFF_DEFAULT;
        g_elem_state[i].state.actual[T_TAR] = LIGHTNESS_DEFAULT;
        g_elem_state[i].state.temp[T_TAR] = CTL_TEMP_DEFAULT;
        g_elem_state[i].state.trans = 0;
        g_elem_state[i].state.delay = 0;
        g_elem_state[i].state.trans_start_time = 0;
        g_elem_state[i].state.trans_end_time = 0;

        g_elem_state[i].powerup.last_actual = LIGHTNESS_DEFAULT;
        g_elem_state[i].powerup.last_temp = CTL_TEMP_DEFAULT;

        g_powerup[i].last_onoff = GEN_ONOFF_DEFAULT;
        g_powerup[i].last_actual = LIGHTNESS_DEFAULT;
        g_powerup[i].last_temp = CTL_TEMP_DEFAULT;
        i++;
    }
    genie_flash_write_userdata(GFI_MESH_POWERUP, (uint8_t *)g_powerup, sizeof(g_powerup));

    LIGHT_DBG("done");
}

static void _save_light_state(elem_state_t *p_elem)
{
    uint8_t *p_read = aos_malloc(sizeof(g_powerup));

    if(p_elem->state.actual[T_CUR] != 0) {
        p_elem->powerup.last_actual = p_elem->state.actual[T_CUR];
        g_powerup[p_elem->elem_index].last_actual = p_elem->state.actual[T_CUR];
    }

    p_elem->powerup.last_temp = p_elem->state.temp[T_CUR];
    g_powerup[p_elem->elem_index].last_temp = p_elem->state.temp[T_CUR];
    // always on
    g_powerup[p_elem->elem_index].last_onoff = 1;

    genie_flash_read_userdata(GFI_MESH_POWERUP, p_read, sizeof(g_powerup));

    if(memcmp(g_powerup, p_read, sizeof(g_powerup))) {
        LIGHT_DBG("save %d %d", g_powerup[p_elem->elem_index].last_actual, g_powerup[p_elem->elem_index].last_temp);
        genie_flash_write_userdata(GFI_MESH_POWERUP, (uint8_t *)g_powerup, sizeof(g_powerup));
    }
    aos_free(p_read);
}

static void _user_init(void)
{
#ifdef CONFIG_GENIE_OTA
    // check ota flag
    if(ais_get_ota_indicat()) {
        g_indication_flag |= INDICATION_FLAG_VERSION;
    }
#endif
    led_pwm_init();
    set_led_off(RED_LED);
    set_led_off(GREEN_LED);
    set_led_off(BLUE_LED);
}

void analize_color(uint16_t light, uint16_t hue, uint16_t saturation)
{
    // 			        明度   色相   饱和度
    // data [0]  [1]  [3][2] [5][4] [7][6]
    // data 0x01 0x23 0x8000 0x0000 0xffff # 红色
    // data 0x01 0x23 0x8000 0x5555 0xffff # 绿色
    // data 0x01 0x23 0x8000 0xaaaa 0xffff # 蓝色
    // data 0x01 0x23 0xffff 0x0000 0x0000 # 白色
    // data 0x01 0x23 0x0000 0x0000 0x0000 # 黑色
    // data 0x01 0x23 0x8000 0x8000 0xffff # 青色
    // data 0x01 0x23 0x8000 0x2aaa 0xffff # 黄色
    if(light == 0x8000 && hue == 0x0000 && saturation == 0xffff)
    {
        set_led_color(RED, my_dev.led_light);
        my_dev.led_color = RED;
    }
    else if(light == 0x8000 && hue == 0x5555 && saturation == 0xffff)
    {
        set_led_color(GREEN, my_dev.led_light);
        my_dev.led_color = GREEN;
    }
    else if(light == 0x8000 && hue == 0xaaaa && saturation == 0xffff)
    {
        set_led_color(BLUE, my_dev.led_light);
        my_dev.led_color = BLUE;
    }
    else if(light == 0xffff && hue == 0x0000 && saturation == 0x0000)
    {
        set_led_color(WHITE, my_dev.led_light);
        my_dev.led_color = WHITE;
    }
    else if(light == 0x8000 && hue == 0x2aaa && saturation == 0xffff)
    {
        set_led_color(YELLOW, my_dev.led_light);
        my_dev.led_color = YELLOW;
    }
    else
    {
        set_led_color(WHITE, my_dev.led_light);
        my_dev.led_color = WHITE;
    }

    return;
}

void set_led_recv(elem_state_t *p_user_state)
{
    if(p_user_state == NULL)
    {
        printf("user_state is NULL!\r\n");
        return;
    }
    printf("onoff=%d,actual=%d,temperature=%d\r\n",
            p_user_state->state.onoff[T_CUR],
            p_user_state->state.actual[T_CUR],
            p_user_state->state.temp[T_CUR]);
    #if 0
    if (p_user_state->state.onoff[T_CUR])
    {
        set_led_on(RED_LED);
        set_led_on(GREEN_LED);
        set_led_on(BLUE_LED);
    }
    else
    {
        set_led_off(RED_LED);
        set_led_off(GREEN_LED);
        set_led_off(BLUE_LED);
        return;
    }
    #endif
    // 设置灯的亮度,这里其实不用判断开灯关灯的值，因为亮度与开关直接有关联
    // (1)当亮度为0的时候 onoff直接就为0，亮度非0的时候，onoff为1
    // (2)另外，也不需要记录关灯前的亮度，天猫精灵会自动帮我们记录，
    //    当关灯再开灯的时候会自动下发上次开灯的亮度值，并且，亮度值并非
    //    是平台说的0-100，实际是0-65535，注意换算百分比。
    my_dev.led_light = p_user_state->state.actual[T_CUR];
    set_led_color(my_dev.led_color, (((float)p_user_state->state.actual[T_CUR]) / 0xffff));
    return;
}

void vnd_model_recv(vnd_model_msg *p_vnd_msg)
{
    int i = 0;
    uint16_t light = 0;
    uint16_t hue = 0;
    uint16_t saturation = 0;
    uint16_t type = 0;

    vnd_model_msg reply_msg = {0};
    uint8_t seg_count = 0;

    if(p_vnd_msg == NULL)
    {
        printf("p_vnd_msg is NULL!\r\n");
        return;
    }
    printf("opid 0x%x vnd_msg len=%d\r\n", p_vnd_msg->opid, p_vnd_msg->len);
    printf("data ");
    for (; i < p_vnd_msg->len; i++)
    {
        printf("0x%x ", p_vnd_msg->data[i]);
    }
    printf("\r\n");

    type |= p_vnd_msg->data[1];
    type <<= 8;
    type |= p_vnd_msg->data[0];

    switch(type)
    {
        case 0x0123:
            light |= p_vnd_msg->data[3];
            light <<= 8;
            light |= p_vnd_msg->data[2];

            hue |= p_vnd_msg->data[5];
            hue <<= 8;
            hue |= p_vnd_msg->data[4];

            saturation |= p_vnd_msg->data[7];
            saturation <<= 8;
            saturation |= p_vnd_msg->data[6];

            analize_color(light, hue, saturation);
            break;
        default:
            break;
    }
    
    seg_count = get_seg_count(p_vnd_msg->len + 4);
    reply_msg.opid = VENDOR_OP_ATTR_INDICATE; 
    reply_msg.tid = vendor_model_msg_gen_tid();
    reply_msg.data = p_vnd_msg->data;
    reply_msg.len = p_vnd_msg->len;
    reply_msg.p_elem = &elements[0];
    reply_msg.retry_period = 125 * seg_count + 400;
    reply_msg.retry = VENDOR_MODEL_MSG_DFT_RETRY_TIMES;
    genie_vendor_model_msg_send(&reply_msg);
    return;

}

void user_event(E_GENIE_EVENT event, void *p_arg)
{
    E_GENIE_EVENT next_event = event;
    switch(event) {
        case GENIE_EVT_SW_RESET:
        case GENIE_EVT_HW_RESET_START:

            break;
        case GENIE_EVT_HW_RESET_DONE:
            _reset_light_para();
            break;
        case GENIE_EVT_SDK_MESH_INIT:
            _init_light_para();
            _user_init();
            if (!genie_reset_get_flag()) {
                next_event = GENIE_EVT_SDK_ANALYZE_MSG;
            }
            break;
        case GENIE_EVT_SDK_MESH_PROV_SUCCESS:
            break;
        case GENIE_EVT_SDK_TRANS_CYCLE:
        case GENIE_EVT_SDK_ACTION_DONE:
            {
                printf("++++++++++ GENIE_EVT_SDK_ACTION_DONE ++++++++++\r\n");
                elem_state_t *user_state = (elem_state_t *)p_arg;
                set_led_recv(user_state);
            }
            break;
        case GENIE_EVT_SDK_INDICATE:
            break;
        case GENIE_EVT_SDK_VENDOR_MSG:
            {
                printf("++++++++++ GENIE_EVT_SDK_VENDOR_MSG ++++++++++\r\n");
                vnd_model_msg *my_vnd_msg = (vnd_model_msg *)p_arg;
                vnd_model_recv(my_vnd_msg);
            }
            break;
        default:
            break;
    }
    if(next_event != event) {
        genie_event(next_event, p_arg);
    }
}
#if 1
int application_start(int argc, char **argv)
{
    printf("++++++++++ BUILD_TIME:%s ++++++++++\r\n", __DATE__","__TIME__);
    genie_init();
    return 0;
}
#else
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
#endif

