/*
 * Palacios VM Raw Packet interface
 * (c) Lei Xia, 2010
 */

#ifndef __PALACIOS_PACKET_H__
#define __PALACIOS_PACKET_H__

int palacios_init_packet(const char * eth_dev);
int palacios_deinit_packet(void);

#endif
