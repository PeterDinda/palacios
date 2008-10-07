/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */


#include <geekos/net.h>
#include <geekos/socket.h>
#include <geekos/ne2k.h>

#include <geekos/debug.h>

#ifdef LWIP

#include <lwip/apps/ping.h>
#include <lwip/lwip/sockets.h>
#include <lwip/ipv4/lwip/ip_addr.h>
#include <lwip/netif/ne2kif.h>
#include <lwip/sys.h>
#include <lwip/netifapi.h>
#include <lwip/tcpip.h>

#include <netif/etharp.h>


static void
tcpip_init_done(void *arg)
{
  sys_sem_t *sem;
  sem = arg;
  sys_sem_signal(*sem);
}

#endif

void Init_Network() {

  //temporay now we are using lwip sockets
  // init_socket_layer();

#ifdef LWIP
  
  struct ip_addr ipaddr, netmask, gateway;
  sys_sem_t sem;
  err_t err;

  sem = sys_sem_new(0);

#ifdef LWIP_DEBUG
  PrintBoth("lwIP: before tcpip_init\n");
#endif

  tcpip_init(tcpip_init_done, &sem);  //initial the whole lwip module

#ifdef LWIP_DEBUG
  PrintBoth("lwIP: After tcpip_init\n");
#endif

  sys_sem_wait(sem);
  sys_sem_free(sem);
 
  IP4_ADDR(&gateway, 192,168,1,1);
  IP4_ADDR(&ipaddr, 192,168,1,2);
  IP4_ADDR(&netmask, 255,255,255,0);


  err = netifapi_netif_add(&ne2kif, &ipaddr, &netmask, &gateway, 
  						NULL, ne2kif_init, ethernet_input); 
  
  if (err != ERR_OK){
		PrintBoth("lwip: initial network failed! add netif error %d/n", err);
		return;
  }
  
  netifapi_netif_set_default(&ne2kif);

  //initial a network application
  ping_init();

#endif

}




#if 0
void test_network() {

    uchar_t local_addr[4];
    uchar_t remote_addr[4];

    local_addr[0] = 10;
    local_addr[1] = 0;
    local_addr[2] = 2;
    local_addr[3] = 21;

//    set_ip_addr(local_addr);

    remote_addr[0] = 10;
    remote_addr[1] = 0;
    remote_addr[2] = 2;
    remote_addr[3] = 20;


   // connect(remote_addr, 4301);

}

#endif
