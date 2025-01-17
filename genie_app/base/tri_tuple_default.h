/*
 * Copyright (C) 2018-2020 Alibaba Group Holding Limited
 */

#ifndef TRI_TUPLE_DEFAULT_H_
#define TRI_TUPLE_DEFAULT_H_


#if defined(BOARD_TG7100B) || defined(BOARD_CH6121EVB)
/* default UUID for identifying the unprovisioned node */
#define DEFAULT_PID 10857
#define DEFAULT_SECRET "7f5a348ad47baac74e48b8d6e980cb83"
#define DEFAULT_MAC "f8a7638ca646"
#else

//UUID是来自于MAC地址，所以这里最好配网器和节点也不同
#ifdef CONFIG_BT_MESH_ROLE_PROVISIONER
#define DEFAULT_PID    31218019                             // product_id
#define DEFAULT_SECRET "4f4e8a0d7b1daecb3d5f93b677bbe395"   // device_secret
#define DEFAULT_MAC    "74f0ad09cf5c"                       // device_name
#else
/* default UUID for identifying the unprovisioned node */
// 602 1565d0497400c999e984cffa4da4fdf9 78da07c11ca8
#define DEFAULT_PID    21218019                             // product_id
#define DEFAULT_SECRET "3f4e8a0d7b1daecb3d5f93b677bbe395"   // device_secret
#define DEFAULT_MAC    "64f0ad09cf5c"                       // device_name
#endif

#endif
#if 0
#define DEFAULT_PID 761
#define DEFAULT_SECRET "8a99315d87da5d24db777cb7a0f9d687"
#define DEFAULT_MAC { 0xd2, 0x0a, 0x02, 0x3a, 0x9e, 0x10 }
#endif

#endif // TRI_TUPLE_DEFAULT_H_
