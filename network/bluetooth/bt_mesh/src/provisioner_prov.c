/*  Bluetooth Mesh */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef  CONFIG_BT_MESH_ROLE_PROVISIONER
#include <stdio.h>

#include <zephyr.h>
#include <errno.h>
#include <atomic.h>
#include <misc/util.h>
#include <misc/byteorder.h>

#include <net/buf.h>
#include <bluetooth.h>
#include <conn.h>
#include <api/mesh.h>
#include <uuid.h>
#include "genie_mesh_flash.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_MESH_DEBUG_PROV)
#include "common/log.h"

#ifdef MESH_DEBUG_PROV
#define ALI_PROV_TAG "\t[ALI_PROV]"
#define PROV_D(f, ...) printf("%d "ALI_PROV_TAG"[D] %s "f"\n", (u32_t)aos_now_ms(), __func__, ##__VA_ARGS__)
#else
#define PROV_D(f, ...)
#endif
#include "ecc.h"

#include "port/mesh_hal_ble.h"
#include "mesh_crypto.h"
#include "adv.h"
#include "mesh.h"
#include "net.h"
#include "access.h"
#include "foundation.h"
#include "proxy.h"
#include <mesh/main.h>


#include "bt_mesh_custom_log.h"

#include "provisioner_prov.h"
#include "tri_tuple_default.h"

/* 4 transmissions, 20ms interval */
#define PROV_XMIT_COUNT        0
#define PROV_XMIT_INT          20

#define AUTH_METHOD_NO_OOB     0x00
#define AUTH_METHOD_STATIC     0x01
#define AUTH_METHOD_OUTPUT     0x02
#define AUTH_METHOD_INPUT      0x03

#define OUTPUT_OOB_BLINK       0x00
#define OUTPUT_OOB_BEEP        0x01
#define OUTPUT_OOB_VIBRATE     0x02
#define OUTPUT_OOB_NUMBER      0x03
#define OUTPUT_OOB_STRING      0x04

#define INPUT_OOB_PUSH         0x00
#define INPUT_OOB_TWIST        0x01
#define INPUT_OOB_NUMBER       0x02
#define INPUT_OOB_STRING       0x03

#define PROV_ERR_NONE          0x00
#define PROV_ERR_NVAL_PDU      0x01
#define PROV_ERR_NVAL_FMT      0x02
#define PROV_ERR_UNEXP_PDU     0x03
#define PROV_ERR_CFM_FAILED    0x04
#define PROV_ERR_RESOURCES     0x05
#define PROV_ERR_DECRYPT       0x06
#define PROV_ERR_UNEXP_ERR     0x07
#define PROV_ERR_ADDR          0x08

#define PROV_INVITE            0x00
#define PROV_CAPABILITIES      0x01
#define PROV_START             0x02
#define PROV_PUB_KEY           0x03
#define PROV_INPUT_COMPLETE    0x04
#define PROV_CONFIRM           0x05
#define PROV_RANDOM            0x06
#define PROV_DATA              0x07
#define PROV_FAILED            0x09
#define PROV_COMPLETE          0x08

#define PROV_ALG_P256          0x00

#define GPCF(gpc)              (gpc & 0x03)
#define GPC_START(last_seg)    (((last_seg) << 2) | 0x00)
#define GPC_ACK                0x01
#define GPC_CONT(seg_id)       (((seg_id) << 2) | 0x02)
#define GPC_CTL(op)            (((op) << 2) | 0x03)

#define START_PAYLOAD_MAX      20
#define CONT_PAYLOAD_MAX       23

#define START_LAST_SEG(gpc)    (gpc >> 2)
#define CONT_SEG_INDEX(gpc)    (gpc >> 2)

#define BEARER_CTL(gpc)        (gpc >> 2)
#define LINK_OPEN              0x00
#define LINK_ACK               0x01
#define LINK_CLOSE             0x02

#define CLOSE_REASON_SUCCESS   0x00
#define CLOSE_REASON_TIMEOUT   0x01
#define CLOSE_REASON_FAILED    0x02

#define PROV_BUF(len) NET_BUF_SIMPLE(PROV_BUF_HEADROOM + len)


#define XACT_SEG_DATA(_seg) (&link.rx.buf->data[20 + ((_seg - 1) * 23)])
#define XACT_SEG_RECV(_seg) (link.rx.seg &= ~(1 << (_seg)))

#define XACT_NVAL              0xff

enum {
    REMOTE_PUB_KEY,        /* Remote key has been received */
    LOCAL_PUB_KEY,         /* Local public key is available */
    LINK_ACTIVE,           /* Link has been opened */
    WAIT_GEN_DHKEY,        /* Waiting for remote public key to generate DHKey */
    HAVE_DHKEY,            /* DHKey has been calcualted */
    SEND_CONFIRM,          /* Waiting to send Confirm value */
    WAIT_NUMBER,           /* Waiting for number input from user */
    WAIT_STRING,           /* Waiting for string input from user */

    NUM_FLAGS,
};


#define BLE_MESH_ADDR_IS_UNICAST(addr)  ((addr) && (addr) < 0x8000)
#define BLE_MESH_ADDR_IS_GROUP(addr)    ((addr) >= 0xc000 && (addr) <= 0xff00)
#define BLE_MESH_ADDR_IS_VIRTUAL(addr)  ((addr) >= 0x8000 && (addr) < 0xc000)
#define BLE_MESH_ADDR_IS_RFU(addr)      ((addr) >= 0xff00 && (addr) <= 0xfffb)

#define BLE_MESH_ADDR_UNASSIGNED   0x0000
#define BLE_MESH_ADDR_ALL_NODES    0xffff
#define BLE_MESH_ADDR_PROXIES      0xfffc
#define BLE_MESH_ADDR_FRIENDS      0xfffd
#define BLE_MESH_ADDR_RELAYS       0xfffe


#define BLE_MESH_KEY_PRIMARY            0x0000
#define BLE_MESH_KEY_ANY                0xffff

#define BLE_MESH_NODE_IDENTITY_STOPPED       0x00
#define BLE_MESH_NODE_IDENTITY_RUNNING       0x01
#define BLE_MESH_NODE_IDENTITY_NOT_SUPPORTED 0x02

#define BLE_MESH_IVU_MIN_HOURS      96
#define BLE_MESH_IVU_HOURS          (BLE_MESH_IVU_MIN_HOURS / CONFIG_BLE_MESH_IVU_DIVIDER)
#define BLE_MESH_IVU_TIMEOUT        K_HOURS(BLE_MESH_IVU_HOURS)



//节点名字最大长度
#define BLE_MESH_NODE_NAME_SIZE         31
//最大连接节点数
#define CONFIG_BLE_MESH_MAX_PROV_NODES  5

/* Each node information stored by provisioner */
struct bt_mesh_node {
    /* Device information */
    uint8_t  addr[6];      /* Node device address */
    uint8_t  addr_type;    /* Node device address type */
    uint8_t  dev_uuid[16]; /* Node Device UUID */
    uint16_t oob_info;     /* Node OOB information */

    /* Provisioning information */
    uint16_t unicast_addr; /* Node unicast address */
    uint8_t  element_num;  /* Node element number */
    uint16_t net_idx;      /* Node NetKey Index */
    uint8_t  flags;        /* Node key refresh flag and iv update flag */
    uint32_t iv_index;     /* Node IV Index */
    uint8_t  dev_key[16];  /* Node device key */

    /* Additional information */
    char  name[BLE_MESH_NODE_NAME_SIZE + 1]; /* Node name */
    uint16_t comp_length;  /* Length of Composition Data */
    uint8_t *comp_data;    /* Value of Composition Data */
} __packed;


struct prov_link {
    ATOMIC_DEFINE(flags, NUM_FLAGS);
#if defined(CONFIG_BT_MESH_PB_GATT)
    bt_mesh_conn_t conn;    /* GATT connection */
#endif
    uint8_t  uuid[16];       /* check if device is being provisioned*/
    u8_t  dhkey[32];         /* Calculated DHKey */
    u8_t  expect;            /* Next expected PDU */

    uint8_t  element_num;

    u8_t  oob_method;
    u8_t  oob_action;
    u8_t  oob_size;

    u8_t  conf[16];          /* Remote Confirmation */
    u8_t  rand[16];          /* Local Random */
    u8_t  auth[16];          /* Authentication Value */

    u8_t  conf_salt[16];     /* ConfirmationSalt */
    u8_t  conf_key[16];      /* ConfirmationKey */
    u8_t  conf_inputs[145];  /* ConfirmationInputs */
    u8_t  prov_salt[16];     /* Provisioning Salt */

