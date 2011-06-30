/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it under the terms of the GNU General Public License
 * Version 2 (GPLv2).  The accompanying COPYING file contains the
 * full text of the license.
 */

#ifndef __PALACIOS_VNET_H__
#define __PALACIOS_VNET_H__

#include <vnet/vnet.h>


typedef enum {UDP, TCP, RAW, NONE} vnet_brg_proto_t;

struct vnet_brg_stats{
    uint64_t pkt_from_vmm;
    uint64_t pkt_to_vmm;
    uint64_t pkt_drop_vmm;
    uint64_t pkt_from_phy;
    uint64_t pkt_to_phy;
    uint64_t pkt_drop_phy;
};

void vnet_brg_delete_link(uint32_t idx);
uint32_t vnet_brg_add_link(uint32_t ip, uint16_t port, vnet_brg_proto_t proto);
int vnet_brg_link_stats(uint32_t link_idx, struct nic_statistics * stats);
int vnet_brg_stats(struct vnet_brg_stats * stats);


int  vnet_bridge_init(void);
void vnet_bridge_deinit(void);


int vnet_ctrl_init(void);
void vnet_ctrl_deinit(void);


#endif

