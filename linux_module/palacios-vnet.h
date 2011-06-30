/*
 * Lei Xia 2010
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