    u8_t local_conf[16];        /* Local Confirmation */
    uint16_t assign_addr;       /* Application assigned address for the device */
    uint16_t unicast_addr;      /* unicast address allocated for device */
    uint8_t  ki_flags;          /* Key refresh flag and iv update flag */
    uint32_t iv_index;          /* IV Index */
#if defined(CONFIG_BT_MESH_PB_ADV)
    u32_t    id;                /* Link ID */
    bool     linking;           /* Linking is being establishing */
    uint16_t send_link_close;   /* Link close is being sent flag */
    uint8_t  pending_ack;       /* Decide which transaction id ack is pending */
    uint8_t  expect_ack_for;    /* Transaction ACK expected for provisioning pdu */
    uint8_t  tx_pdu_type;       /* The current transmitted Provisioning PDU type */

    struct {
        u8_t  id;        /* Transaction ID */
        u8_t  prev_id;   /* Previous Transaction ID */
        u8_t  seg;       /* Bit-field of unreceived segments */
        u8_t  last_seg;  /* Last segment (to check length) */
        u8_t  fcs;       /* Expected FCS value */
        struct net_buf_simple *buf;
    } rx;

    struct {
        /* Start timestamp of the transaction */
        s64_t start;

        /* Transaction id*/
        u8_t id;

        /* Pending outgoing buffer(s) */
        struct net_buf *buf[3];

        /* Retransmit timer */
        struct k_delayed_work retransmit;
    } tx;
#endif
};



struct bt_mesh_prov_ctx {
    /* Primary element address of Provisioner */
    uint16_t primary_addr;

    /* Provisioning bearers used by Provisioner */
    bt_mesh_prov_bearer_t bearers;

    /* Current number of PB-ADV provisioned devices simultaneously */
    uint8_t  pba_count;

    /* Current number of PB-GATT provisioned devices simultaneously */
    uint8_t  pbg_count;

    /* Current unicast address going to allocated */
    uint16_t curr_alloc_addr;

    /* Current net_idx going to be used in provisioning data */
    uint16_t curr_net_idx;

    /* Current flags going to be used in provisioning data */
    uint8_t  curr_flags;

    /* Current iv_index going to be used in provisioning data */
    uint16_t curr_iv_index;

    /* Length of Static OOB value */
    uint8_t  static_oob_len;

    /* Static OOB value */
    uint8_t  static_oob_val[16];

    /* Offset of the device uuid to be matched, based on zero */
    uint8_t  match_offset;

    /* Length of the device uuid to be matched (start from the match_offset) */
    uint8_t  match_length;

    /* Value of the device uuid to be matched */
    uint8_t  match_value[16];

    /* Indicate when received uuid_match adv_pkts, can provision it at once */
    bool prov_after_match;

#if defined(CONFIG_BLE_MESH_PB_ADV)
    /* Mutex used to protect the PB-ADV procedure */
    bt_mesh_mutex_t pb_adv_lock;

    /* Mutex used to protect the adv buf during PB-ADV procedure */
    bt_mesh_mutex_t pb_buf_lock;
#endif

#if defined(CONFIG_BLE_MESH_PB_GATT)
    /* Mutex used to protect the PB-GATT procedure */
    bt_mesh_mutex_t pb_gatt_lock;
#endif


};

struct prov_rx {
    u32_t link_id;
    u8_t  xact_id;
    u8_t  gpc;
};

#define BLE_MESH_KEY_UNUSED             0xffff
#define CONFIG_BLE_MESH_SUBNET_COUNT    3
#define CONFIG_BLE_MESH_PROVISIONER_APP_KEY_COUNT   3

extern struct bt_mesh_net bt_mesh;

#if defined(CONFIG_BT_MESH_PB_ADV)
#define ADV_BUF_SIZE    65

static struct prov_adv_buf {
    struct net_buf_simple buf;
} adv_buf;

static uint8_t adv_buf_data[ADV_BUF_SIZE];
#endif /* CONFIG_BT_MESH_PB_ADV */


static struct bt_mesh_prov_ctx prov_ctx;
static struct prov_link link;
static const struct bt_mesh_prov *prov;


static uint16_t addr;
static uint32_t seq;
static uint8_t provisoner_devkey[16];
static mesh_netkey_para_t g_netkey;
static mesh_appkey_para_t appkey;
static u32_t seq;
static u16_t addr;


static struct bt_mesh_node mesh_nodes[CONFIG_BLE_MESH_MAX_PROV_NODES];
uint8_t filter_uuid[] = {0xa8,0x01};

#define RETRANSMIT_TIMEOUT   K_MSEC(500)
#define BUF_TIMEOUT          K_MSEC(400)
#define TRANSACTION_TIMEOUT  K_SECONDS(30)

#if defined(CONFIG_BT_MESH_PB_GATT)
#define PROV_BUF_HEADROOM 5
#else
#define PROV_BUF_HEADROOM 0
static struct net_buf_simple *rx_buf = NET_BUF_SIMPLE(65);
#endif



static void close_link(u8_t err, u8_t reason);
static void free_segments(void);
int bt_mesh_pb_gatt_close(bt_mesh_conn_t conn);
static inline int prov_send(struct net_buf_simple *buf);
static void send_pub_key(uint8_t oob);
static struct net_buf *adv_buf_create(void);

void bt_mesh_role_provisioner_test(void)
{
    printf("This is bt_mesh_role_provisioner_test!!!\r\n");
    return;
}

// 配网器通过网络索引获取网络密钥
const uint8_t *bt_mesh_provisioner_net_key_get(uint16_t net_idx)
{
    struct bt_mesh_subnet *sub = NULL;
    int i;

    BT_DBG("%s", __func__);

    for (i = 0; i < ARRAY_SIZE(bt_mesh.p_sub); i++) {
        sub = bt_mesh.p_sub[i];
        BT_INFO("sub->net_idx = %u",sub->net_idx);
        if (sub && sub->net_idx == net_idx) {
            if (sub->kr_flag) {
                return sub->keys[1].net;
            } else {
                return sub->keys[0].net;
            }
        }
    }

    return NULL;
}



#if defined(CONFIG_BT_MESH_PB_ADV)
//发送重传
static void buf_sent(int err, void *user_data)
{
    //如果发送缓存为空，则返回
    if (!link.tx.buf[0]) {
        return;
    }
	//如果不为空，启动重传
    k_delayed_work_submit(&link.tx.retransmit, RETRANSMIT_TIMEOUT);
}

static struct bt_mesh_send_cb buf_sent_cb = {
    .end = buf_sent,
};
//初始化缓存区
static void prov_buf_init(struct net_buf_simple *buf, u8_t type)
{
    net_buf_simple_init(buf, PROV_BUF_HEADROOM);
    net_buf_simple_add_u8(buf, type);
}


//可靠发送缓存数据
static void send_reliable(void)
{
    int i;

    link.tx.start = k_uptime_get();

    for (i = 0; i < ARRAY_SIZE(link.tx.buf); i++) {
        struct net_buf *buf = link.tx.buf[i];

        if (!buf) {
            break;
        }

        if (i + 1 < ARRAY_SIZE(link.tx.buf) && link.tx.buf[i + 1]) {
            bt_mesh_adv_send(buf, NULL, NULL);
        } else {
            bt_mesh_adv_send(buf, &buf_sent_cb, NULL);
        }
    }
}
//取消发送
static void prov_clear_tx(void)
{

    k_delayed_work_cancel(&link.tx.retransmit);
	//清除缓存
    free_segments();
}
//发送PB-ADV广播
static int bearer_ctl_send(u8_t op, void *data, u8_t data_len)
{
    struct net_buf *buf;

    printf("发送%s op 0x%02x data_len %u\r\n",op == 0 ? "连接":op == 1 ? "应答" : op == 2 ? "断开" : NULL , op, data_len);

    prov_clear_tx();

    buf = adv_buf_create();
    if (!buf) {
        return -ENOBUFS;
    }
	//包头填充
    net_buf_add_be32(buf, link.id);
    /* Transaction ID, always 0 for Bearer messages */
    net_buf_add_u8(buf, 0x00);
    net_buf_add_u8(buf, GPC_CTL(op));
    net_buf_add_mem(buf, data, data_len);

    link.tx.buf[0] = buf;
    //可靠发送
    send_reliable();
	//如果是关闭操作还需修改一些其它标志
    if (op == LINK_CLOSE) {
        uint8_t reason = *(uint8_t *)data;
        link.send_link_close = ((reason & BIT_MASK(2)) << 1) | BIT(0);
        link.tx.id = 0;
    }

    return 0;
}

//配网器删除节点
int bt_mesh_provisioner_remove_node(const uint8_t uuid[16])
{
    int i;

    BT_DBG("%s", __func__);

    if (uuid == NULL) {
        BT_ERR("Invalid device uuid");
        return 0;
    }

    for (i = 0; i < ARRAY_SIZE(mesh_nodes); i++) {
        if (!memcmp(mesh_nodes[i].dev_uuid, uuid, 16)) {

            memset(&mesh_nodes[i],0,sizeof(mesh_nodes[i]));
        }
    }

    return 0;
}
//通过UUID找到节点
static struct bt_mesh_node *provisioner_find_node_with_uuid(const uint8_t uuid[16], uint16_t *index)
{
    int i;

