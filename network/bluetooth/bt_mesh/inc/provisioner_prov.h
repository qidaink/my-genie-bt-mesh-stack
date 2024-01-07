/*  Bluetooth Mesh */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __PROVISIONER_PROV_H
#define __PROVISIONER_PROV_H
#ifdef CONFIG_BT_MESH_ROLE_PROVISIONER
#include <net/buf.h>
#include "mesh.h"

void provisioner_link_open(const uint8_t * uuid);
int is_provisioning(void);
int bt_prov_active(void);
void bt_mesh_prov_adv_recv(struct net_buf_simple *buf);
int bt_mesh_provisioner_prov_init(const struct bt_mesh_prov *prov_info);
#endif
#endif

