/**
 * @file
 * Ethernet Interface for ne2k device
 *
 * Lei Xia (lxia@northwestern.edu
 */

#include "lwip/opt.h"
 

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include "netif/etharp.h"
#include "netif/ppp_oe.h"
#include "lwip/netifapi.h"
#include "netif/ne2kif.h"


#include <geekos/ne2k.h>
#include <geekos/ktypes.h>
#include <geekos/debug.h>


struct netif ne2kif; 

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

err_t
ne2kif_output(struct netif *netif, struct pbuf *p)
{
  struct pbuf *q;
  int size, offset, remlen;
  uchar_t *packet;
  
#if ETH_PAD_SIZE
  pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

  size = p->tot_len;
  offset = 0;
  remlen = size;

  packet = (uchar_t *)Malloc (remlen);

  for(q = p; q != NULL && remlen >0; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */

	memcpy(packet+offset, q->payload, q->len < remlen? q->len:remlen);
	remlen -= q->len;
	offset += q->len;
    
  }
  NE2K_Send_Packet(packet, size);

  //signal that packet should be sent();

#if ETH_PAD_SIZE
  pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif
  
  LINK_STATS_INC(link.xmit);

  return ERR_OK;
}


/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
void
ne2kif_input(struct NE2K_Packet_Info * info, uchar_t * pkt)
{
  struct pbuf *p, *q;
  uint_t offset;
  uint_t len, rlen;
  
  len = info->size; 

  PrintBoth("Ne2k: Packet REceived\n");

#if ETH_PAD_SIZE
  len += ETH_PAD_SIZE; // allow room for Ethernet padding  
#endif

  p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
  
  if (p != NULL) {

#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE); // drop the padding word 
#endif

    // iterate over the pbuf chain until it has read the entire packet into the pbuf.
    for(offset = 0, q = p; q != NULL && rlen > 0; q = q->next) {
      memcpy(q->payload, pkt+offset, rlen > q->len? q->len: rlen);
      rlen -= q->len;
      offset += q->len;
    }

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

    LINK_STATS_INC(link.recv);
  } else {
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
  }

  Free(pkt);
  
  if (p == NULL) return;
  /* points to packet payload, which starts with an Ethernet header */
  //ethhdr = p->payload;

  /* full packet send to tcpip_thread to process */
  if (ne2kif.input(p, &ne2kif)!=ERR_OK)
     { LWIP_DEBUGF(NETIF_DEBUG, ("ne2kif_input: IP input error\n"));
       pbuf_free(p);
       p = NULL;
     }

#if 0
  switch (htons(ethhdr->type)) {
  /* IP or ARP packet? */
  case ETHTYPE_IP:
  case ETHTYPE_ARP:
#if PPPOE_SUPPORT
  /* PPPoE packet? */
  case ETHTYPE_PPPOEDISC:
  case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
    
    break;

  default:
    pbuf_free(p);
    p = NULL;
    break;
  }
#endif

}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
ne2kif_init(struct netif *netif)
{

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  /*
   * Initialize the snmp variables and counters inside the struct netif.
   * The last argument should be replaced with your link speed, in units
   * of bits per second.
   */
  NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 10000000);

  netif->state = NULL;
  netif->name[0] = 'n';
  netif->name[1] = 'e';
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
  netif->output = etharp_output;
  netif->linkoutput = ne2kif_output;
  

  /* set MAC hardware address length */
  netif->hwaddr_len = ETHARP_HWADDR_LEN;

  /* set MAC hardware address */
  netif->hwaddr[0] = PHY_ADDR1;
  netif->hwaddr[1] = PHY_ADDR2;
  netif->hwaddr[2] = PHY_ADDR3;
  netif->hwaddr[3] = PHY_ADDR4;
  netif->hwaddr[4] = PHY_ADDR5;
  netif->hwaddr[5] = PHY_ADDR6;

  /* maximum transfer unit */
  netif->mtu = 1500;
  
  /* device capabilities */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

  //initate ne2k hardware
  Init_Ne2k(&ne2kif_input);

  return ERR_OK;
}