    BT_DBG("%s", __func__);

    if (uuid == NULL) {
        BT_ERR("Invalid device uuid");
        return NULL;
    }

    for (i = 0; i < ARRAY_SIZE(mesh_nodes); i++) {
        if (!memcmp(mesh_nodes[i].dev_uuid, uuid, 16)) {
            if (index) {
                *index = i;
            }
            return &mesh_nodes[i];
        }
    }

    return NULL;
}


//节点是否连接
int node_is_connected(const uint8_t *uuid)
{
    if(uuid == NULL)
    {
        printf("UUID错误\r\n");
        return -1;
    }
    for(int i = 0; i < CONFIG_BLE_MESH_MAX_PROV_NODES;i++)
    {
        if(memcmp(&mesh_nodes[i].dev_uuid[0],uuid,16) == 0)
        {
            printf("已经建立过连接\r\n");
            return 1;
        }
    }
    
    return 0;

}
//广播包过滤uuid
int adv_filter_uuid(const uint8_t * uuid)
{
    if(uuid == NULL)
    {
        printf("uuid 错误\r\n");
        return -1;
    }
    if(memcmp(filter_uuid,uuid,sizeof(filter_uuid)) == 0)
    {
        printf("找到未配网设备\r\n");
        return 0;
    }

    return 1;
}

//获得网络层缓存区
static struct net_buf_simple *bt_mesh_pba_get_buf(void)
{
    struct net_buf_simple *buf = &adv_buf.buf;

    buf->len  = 0;
    buf->data = adv_buf_data;
    *(&buf->__buf[0]) = adv_buf_data;
    buf->size = 65;

    return buf;
}


//建立连接
void provisioner_link_open(const uint8_t * uuid)
{
    //先判断UUID是否合法
    if(node_is_connected(uuid) || adv_filter_uuid(uuid))
    {
        return ;
    }

    if(atomic_test_bit(link.flags, LINK_ACTIVE))
    {
        printf("已经在配网阶段\r\n");
        return;
    }

    printf("生成随机数\r\n");
    //随机生成一个连接ID
    bt_mesh_rand(&link.id,sizeof(link.id));
    //发起配网操作
    bearer_ctl_send(LINK_OPEN,(void *)uuid, 16);
    printf("发起配网\r\n");
    //应用层触发开始配网事件
    genie_event(GENIE_EVT_SDK_MESH_PROV_START,&link.id);
    

    /* Set LINK_ACTIVE just to be in compatibility with current Zephyr code */
    atomic_set_bit(link.flags, LINK_ACTIVE);

    if (prov->link_open) {
        prov->link_open(BT_MESH_PROV_ADV);
    }

    link.expect = PROV_INVITE;


    return;
}
//发送邀请
static void send_invite(void)
{
    //实例化网络层缓存buf，实际为局部结构体变量
    struct net_buf_simple *buf = PROV_BUF(2);
	//初始化缓存头部类型
    prov_buf_init(buf, PROV_INVITE);
    //添加连接超时
    net_buf_simple_add_u8(buf, 0);
    //等待连接时间记录到本地
    link.conf_inputs[0] = 0;
	//发送缓存
    if (prov_send(buf)) {
        BT_ERR("Failed to send Provisioning Invite");
        close_link(PROV_ERR_NVAL_PDU, CLOSE_REASON_FAILED);
        return;
    }
    printf("已发出邀请\r\n");
    //下次预期接收配网能力
    link.expect = PROV_CAPABILITIES;
}

//释放实例化缓存
static void free_segments(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(link.tx.buf); i++) {
        struct net_buf *buf = link.tx.buf[i];

        if (!buf) {
            break;
        }

        link.tx.buf[i] = NULL;
        /* Mark as canceled */
        BT_MESH_ADV(buf)->busy = 0;
        net_buf_unref(buf);
    }
}


//复位连接
static void reset_link(void)
{
    prov_clear_tx();

    if (prov->link_close) {
        prov->link_close(BT_MESH_PROV_ADV);
    }

    /* Clear everything except the retransmit delayed work config */
    memset(&link, 0, offsetof(struct prov_link, tx.retransmit));

    link.rx.prev_id = XACT_NVAL;

    if (bt_mesh_pub_key_get()) {
        atomic_set_bit(link.flags, LOCAL_PUB_KEY);
    }

#if defined(CONFIG_BT_MESH_PB_GATT)
    link.rx.buf = bt_mesh_proxy_get_buf();
#else
    net_buf_simple_init(rx_buf, 0);
    link.rx.buf = rx_buf;
#endif

    /* Disable Attention Timer if it was set */
    if (link.conf_inputs[0]) {
        bt_mesh_attention(NULL, 0);
    }
}


//创建广播
static struct net_buf *adv_buf_create(void)
{
    struct net_buf *buf;

    buf = bt_mesh_adv_create(BT_MESH_ADV_PROV, PROV_XMIT_COUNT,
                             PROV_XMIT_INT, BUF_TIMEOUT);
    if (!buf) {
        BT_ERR("Out of provisioning buffers");
        return NULL;
    }

    return buf;
}

static u8_t pending_ack = XACT_NVAL;
//响应完成回调用
static void ack_complete(u16_t duration, int err, void *user_data)
{
    BT_DBG("xact %u complete", (u8_t)pending_ack);
    pending_ack = XACT_NVAL;
}
//发送响应
static void gen_prov_ack_send(u8_t xact_id)
{
    static const struct bt_mesh_send_cb cb = {
        .start = ack_complete,
    };
    const struct bt_mesh_send_cb *complete;
    struct net_buf *buf;

    BT_DBG("pending_ack = %u,xact_id %u",pending_ack,xact_id);

    if (pending_ack == xact_id) {
        BT_DBG("Not sending duplicate ack");
        return;
    }
	//创建广播包缓存
    buf = adv_buf_create();
    if (!buf) {
        return;
    }

    if (pending_ack == XACT_NVAL) {
        pending_ack = xact_id;
        complete = &cb;
    } else {
        complete = NULL;
    }

    net_buf_add_be32(buf, link.id);
    net_buf_add_u8(buf, xact_id);
    net_buf_add_u8(buf, GPC_ACK);
	//发送响应广播
    bt_mesh_adv_send(buf, complete, NULL);
    //释放缓存
    net_buf_unref(buf);
}



//计算最后包号
static u8_t last_seg(u8_t len)
{
    //如果小于20个字节 就一包数据
    if (len <= START_PAYLOAD_MAX) {
        return 0;
    }
	//否则超过20个字节就要分包（len -= 20）
    len -= START_PAYLOAD_MAX;
	//一包数据传23个字节（第一包额外加了一些包头所以是20个字节）
    return 1 + (len / CONT_PAYLOAD_MAX);   // 1 + （len / 23）
}
//下一个传输编号
static inline u8_t next_transaction_id(void)
{
    if (link.tx.id > 0x7F) {
        link.tx.id = 0x0;
    }
    return link.tx.id++;
}
//发送广播
static int prov_send_adv(struct net_buf_simple *msg)
{
    struct net_buf *start, *buf;
    u8_t seg_len, seg_id;
    u8_t xact_id;

    
    //PROV_D("len %u: %s", msg->len, bt_hex(msg->data, msg->len));

	//清除发送缓存
    prov_clear_tx();
	//创建广播包缓存
    start = adv_buf_create();
    if (!start) {
        return -ENOBUFS;
    }
	//下一个传输编号
    xact_id = next_transaction_id();
    net_buf_add_be32(start, link.id);
    net_buf_add_u8(start, xact_id);

    net_buf_add_u8(start, GPC_START(last_seg(msg->len)));
    net_buf_add_be16(start, msg->len);
    net_buf_add_u8(start, bt_mesh_fcs_calc(msg->data, msg->len));

    link.tx.buf[0] = start;

    seg_len = min(msg->len, START_PAYLOAD_MAX);
//    printf("seg 0 len %u: %s", seg_len, bt_hex(msg->data, seg_len));
    net_buf_add_mem(start, msg->data, seg_len);
    net_buf_simple_pull(msg, seg_len);
    printf("发送: 传输标号 = %u\r\n",xact_id);
    buf = start;
    //将所以数据写入缓存
    for (seg_id = 1; msg->len > 0; seg_id++) {
        if (seg_id >= ARRAY_SIZE(link.tx.buf)) {
            BT_ERR("Too big message");
            free_segments();
            return -E2BIG;
        }

        buf = adv_buf_create();
        if (!buf) {
            free_segments();
            return -ENOBUFS;
        }

        link.tx.buf[seg_id] = buf;

        seg_len = min(msg->len, CONT_PAYLOAD_MAX);

        printf("seg_id %u len %u: %s\r\n", seg_id, seg_len,
               bt_hex(msg->data, seg_len));

        net_buf_add_be32(buf, link.id);
        net_buf_add_u8(buf, xact_id);
        net_buf_add_u8(buf, GPC_CONT(seg_id));
        net_buf_add_mem(buf, msg->data, seg_len);
        net_buf_simple_pull(msg, seg_len);
    }
	//可靠发送
    send_reliable();

    return 0;
}

