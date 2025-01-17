/*  Bluetooth Mesh */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __PROV_H
#define __PROV_H

#include <mesh_def.h>

void bt_mesh_role_node_test(void);
void bt_mesh_pb_adv_recv(struct net_buf_simple *buf);

bool bt_prov_active(void);

int bt_mesh_pb_gatt_open(bt_mesh_conn_t conn);
int bt_mesh_pb_gatt_close(bt_mesh_conn_t conn);
int bt_mesh_pb_gatt_recv(bt_mesh_conn_t conn, struct net_buf_simple *buf);

const u8_t *bt_mesh_prov_get_uuid(void);

int bt_mesh_prov_init(const struct bt_mesh_prov *prov);

void bt_mesh_prov_complete(u16_t net_idx, u16_t addr);
void bt_mesh_prov_reset(void);
void bt_mesh_prov_reset_link(void);

const struct bt_mesh_prov *bt_mesh_prov_get(void);

#ifdef GENIE_ULTRA_PROV
void ultra_prov_free(void);
void ultra_prov_recv_random(uint8_t *buf);
void ultra_prov_recv_prov_data(uint8_t *buf);
uint8_t check_ultra_prov_adv(uint8_t *data);
#endif


#endif

