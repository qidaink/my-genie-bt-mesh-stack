/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef _MULTI_ADV_H_
#define _MULTI_ADV_H_

#include "port/mesh_hal_os.h"

#define MAX_MULTI_ADV_INSTANT       4
#define MAX_AD_DATA_LEN             31

#define TIME_PRIOD_MS               100
#define SLOT_PER_PERIOD             (TIME_PRIOD_MS*8/5)

#define MAX_MIN_MULTI               300

#define HIGH_DUTY_CYCLE_INTERVAL    (40*8/5)

#define MULTI_ADV_EV_COUNT          3


#define EVENT_TYPE_PROXY_EVENT              (1<<0)
#define EVENT_TYPE_MESH_EVENT               (1<<1)
#define EVENT_TYPE_MULTI_ADV_EVENT          (1<<2)
#define EVENT_TYPE_MULTI_ADV_TIMER_EVENT    (1<<3)
#define EVENT_TYPE_MESH_TIMER_EVENT         (1<<4)
#define EVENT_TYPE_ALL              (EVENT_TYPE_PROXY_EVENT|EVENT_TYPE_MESH_EVENT|                  \
                                    EVENT_TYPE_MULTI_ADV_TIMER_EVENT|EVENT_TYPE_MULTI_ADV_EVENT|EVENT_TYPE_MESH_TIMER_EVENT)

struct multi_adv_instant {
    uint8_t inuse_flag;
       
    /* for parameters  */
    struct bt_le_adv_param param;
    uint8_t ad[MAX_AD_DATA_LEN];
    uint8_t ad_len;
    uint8_t sd[MAX_AD_DATA_LEN];
    uint8_t sd_len;

    /* own address maybe used */
    bt_addr_t own_addr;
    uint8_t own_addr_valid;
    
    /* for schedule */
    int instant_id;
    int instant_interval;
    int instant_offset;
    uint32_t clock;
    uint32_t clock_instant_offset;
    uint32_t clock_instant_total;
    uint32_t next_wakeup_time;
};

typedef enum {
    SCHEDULE_IDLE,
    SCHEDULE_READY,
    SCHEDULE_START,
    SCHEDULE_STOP,
}SCHEDULE_STATE;

struct multi_adv_scheduler {
    SCHEDULE_STATE schedule_state;
    uint8_t schedule_timer_active;
    uint32_t slot_clock;
    uint16_t slot_offset;
    uint16_t next_slot_offset;
    uint32_t next_slot_clock;
};

kevent_t *bt_mesh_multi_adv_get_event(void);
int bt_mesh_multi_adv_thread_init(void);
int bt_mesh_multi_adv_thread_run(void);

int bt_le_multi_adv_start(const struct bt_le_adv_param *param,
                    const struct bt_data *ad, size_t ad_len,
                    const struct bt_data *sd, size_t sd_len, int *instant_id);
int bt_le_multi_adv_stop(int instant_id);


#endif