#endif /* CONFIG_BT_MESH_PB_ADV */

#if defined(CONFIG_BT_MESH_PB_GATT)
static int prov_send_gatt(struct net_buf_simple *msg)
{
    if (!link.conn) {
        return -ENOTCONN;
    }

    return bt_mesh_proxy_send(link.conn, BT_MESH_PROXY_PROV, msg);
}
#endif /* CONFIG_BT_MESH_PB_GATT */

static inline int prov_send(struct net_buf_simple *buf)
{
#if defined(CONFIG_BT_MESH_PB_GATT)
    if (link.conn) {
        return prov_send_gatt(buf);
    }
#endif
#if defined(CONFIG_BT_MESH_PB_ADV)
    //进入到这里发送
    return prov_send_adv(buf);
#else
    return 0;
#endif
}

//发送配网失败消息
static void prov_send_fail_msg(u8_t err)
{
    struct net_buf_simple *buf = PROV_BUF(2);

    prov_buf_init(buf, PROV_FAILED);
    net_buf_simple_add_u8(buf, err);
    prov_send(buf);
}

static void prov_invite(const u8_t *data)
{
    printf("prov_invite\r\n");
}

static void prov_capabilities(const u8_t *data)
{
    printf("接收到配网能力\r\n");
    struct net_buf_simple *buf = PROV_BUF(6);
    uint16_t algorithms = 0U, output_action = 0U, input_action = 0U;
    uint8_t  element_num = 0U, pub_key_oob = 0U, static_oob = 0U,
             output_size = 0U, input_size = 0U;
    uint8_t  auth_method = 0U, auth_action = 0U, auth_size = 0U;

    element_num = data[0];
    BT_INFO("Elements:          0x%02x", element_num);
    if (!element_num) {
        BT_ERR("Invalid element number %d", element_num);
        goto fail;
    }
    //元素个数
    link.element_num = element_num;

    algorithms = sys_get_be16(&data[1]);
    BT_INFO("Algorithms:        0x%04x", algorithms);
    if (algorithms != BIT(PROV_ALG_P256)) {
        BT_ERR("Invalid algorithms 0x%04x", algorithms);
        goto fail;
    }
	//配网能力标志检查
    pub_key_oob = data[3];
    BT_INFO("Public Key Type:   0x%02x", pub_key_oob);
    if (pub_key_oob > 0x01) {
        BT_ERR("Invalid public key type 0x%02x", pub_key_oob);
        goto fail;
    }
    
    pub_key_oob = ((prov->prov_pub_key_oob &&
                    prov->prov_pub_key_oob_cb) ? pub_key_oob : 0x00);
    
    static_oob = data[4];
    BT_INFO("Static OOB Type:   0x%02x", static_oob);
    if (static_oob > 0x01) {
        BT_ERR("Invalid Static OOB type 0x%02x", static_oob);
        goto fail;
    }
    static_oob = (prov_ctx.static_oob_len ? static_oob : 0x00);

    output_size = data[5];
    BT_INFO("Output OOB Size:   0x%02x", output_size);
    if (output_size > 0x08) {
        BT_ERR("Invalid Output OOB size %d", output_size);
        goto fail;
    }

    output_action = sys_get_be16(&data[6]);
    BT_INFO("Output OOB Action: 0x%04x", output_action);
    if (output_action > 0x1f) {
        BT_ERR("Invalid Output OOB action 0x%04x", output_action);
        goto fail;
    }

    /* Provisioner select output action */
    if (prov->prov_input_num && output_size) {
        output_action = __builtin_ctz(output_action);
    } else {
        output_size = 0x0;
        output_action = 0x0;
    }

    input_size = data[8];
    BT_INFO("Input OOB Size:    0x%02x", input_size);
    if (input_size > 0x08) {
        BT_ERR("Invalid Input OOB size %d", input_size);
        goto fail;
    }

    input_action = sys_get_be16(&data[9]);
    BT_INFO("Input OOB Action:  0x%04x", input_action);
    if (input_action > 0x0f) {
        BT_ERR("Invalid Input OOB action 0x%04x", input_action);
        goto fail;
    }

#if 0
    /* Make sure received pdu is ok and cancel the timeout timer */
    if (bt_mesh_atomic_test_and_clear_bit(link.flags, TIMEOUT_START)) {
        k_delayed_work_cancel(&link.timeout);
    }
#endif

    /* Provisioner select input action */
    if (prov->prov_output_num && input_size) {
        input_action = __builtin_ctz(input_action);
    } else {
        input_size = 0x0;
        input_action = 0x0;
    }

    if (static_oob) {
        /* if static oob is valid, just use static oob */
        auth_method = AUTH_METHOD_STATIC;
        auth_action = 0x00;
        auth_size   = 0x00;
    } else {
        if (!output_size && !input_size) {
            auth_method = AUTH_METHOD_NO_OOB;
            auth_action = 0x00;
            auth_size   = 0x00;
        } else if (!output_size && input_size) {
            auth_method = AUTH_METHOD_INPUT;
            auth_action = (uint8_t)input_action;
            auth_size   = input_size;
        } else {
            auth_method = AUTH_METHOD_OUTPUT;
            auth_action = (uint8_t)output_action;
            auth_size   = output_size;
        }
    }

    /* Store provisioning capabilities value in conf_inputs */
    //保存要发送的参数到 conf_inputs 里，以便后期用来生成“配网佐料”
    memcpy(&link.conf_inputs[1], data, 11);
    //填充配网阶段参数
    prov_buf_init(buf, PROV_START);
    net_buf_simple_add_u8(buf, prov->prov_algorithm);
    net_buf_simple_add_u8(buf, pub_key_oob);
    net_buf_simple_add_u8(buf, auth_method);
    net_buf_simple_add_u8(buf, auth_action);
    net_buf_simple_add_u8(buf, auth_size);
    //复制到认证数据里计算认证时用
    memcpy(&link.conf_inputs[12], &buf->data[1], 5);
    //发送provsisioning start
    if (prov_send(buf)) {
        BT_ERR("Failed to send Provisioning Start");
        goto fail;
    }

    link.oob_method = auth_method;
    link.oob_action = auth_action;
    link.oob_size   = auth_size;

    /** After prov start sent, use OOB to get remote public key.
     *  And we just follow the procedure in Figure 5.15 of Section
     *  5.4.2.3 of Mesh Profile Spec.
     */
#if 1
    if (pub_key_oob) {
        if (prov->prov_pub_key_oob_cb()) {
            BT_ERR("Failed to notify input OOB Public Key");
            goto fail;
        }
    }
#endif

    /** If using PB-ADV, need to listen for transaction ack,
     *  after ack is received, provisioner can send public key.
     */
#if defined(CONFIG_BT_MESH_PB_ADV)

    link.expect_ack_for = PROV_START;
    return;

#endif /* CONFIG_BT_MESH_PB_ADV */
    //发送交换公钥
    send_pub_key(pub_key_oob);
    return;

fail:
    close_link(PROV_ERR_CFM_FAILED,CLOSE_REASON_FAILED);
    return;
}

//oob认证数据生成（无oob只能填充0）
static int prov_auth(u8_t method, u8_t action, u8_t size)
{

    switch (method) {
        case AUTH_METHOD_STATIC:
            if (action || size) {
                return -EINVAL;
            }
            memcpy(link.auth + 16 - prov->static_val_len,
                   prov->static_val, prov->static_val_len);
            memset(link.auth, 0, sizeof(link.auth) - prov->static_val_len);
            return 0;
        //只支持无OOB
        case AUTH_METHOD_NO_OOB:
            if (action || size) {
                return -EINVAL;
            }

            memset(link.auth, 0, sizeof(link.auth));
            return 0;

        case AUTH_METHOD_OUTPUT:
        case AUTH_METHOD_INPUT:
    

        default:
            return -EINVAL;
    }
}

static void prov_start(const u8_t *data)
{
    printf("prov_start\r\n");   
}


