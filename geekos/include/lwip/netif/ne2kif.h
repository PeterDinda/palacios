/* ne2k network interface for lwip
  *
  * Lei Xia (lxia@northwestern.edu)
  */

#ifndef __NETIF_NE2KIF_H__
#define __NETIF_NE2KIF_H__

#include <lwip/lwip/netif.h>
#include <lwip/lwip/err.h>
#include <geekos/ktypes.h>
#include <geekos/ne2k.h>

extern struct netif ne2kif;

err_t ne2kif_init(struct netif *netif);

void ne2kif_input(struct NE2K_Packet_Info * info, uchar_t * pkt);

err_t ne2kif_output(struct netif * netif, struct pbuf * p);

#endif /*__NETIF_NE2KIF_H__*/
