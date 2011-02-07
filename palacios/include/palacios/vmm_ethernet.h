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
 * Author: Lei Xia <lxia@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __ETHERNET_H__
#define __ETHERNET_H__

#define ETHERNET_HEADER_LEN 14
#define ETHERNET_MTU   1500
#define ETHERNET_PACKET_LEN (ETHERNET_HEADER_LEN + ETHERNET_MTU)
#define ETH_ALEN 6


#ifdef __V3VEE__

#include <palacios/vmm.h>

static inline int is_multicast_ethaddr(const uint8_t * addr)
{
    V3_ASSERT(ETH_ALEN == 6);
	
    return (0x01 & addr[0]);
}

static inline int is_broadcast_ethaddr(const uint8_t * addr)
{
    V3_ASSERT(ETH_ALEN == 6);
	
    return (addr[0] & addr[1] & addr[2] & addr[3] & addr[4] & addr[5]) == 0xff;
}


static inline int compare_ethaddr(const uint8_t * addr1, const uint8_t * addr2)
{
    const uint16_t *a = (const uint16_t *) addr1;
    const uint16_t *b = (const uint16_t *) addr2;

    V3_ASSERT(ETH_ALEN == 6);
    return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) != 0;
}


static inline int compare_ether_hdr(const uint8_t * hdr1, const uint8_t * hdr2)
{
    uint32_t *a32 = (uint32_t *)(hdr1 + 2);
    uint32_t *b32 = (uint32_t *)(hdr2 + 2);

    V3_ASSERT(ETHERNET_HEADER_LEN == 14);

    return (*(uint16_t *)hdr1 ^ *(uint16_t *)hdr2) | (a32[0] ^ b32[0]) |
             (a32[1] ^ b32[1]) | (a32[2] ^ b32[2]);
}

/* AA:BB:CC:DD:EE:FF */
static inline int str2mac(char * macstr, uint8_t * mac){
    char hex[2], *s = macstr;
    int i = 0;

    while(s){
	memcpy(hex, s, 2);
	mac[i++] = (char)atox(hex);	
	if (i == ETH_ALEN) return 0;
	s=strchr(s, ':');
	if(s) s++;
    }

    return -1;
}


/* generate random ethernet address */
static inline void random_ethaddr(uint8_t * addr)
{
    uint64_t val;

    /* using current rdtsc as random number */
    rdtscll(val);
    *(uint64_t *)addr = val;
	
    addr [0] &= 0xfe;	/* clear multicast bit */
    addr [0] |= 0x02;	/* set local assignment bit (IEEE802) */
}


#endif

#endif