//发送认证
static void send_confirm(void)
{
    struct net_buf_simple *buf = PROV_BUF(17);
    uint8_t *conf = NULL;

    BT_INFO("ConfInputs[0]   %s", bt_hex(link.conf_inputs, 64));
    BT_INFO("ConfInputs[64]  %s", bt_hex(&link.conf_inputs[64], 64));
    BT_INFO("ConfInputs[128] %s", bt_hex(&link.conf_inputs[128], 17));
	//生成Confirmation salt  认证佐料
    if (bt_mesh_prov_conf_salt(link.conf_inputs, link.conf_salt)) {
        BT_ERR("Unable to generate confirmation salt");
        close_link(PROV_ERR_UNEXP_ERR, CLOSE_REASON_FAILED);
        return;
    }

    BT_INFO("ConfirmationSalt: %s", bt_hex(link.conf_salt, 16));
	//共享密钥和认证佐料生成认证密钥
    if (bt_mesh_prov_conf_key(link.dhkey, link.conf_salt, link.conf_key)) {
        BT_ERR("Unable to generate confirmation key");
        close_link(PROV_ERR_UNEXP_ERR, CLOSE_REASON_FAILED);
        return;
    }

    BT_INFO("ConfirmationKey: %s", bt_hex(link.conf_key, 16));
	//生成随机值
    if (bt_mesh_rand(link.rand, 16)) {
        BT_ERR("Unable to generate random number");
        close_link(PROV_ERR_UNEXP_ERR, CLOSE_REASON_FAILED);
        return;
    }

    BT_INFO("LocalRandom: %s", bt_hex(link.rand, 16));

    prov_buf_init(buf, PROV_CONFIRM);

    conf = net_buf_simple_add(buf, 16);
    //认证密钥、随机值和oob数据（这里为0）生成确认值直接写入到了buf
    if (bt_mesh_prov_conf(link.conf_key, link.rand, link.auth,conf)) {
        BT_ERR("Unable to generate confirmation value");
        close_link(PROV_ERR_UNEXP_ERR, CLOSE_REASON_FAILED);
        return;
    }
    //拷贝确认值到本地存储
    memcpy(link.local_conf, conf, 16);
    //打印要发送得认证数据
    BT_INFO("********************************************");
    BT_INFO("link.conf_key: %s", bt_hex(link.conf_key, 16));
    BT_INFO("link.rand: %s", bt_hex(link.rand, 16));
    BT_INFO("conf_verify: %s", bt_hex(conf, 16));
    BT_INFO("********************************************");
    //发送确认值
    if (prov_send(buf)) {
        BT_ERR("Failed to send Provisioning Confirm");
        close_link(PROV_ERR_RESOURCES, CLOSE_REASON_FAILED);
        return;
    }

    link.expect = PROV_CONFIRM;
}





//共享密钥生成回调函数
static void prov_dh_key_cb(const u8_t key[32])
{
    BT_INFO("DHkey 地址：%p", key);
    if (!key) {
        BT_ERR("DHKey generation failed");
        close_link(PROV_ERR_UNEXP_ERR, CLOSE_REASON_FAILED);
        return;
    }

    sys_memcpy_swap(link.dhkey, key, 32);

    BT_INFO("DHkey: %s", bt_hex(link.dhkey, 32));

    atomic_set_bit(link.flags, HAVE_DHKEY);

    if (prov_auth(link.oob_method,
                  link.oob_action, link.oob_size) < 0) {
        BT_ERR("Failed to authenticate");
        close_link(PROV_ERR_UNEXP_ERR, CLOSE_REASON_FAILED);
    }
    if (link.oob_method == AUTH_METHOD_OUTPUT ||
            link.oob_method == AUTH_METHOD_INPUT) {
        return;
    }
	//如果下次预期的PDU不是输入比较，那就发送认证值
    if (link.expect != PROV_INPUT_COMPLETE) {
        send_confirm();
    }

    return;
}
//生成共享密钥
static void prov_gen_dh_key()
{
    uint8_t pub_key[64] = {0};

    /* Copy device public key in little-endian for bt_mesh_dh_key_gen().
     * X and Y halves are swapped independently.
     */
    sys_memcpy_swap(&pub_key[0], &link.conf_inputs[81], 32);
    sys_memcpy_swap(&pub_key[32], &link.conf_inputs[113], 32);
    //Diffie-Hellman Key
    if (bt_mesh_dh_key_gen(pub_key, prov_dh_key_cb)) {
        BT_ERR("Failed to generate DHKey");
        close_link(PROV_ERR_UNEXP_ERR, CLOSE_REASON_FAILED);
        return;
    }
}

//发送公钥
static void send_pub_key(uint8_t oob)
{
    struct net_buf_simple *buf = PROV_BUF(65);
    const u8_t *key;

    key = bt_mesh_pub_key_get();
    if (!key) {
        BT_ERR("No public key available");
        close_link(PROV_ERR_RESOURCES, CLOSE_REASON_FAILED);
        return;
    }
    
    BT_INFO("Local Public Key: %s\r\n", bt_hex(key, 64));
    atomic_set_bit(link.flags, LOCAL_PUB_KEY);
    prov_buf_init(buf, PROV_PUB_KEY);

    /* Swap X and Y halves independently to big-endian */
    sys_memcpy_swap(net_buf_simple_add(buf, 32), key, 32);
    sys_memcpy_swap(net_buf_simple_add(buf, 32), &key[32], 32);
    //存储公钥到conf_inputs里
    memcpy(&link.conf_inputs[17], &buf->data[1], 64);

    prov_send(buf);

    if (!oob) {
        BT_INFO("进入!oob link.expect = PROV_PUB_KEY");
        link.expect = PROV_PUB_KEY;
    } else {
        BT_INFO("没有进入link.expect = PROV_PUB_KEY");
        /** Have already got device public key. If next is to
         *  send confirm(not wait for input complete), need to
         *  wait for transactiona ack for public key then send
         *  provisioning confirm pdu.
         */
#if defined(CONFIG_BLE_MESH_PB_ADV)
//        if (idx < CONFIG_BLE_MESH_PBA_SAME_TIME) {
//            link.expect_ack_for = PROV_PUB_KEY;
//            return;
//        }
#endif /* CONFIG_BLE_MESH_PB_ADV */

        /* If remote public key has been read, then start to generate DHkey,
         * otherwise wait for device oob public key.
         */
        if (atomic_test_bit(link.flags, REMOTE_PUB_KEY)) {
            prov_gen_dh_key();
        } else {
            atomic_set_bit(link.flags, WAIT_GEN_DHKEY);
        }
    }

}

static void prov_pub_key(const u8_t *data)
{
    BT_INFO("Remote Public Key: %s", bt_hex(data, 64));

    //拷贝对方设备公钥
    memcpy(&link.conf_inputs[81], data, 64);

    if (!atomic_test_bit(link.flags, LOCAL_PUB_KEY)) {
        /* Clear retransmit timer */
#if defined(CONFIG_BT_MESH_PB_ADV)
        prov_clear_tx();
#endif
        atomic_set_bit(link.flags, REMOTE_PUB_KEY);
        BT_WARN("Waiting for local public key");
        return;
    }
    prov_gen_dh_key();
    //send_pub_key();
}


