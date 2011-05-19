/*
 * Palacios VNET Host Bridge
 * (c) Lei Xia, 2010
 */

#ifndef __PALACIOS_VNET_BRIDGE_H__
#define __PALACIOS_VNET_BRIDGE_H__

int  palacios_vnet_init(void);
int  palacios_init_vnet_bridge(void);

void palacios_vnet_deinit(void);
void palacios_deinit_vnet_bridge(void);

#endif