static void prov_input_complete(const u8_t *data)
{
    send_confirm();
}
//收到对方确认值进入
static void prov_confirm(const u8_t *data)
{
    BT_INFO("Remote Confirm: %s", bt_hex(data, 16));
    
    struct net_buf_simple *buf = PROV_BUF(17);

    /* NOTE: The Bluetooth SIG recommends that potentially vulnerable mesh provisioners
     * restrict the authentication procedure and not accept provisioning random and
     * provisioning confirmation numbers from a remote peer that are the same as those
     * selected by the local device (CVE-2020-26560).
     */
    //本地确认值和对方 确认值要不一样
    if (!memcmp(data, link.local_conf, 16)) {
        BT_ERR("Confirmation value is identical to ours, rejecting.");
        close_link(PROV_ERR_UNEXP_ERR, CLOSE_REASON_FAILED);
        return;
    }


    memcpy(link.conf, data, 16);
	//如果标志位已经有共享密钥，发送认证值标志位也置位
    if (atomic_test_bit(link.flags, HAVE_DHKEY)) {
#if defined(CONFIG_BLE_MESH_PB_ADV)
        prov_clear_tx(idx);
#endif
        atomic_set_bit(link.flags, SEND_CONFIRM);
    }
	//发送随机值
    prov_buf_init(buf, PROV_RANDOM);

    net_buf_simple_add_mem(buf, link.rand, 16);

    if (prov_send(buf)) {
        BT_ERR("Failed to send Provisioning Random");
        close_link(PROV_ERR_CFM_FAILED, CLOSE_REASON_FAILED);
        return;
    }

    link.expect = PROV_RANDOM;
}
//发送配网数据
static void send_prov_data(void)
{
    struct net_buf_simple *buf = PROV_BUF(34);
    uint16_t prev_addr = 0;
    uint16_t max_addr = 0;
    struct bt_mesh_node *node = NULL;
    const uint8_t *netkey = NULL;
    uint8_t session_key[16] = {0};
    uint8_t nonce[13] = {0};
    uint8_t pdu[25] = {0};
    int err = 0;
	//生成会话密钥
    err = bt_mesh_session_key(link.dhkey, link.prov_salt, session_key);
    if (err) {
        BT_ERR("Failed to generate session key");
        goto fail;
    }
    BT_INFO("SessionKey: %s", bt_hex(session_key, 16));
	//生成一次性随机值
    err = bt_mesh_prov_nonce(link.dhkey, link.prov_salt, nonce);
    if (err) {
        BT_ERR("Failed to generate session nonce");
        goto fail;
    }
    BT_INFO("Nonce: %s", bt_hex(nonce, 13));

    /* Assign provisioning data for the device. Currently all provisioned devices
     * will be added to the primary subnet, and may add an API to choose to which
     * subnet will the device be provisioned later.
     */
#if 1
    //获得网络密钥
    netkey = bt_mesh_provisioner_net_key_get(prov_ctx.curr_net_idx);
    BT_INFO("获取网络密钥 netkey: %p",netkey);
    if (!netkey) {
        BT_ERR("No NetKey for provisioning data");
        goto fail;
    }
    memcpy(pdu, netkey, 16);
    sys_put_be16(prov_ctx.curr_net_idx, &pdu[16]);
    pdu[18] = prov_ctx.curr_flags;
    sys_put_be32(prov_ctx.curr_iv_index, &pdu[19]);
    

    /**
     * The Provisioner must not reuse unicast addresses that have been
     * allocated to a device and sent in a Provisioning Data PDU until
     * the Provisioner receives an Unprovisioned Device beacon or
     * Service Data for the Mesh Provisioning Service from that same
     * device, identified using the Device UUID of the device.
     */

    /* 检查此设备是否为重新配置的设备 */
    node = provisioner_find_node_with_uuid(link.uuid,NULL);
    if (node) {
        if (link.element_num <= node->element_num) {
            /**
             * If the device is provisioned before, but the element number of
             * the device is bigger now, then we treat it as a new device.
             */
            prev_addr = node->unicast_addr;
        }
        bt_mesh_provisioner_remove_node(link.uuid);
    }

    max_addr = 0x7FFF;

    if (BLE_MESH_ADDR_IS_UNICAST(prev_addr)) {
        sys_put_be16(prev_addr, &pdu[23]);
        link.unicast_addr = prev_addr;
    } else {
        uint16_t alloc_addr = BLE_MESH_ADDR_UNASSIGNED;

        if (BLE_MESH_ADDR_IS_UNICAST(link.assign_addr)) {
            alloc_addr = link.assign_addr;
        } else {
            /* If this device to be provisioned is a new device */
            if (prov_ctx.curr_alloc_addr == BLE_MESH_ADDR_UNASSIGNED) {
                BT_ERR("Not enough unicast address to be allocated");
                goto fail;
            }
            alloc_addr = prov_ctx.curr_alloc_addr;
        }

        if (alloc_addr + link.element_num - 1 > max_addr) {
            BT_ERR("Not enough unicast address for the device");
            goto fail;
        }
#if 0
        /* Make sure the assigned unicast address is not identical with any unicast
         * address of other nodes. And make sure the address is not identical with
         * any unicast address of Provisioner.
         */
        if (bt_mesh_provisioner_check_is_addr_dup(alloc_addr, link.element_num, true)) {
            BT_ERR("Duplicate assigned address 0x%04x", alloc_addr);
            goto fail;
        }
#endif
        sys_put_be16(alloc_addr, &pdu[23]);
        link.unicast_addr = alloc_addr;
    }
#endif
    prov_buf_init(buf, PROV_DATA);
	//配网数据加密
    err = bt_mesh_prov_encrypt(session_key, nonce, pdu, net_buf_simple_add(buf, 33));
    if (err) {
        BT_ERR("Failed to encrypt provisioning data");
        goto fail;
    }
	//发送加密后的配网数据
    if (prov_send(buf)) {
        BT_ERR("Failed to send Provisioning Data");
        goto fail;
    }

    /**
     * We update the next unicast address to be allocated here because if
     * Provisioner is provisioning two devices at the same time, we need
     * to assign the unicast address for them correctly. Hence we should
     * not update the prov_ctx.curr_alloc_addr after the proper provisioning
     * complete pdu is received.
     */
#if 1
    if (!BLE_MESH_ADDR_IS_UNICAST(prev_addr)) {
        if (BLE_MESH_ADDR_IS_UNICAST(link.assign_addr)) {
            /* Even if the unicast address of the node is assigned by the
             * application, we will also update the prov_ctx.curr_alloc_addr
             * here, in case Users use the two methods together (i.e. allocate
             * the unicast address for the node internally and assign the
             * unicast address for the node from application).
             */
            if (prov_ctx.curr_alloc_addr < link.assign_addr + link.element_num) {
                prov_ctx.curr_alloc_addr = link.assign_addr + link.element_num;
            }
        } else {
            prov_ctx.curr_alloc_addr += link.element_num;
            if (prov_ctx.curr_alloc_addr > max_addr) {
                /* No unicast address will be used for further provisioning */
                prov_ctx.curr_alloc_addr = 0;
            }
        }
#if 0
        /* Store the available unicast address range to flash */
        if (IS_ENABLED(CONFIG_BLE_MESH_SETTINGS)) {
            bt_mesh_store_prov_info(prov_ctx.primary_addr, prov_ctx.curr_alloc_addr);
        }
#endif
    }


    link.ki_flags = prov_ctx.curr_flags;
    link.iv_index = prov_ctx.curr_iv_index;
    
#endif
    link.expect = PROV_COMPLETE;
    return;

fail:
    close_link(PROV_ERR_UNEXP_ERR, CLOSE_REASON_FAILED);
    return;
}


//接收到对方随机值进入
static void prov_random(const u8_t *data)
{
    uint8_t conf_verify[16] = {0};
    //双方随机值比较 不一样才对 
    if (!memcmp(data, link.rand, 16)) {
        BT_ERR("Random value is identical to ours, rejecting.");
        goto fail;
    }
	//通过未配网设备发来的随机数生成确认值
    if (bt_mesh_prov_conf(link.conf_key, data, link.auth, conf_verify)) {
        BT_ERR("Failed to calculate confirmation verification");
        goto fail;
    }
	//拿未配网设备发来的确认值和配网器通过随机数生成的确认值是否相等
    if (memcmp(conf_verify, link.conf, 16)) {
        BT_ERR("Invalid confirmation value");
        BT_INFO("Received:   %s", bt_hex(link.conf, 16));
        BT_INFO("Calculated: %s",  bt_hex(conf_verify, 16));
        goto fail;
    }



    /** After provisioner receives provisioning random from device,
     *  and successfully check the confirmation, the following
     *  should be done:
     *  1. bt_mesh_calloc memory for prov_salt
     *  2. calculate prov_salt
     *  3. prepare provisioning data and send
     */
    //生成配网佐料 -> link.prov_salt
    if (bt_mesh_prov_salt(link.conf_salt, link.rand, data,link.prov_salt)) {
        BT_ERR("Failed to generate ProvisioningSalt");
        goto fail;
    }

    BT_INFO("ProvisioningSalt: %s", bt_hex(link.prov_salt, 16));
	//发送配网数据
    send_prov_data();
    return;

fail:
    close_link(PROV_ERR_UNEXP_ERR, CLOSE_REASON_FAILED);
    return;
}

static void prov_data(const u8_t *data)
{
    printf("prov_data\r\n");
}
//收到配网完成进入
static void prov_complete(const u8_t *data)
{
    uint8_t device_key[16] = {0};
    uint16_t net_idx = 0U;
    //uint16_t index = 0U;
    int err = 0;
    //int i;


    //设备密钥
    err = bt_mesh_dev_key(link.dhkey, link.prov_salt, device_key);
    if (err) {
        BT_ERR("Failed to generate device key");
        close_link(PROV_ERR_NONE, CLOSE_REASON_FAILED);
        return;
    }


    net_idx = prov_ctx.curr_net_idx;

    //保存节点数据
    /*
    err = bt_mesh_provisioner_provision(&link[idx].addr, link[idx].uuid, link[idx].oob_info,
                                        link[idx].unicast_addr, link[idx].element_num, net_idx,
                                        link[idx].ki_flags, link[idx].iv_index, device_key, &index);


    if (prov->prov_complete) {
        prov->prov_complete(index, link[idx].uuid, link[idx].unicast_addr,
                            link[idx].element_num, net_idx);
    }


    for (i = 0; i < ARRAY_SIZE(unprov_dev); i++) {
        if (!memcmp(unprov_dev[i].uuid, link[idx].uuid, 16) &&
            (unprov_dev[i].flags & RM_AFTER_PROV)) {
            memset(&unprov_dev[i], 0, sizeof(struct unprov_dev_queue));
            break;
        }
    }
    */
    close_link(PROV_ERR_NONE, CLOSE_REASON_SUCCESS);
}

static void prov_failed(const u8_t *data)
{
    BT_WARN("Error: 0x%02x", data[0]);
    close_link(PROV_ERR_NONE, CLOSE_REASON_FAILED);
}

static const struct {
    void (*func)(const u8_t *data);
    u16_t len;
} prov_handlers[] = {
    { prov_invite, 1 },
    { prov_capabilities, 11 },
    { prov_start, 5, },
    { prov_pub_key, 64 },
    { prov_input_complete, 0 },
    { prov_confirm, 16 },
    { prov_random, 16 },
    { prov_data, 33 },
    { prov_complete, 0 },
    { prov_failed, 1 },
};

static void close_link(u8_t err, u8_t reason)
{
    //PROV_D(", 10--->0");
    BT_INFO("close link 10--->0");
#if defined(CONFIG_BT_MESH_PB_GATT)
    if (link.conn) {
        bt_mesh_pb_gatt_close(link.conn);
        return;
    }
#endif

#if defined(CONFIG_BT_MESH_PB_ADV)
    if (err) {
        prov_send_fail_msg(err);
    }

    link.rx.seg = 0;
    bearer_ctl_send(LINK_CLOSE, &reason, sizeof(reason));
#endif

    atomic_clear_bit(link.flags, LINK_ACTIVE);

    /* Disable Attention Timer if it was set */
    if (link.conf_inputs[0]) {
        bt_mesh_attention(NULL, 0);
    }
}

#if defined(CONFIG_BT_MESH_PB_ADV)
static void prov_retransmit(struct k_work *work)
{
    int i;

    printf("");

    if (!atomic_test_bit(link.flags, LINK_ACTIVE)) {
        BT_WARN("Link not active");
        return;
    }

    if (k_uptime_get() - link.tx.start > TRANSACTION_TIMEOUT) {
        BT_WARN("Giving up transaction");
        reset_link();
        return;
    }

    if (link.send_link_close & BIT(0)) {
        //uint8_t reason = (link.send_link_close >> 1) & BIT_MASK(2);
        uint16_t count = (link.send_link_close >> 3);
        if (count >= 2) {
            reset_link();
            return;
        }
        link.send_link_close += BIT(3);
    }

    for (i = 0; i < ARRAY_SIZE(link.tx.buf); i++) {
        struct net_buf *buf = link.tx.buf[i];

        if (!buf) {
            break;
        }

        if (BT_MESH_ADV(buf)->busy) {
            continue;
        }

        printf("%u bytes: %s\r\n", buf->len, bt_hex(buf->data, buf->len));

        if (i + 1 < ARRAY_SIZE(link.tx.buf) && link.tx.buf[i + 1]) {
            bt_mesh_adv_send(buf, NULL, NULL);
        } else {
            bt_mesh_adv_send(buf, &buf_sent_cb, NULL);
        }

    }
}



static void link_ack(struct prov_rx *rx, struct net_buf_simple *buf)
{

    if (buf->len) {
        BT_ERR("Invalid Link ACK length %d", buf->len);
        close_link(PROV_ERR_NVAL_PDU, CLOSE_REASON_FAILED);
        return;
    }

    if (link.expect == PROV_CAPABILITIES) {
        BT_INFO("Link ACK is already received");
        return;
    }
    //发送邀请
    send_invite();
}

static void link_close(struct prov_rx *rx, struct net_buf_simple *buf)
{
    //printf("len %u", buf->len);
    printf("关闭连接\r\n");
    reset_link();
}

static void gen_prov_ctl(struct prov_rx *rx, struct net_buf_simple *buf)
{
    BT_INFO("配网器接收到: op 0x%02x len %u", BEARER_CTL(rx->gpc), buf->len);

    switch (BEARER_CTL(rx->gpc)) {
        case LINK_OPEN:

            break;
        case LINK_ACK:
            if (!atomic_test_bit(link.flags, LINK_ACTIVE)) {
                return;
            }

            link_ack(rx, buf);
            break;
        case LINK_CLOSE:
            if (!atomic_test_bit(link.flags, LINK_ACTIVE)) {
                return;
            }

            link_close(rx, buf);
            break;
        default:
            BT_ERR("Unknown bearer opcode: 0x%02x", BEARER_CTL(rx->gpc));
            return;
    }
}

static void prov_msg_recv(void)
{
    u8_t type = link.rx.buf->data[0];

    BT_INFO("type 0x%02x len %u", type, link.rx.buf->len);
    BT_INFO("data %s", bt_hex(link.rx.buf->data, link.rx.buf->len));

    if (!bt_mesh_fcs_check(link.rx.buf, link.rx.fcs)) {
        BT_ERR("Incorrect FCS");
        return;
    }

    if (type != PROV_FAILED && type != link.expect) {
        BT_WARN("Unexpected msg 0x%02x != 0x%02x", type, link.expect);
        prov_send_fail_msg(PROV_ERR_UNEXP_PDU);
        /* added for NODE/PROV/BV-10-C in PTS 7.4.1*/
        link.expect = PROV_FAILED;
        return;
    }

    if (type >= ARRAY_SIZE(prov_handlers)) {
        BT_ERR("Unknown provisioning PDU type 0x%02x", type);
        close_link(PROV_ERR_NVAL_PDU, CLOSE_REASON_FAILED);
        return;
    }

    if (1 + prov_handlers[type].len != link.rx.buf->len) {
        BT_ERR("Invalid length %u for type 0x%02x",
               link.rx.buf->len, type);
        close_link(PROV_ERR_NVAL_FMT, CLOSE_REASON_FAILED);
        return;
    }

    gen_prov_ack_send(link.rx.id);
    link.rx.prev_id = link.rx.id;
    link.rx.id = 0;


    prov_handlers[type].func(&link.rx.buf->data[1]);
}

static void gen_prov_cont(struct prov_rx *rx, struct net_buf_simple *buf)
{
    u8_t seg = CONT_SEG_INDEX(rx->gpc);

    //printf("分包len %u, seg_index %u\r\n", buf->len, seg);

    if (!link.rx.seg && link.rx.prev_id == rx->xact_id) {
        BT_DBG("Resending ack");
        gen_prov_ack_send(rx->xact_id);
        return;
    }

    if (rx->xact_id != link.rx.id) {
        BT_WARN("Data for unknown transaction (%u != %u)",
                rx->xact_id, link.rx.id);
        return;
    }

    if (seg > link.rx.last_seg) {
        BT_ERR("Invalid segment index %u", seg);
        close_link(PROV_ERR_NVAL_FMT, CLOSE_REASON_FAILED);
        return;
    } else if (seg == link.rx.last_seg) {
        u8_t expect_len;

        expect_len = (link.rx.buf->len - 20 -
                      (23 * (link.rx.last_seg - 1)));
        if (expect_len != buf->len) {
            BT_ERR("Incorrect last seg len: %u != %u",
                   expect_len, buf->len);
            close_link(PROV_ERR_NVAL_FMT, CLOSE_REASON_FAILED);
            return;
        }
    }

    if (!(link.rx.seg & MESH_BIT(seg))) {
        BT_DBG("Ignoring already received segment");
        return;
    }

    memcpy(XACT_SEG_DATA(seg), buf->data, buf->len);
    XACT_SEG_RECV(seg);

    if (!link.rx.seg) {
        prov_msg_recv();
    }
}

static void gen_prov_ack(struct prov_rx *rx, struct net_buf_simple *buf)
{
    //printf("应答包\r\n");
    uint8_t ack_type = 0U, pub_key_oob = 0U;

    BT_DBG("len %u", buf->len);

    if (!link.tx.buf[0]) {
        return;
    }

    if (!link.tx.id) {
        return;
    }

    if (rx->xact_id == (link.tx.id - 1)) {
        prov_clear_tx();

        ack_type = link.expect_ack_for;
        switch (ack_type) {
        case PROV_START:
            pub_key_oob = link.conf_inputs[13];
            send_pub_key(pub_key_oob);
            break;
        case PROV_PUB_KEY:
            prov_gen_dh_key();
            break;
        default:
            break;
        }
        link.expect_ack_for = 0x00;
    }
}

static void gen_prov_start(struct prov_rx *rx, struct net_buf_simple *buf)
{
    if (link.rx.seg) {
        BT_DBG("Got Start while there are unreceived segments");
        return;
    }

    if (link.rx.prev_id == rx->xact_id) {
        BT_DBG("Resending ack");
        gen_prov_ack_send(rx->xact_id);
        return;
    }

    link.rx.buf->len = net_buf_simple_pull_be16(buf);
    link.rx.id  = rx->xact_id;
    link.rx.fcs = net_buf_simple_pull_u8(buf);

    BT_INFO("len %u last_seg %u total_len %u fcs 0x%02x", buf->len,
           START_LAST_SEG(rx->gpc), link.rx.buf->len, link.rx.fcs);

    if (link.rx.buf->len < 1) {
        BT_ERR("Ignoring zero-length provisioning PDU");
        close_link(PROV_ERR_NVAL_FMT, CLOSE_REASON_FAILED);
        return;
    }

    if (link.rx.buf->len > link.rx.buf->size) {
        BT_ERR("Too large provisioning PDU (%u bytes)(%u size)",
               link.rx.buf->len,link.rx.buf->size);
        close_link(PROV_ERR_NVAL_FMT, CLOSE_REASON_FAILED);
        return;
    }

    if (START_LAST_SEG(rx->gpc) > 0 && link.rx.buf->len <= 20) {
        BT_ERR("Too small total length for multi-segment PDU");
        close_link(PROV_ERR_NVAL_FMT, CLOSE_REASON_FAILED);
        return;
    }

    link.rx.seg = (1 << (START_LAST_SEG(rx->gpc) + 1)) - 1;
    link.rx.last_seg = START_LAST_SEG(rx->gpc);
    memcpy(link.rx.buf->data, buf->data, buf->len);
    XACT_SEG_RECV(0);

    if (!link.rx.seg) {
        prov_msg_recv();
    }
}

static const struct {
    void (*const func)(struct prov_rx *rx, struct net_buf_simple *buf);
    const u8_t require_link;
    const u8_t min_len;
} gen_prov[] = {
    { gen_prov_start, true, 3 },
    { gen_prov_ack, true, 0 },
    { gen_prov_cont, true, 0 },
    { gen_prov_ctl, true, 0 },
};

static void gen_prov_recv(struct prov_rx *rx, struct net_buf_simple *buf)
{
    //printf("GPCF(rx->gpc) = %d\r\n",GPCF(rx->gpc));
    if (buf->len < gen_prov[GPCF(rx->gpc)].min_len) {
        BT_ERR("Too short GPC message type %u", GPCF(rx->gpc));
        return;
    }

    if (!atomic_test_bit(link.flags, LINK_ACTIVE) &&
        gen_prov[GPCF(rx->gpc)].require_link) {
        //BT_INFO("Ignoring message that requires active link");
        return;
    }
    
    gen_prov[GPCF(rx->gpc)].func(rx, buf);
}

void bt_mesh_prov_adv_recv(struct net_buf_simple *buf)
{
    struct prov_rx rx;

    rx.link_id = net_buf_simple_pull_be32(buf);


    if (buf->len < 2) {
        BT_WARN("Too short provisioning packet (len %u)", buf->len);
        close_link(PROV_ERR_NVAL_PDU, CLOSE_REASON_FAILED);
        return;
    }

    rx.xact_id = net_buf_simple_pull_u8(buf);
    rx.gpc = net_buf_simple_pull_u8(buf);

    gen_prov_recv(&rx, buf);

}
#endif /* CONFIG_BT_MESH_PB_ADV */


int bt_prov_active(void)
{
    return atomic_test_bit(link.flags, LINK_ACTIVE);
}

void bt_mesh_prov_reset(void)
{
    if (prov->reset) {
        prov->reset();
    }
}

int net_key_init(void)
{
    
    //初始化设备密钥，这里用三元组的 secret 作为配网器设备密钥
    stringtohex(DEFAULT_SECRET, provisoner_devkey, 16);
    bt_mesh_rand(&g_netkey.key[0],16);
    seq = 0;
    addr = 0;
    g_netkey.net_index = 0;
    g_netkey.flag = 0;
    g_netkey.ivi = 0;
    BT_INFO("g_netkey:%s", bt_hex(g_netkey.key,16));
    bt_mesh_provision(g_netkey.key, g_netkey.net_index, g_netkey.flag, g_netkey.ivi, seq, addr, provisoner_devkey);


    /* Dynamically added appkey & netkey will use these key_idx */
    //下一个APP索引和NET索引
    bt_mesh.p_app_idx_next = 0x0000;
    bt_mesh.p_net_idx_next = 0x0001;

    //BT_INFO("netkey:%s", bt_hex(netkey.key,16));
    //初始化应用密钥
    bt_mesh_rand(&appkey.key[0],16);
    appkey.net_index = 0;
    appkey.key_index = 0;
    appkey.flag = 0;

    extern void genie_appkey_register(u16_t net_idx, u16_t app_idx, const u8_t val[16], bool update);
    genie_appkey_register(appkey.net_index, appkey.key_index, appkey.key, appkey.flag);
    BT_INFO("appkey:%s", bt_hex(appkey.key,16));

    return 0;
}


int is_provisioning(void)
{

    return atomic_test_bit(link.flags, LINK_ACTIVE);
}
//通过网络索引获取网络密钥
struct bt_mesh_subnet *bt_mesh_provisioner_subnet_get(uint16_t net_idx)
{
    struct bt_mesh_subnet *sub = NULL;
    int i;

    BT_DBG("%s", __func__);

    if (net_idx == BLE_MESH_KEY_ANY) {
        return bt_mesh.p_sub[0];
    }

    for (i = 0; i < ARRAY_SIZE(bt_mesh.p_sub); i++) {
        sub = bt_mesh.p_sub[i];
        if (sub && sub->net_idx == net_idx) {
            return sub;
        }
    }

    return NULL;
}
//初始化配网器配网信息
int bt_mesh_provisioner_init_prov_info(void)
{
    if (prov_ctx.primary_addr == BLE_MESH_ADDR_UNASSIGNED) {
        /* If unicast address of primary element of Provisioner has not been set
         * before, then the following initialization procedure will be used.
         */
        if (prov == NULL) {
            BT_ERR("No provisioning context provided");
            return -EINVAL;
        }

        if (!BLE_MESH_ADDR_IS_UNICAST(prov->prov_unicast_addr) ||
            !BLE_MESH_ADDR_IS_UNICAST(prov->prov_start_address)) {
            BT_ERR("Invalid address, own 0x%04x, start 0x%04x",
                    prov->prov_unicast_addr, prov->prov_start_address);
            return -EINVAL;
        }
		//获取成分数据
        const struct bt_mesh_comp *comp = bt_mesh_comp_get();
        if (!comp) {
            BT_ERR("Invalid composition data");
            return -EINVAL;
        }

        if (prov->prov_unicast_addr + comp->elem_count > prov->prov_start_address) {
            BT_WARN("Too small start address 0x%04x, update to 0x%04x",
                prov->prov_start_address, prov->prov_unicast_addr + comp->elem_count);
            prov_ctx.curr_alloc_addr = prov->prov_unicast_addr + comp->elem_count;
        } else {
            prov_ctx.curr_alloc_addr = prov->prov_start_address;
        }

        /* Update primary element address with the initialized value here. */
        prov_ctx.primary_addr = prov->prov_unicast_addr;

        //if (IS_ENABLED(CONFIG_BLE_MESH_SETTINGS)) {
        //    bt_mesh_store_prov_info(prov_ctx.primary_addr, prov_ctx.curr_alloc_addr);
        //}
    }

    prov_ctx.curr_net_idx = BLE_MESH_KEY_PRIMARY;
    struct bt_mesh_subnet *sub = bt_mesh_provisioner_subnet_get(BLE_MESH_KEY_PRIMARY);
    prov_ctx.curr_flags = bt_mesh_net_flags(sub);
    prov_ctx.curr_iv_index = bt_mesh.iv_index;

    return 0;
}
//配网器初始化
int bt_mesh_provisioner_prov_init(const struct bt_mesh_prov *prov_info)
{
    BT_INFO("配网器初始化");
    const uint8_t *key = NULL;

    if (!prov_info) {
        BT_ERR("No provisioning context provided");
        return -EINVAL;
    }
    //获得公钥
    key = bt_mesh_pub_key_get();
    if (!key) {
        BT_ERR("Failed to generate Public Key");
        return -EIO;
    }

    prov = prov_info;

    prov_ctx.primary_addr = BLE_MESH_ADDR_UNASSIGNED;

    if (prov->static_val && prov->static_val_len) {
        prov_ctx.static_oob_len = prov->static_val_len > 16 ? 16 : prov->static_val_len;
        memcpy(prov_ctx.static_oob_val, prov->static_val, prov_ctx.static_oob_len);
    }
	//网络密钥初始化
    net_key_init();
    //配网器配网信息初始化
    bt_mesh_provisioner_init_prov_info();
#if defined(CONFIG_BT_MESH_PB_ADV)

    //struct prov_adv_buf *adv = &adv_buf;
    //adv->buf.size = ADV_BUF_SIZE;
    //adv->buf.__buf = adv_buf_data + ADV_BUF_SIZE;
	//初始化重传计数器
    k_delayed_work_init(&link.tx.retransmit, prov_retransmit);
	//初始化上次接收ID和接收缓存
    link.rx.prev_id = XACT_NVAL;
    link.rx.buf = bt_mesh_pba_get_buf();
    
#endif/*CONFIG_BT_MESH_PB_ADV*/


    return 0;
}


#endif
