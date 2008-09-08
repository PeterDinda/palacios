#include <geekos/socket.h>
#include <geekos/malloc.h>
#include <palacios/vmm_types.h>
#include <geekos/ne2k.h>
#include <uip/uip.h>
#include <uip/uip_arp.h>
#include <geekos/int.h>
#include <geekos/vmm_stubs.h>
#include <geekos/queue.h>

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

#define MAX_SOCKS 1024

struct socket sockets[MAX_SOCKS];


int Packet_Received(struct NE2K_Packet_Info* info, uchar_t *pkt) ;

void init_network() {
   int i = 0;
   
   for (i = 0; i < MAX_SOCKS; i++) {
     sockets[i].in_use = 0;
     init_ring_buffer(&(sockets[i].send_queue), 65535);
     init_ring_buffer(&(sockets[i].recv_queue), 65535);
   }
   
   //initiate uIP
   uip_init();
   uip_arp_init();
   
   // set up interrupt handler
   // set up device driver
   
   Init_Ne2k(&Packet_Received);
   
   
}

static int allocate_socket_fd() {
  int i = 0;
  
  for (i = 0; i < MAX_SOCKS; i++) {
    if (sockets[i].in_use == 0) {
      sockets[i].in_use = 1;
      return i;
    }
  }

  return -1;
}

static int release_socket_fd(int sockfd){
  if ((sockfd >= 0) && (sockfd < MAX_SOCKS)) {
    sockets[sockfd].in_use = 0;
  }
  
  return 0;
}


struct socket * get_socket_from_fd(int fd) {
  return &(sockets[fd]);
}


int connect(const uchar_t ip_addr[4], ushort_t port) {
  int sockfd = -1;
  uip_ipaddr_t ipaddr;

  sockfd = allocate_socket_fd();
  if (sockfd == -1) {
    return -1;
  }

  uip_ipaddr(&ipaddr, ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);  
  
  sockets[sockfd].con = uip_connect((uip_ipaddr_t *)&ip_addr, htons(port));

  if (sockets[sockfd].con == NULL){
    release_socket_fd(sockfd);
    return -1;
  }

  return sockfd;
}


void timer_int_Handler(struct Interrupt_State * state){
  int i;
  //handle the periodic calls of uIP

  for(i = 0; i < UIP_CONNS; ++i) {
    uip_periodic(i);

    if(uip_len > 0) {
      NE2K_Transmit(uip_len);
    }
  }


  for(i = 0; i < UIP_UDP_CONNS; i++) {
    uip_udp_periodic(i);

    if(uip_len > 0) {
      NE2K_Transmit(uip_len);
    }
  }
}

// a series of utilities to handle conncetion states
static void connected(int sockfd){

}

static void closed(int sockfd){

}

static void acked(int sockfd){

}

static void newdata(int sockfd){

}

// not finished yet
static void senddata(int sockfd) {
  struct socket * sock = get_socket_from_fd(sockfd);
  int mss = uip_mss();
  int pending_bytes = rb_data_len(&(sock->send_queue));
  int len = (mss < pending_bytes) ? mss: pending_bytes;
  int bytes_read = 0;
  uchar_t * send_buf = uip_appdata;
  
  bytes_read = rb_peek(&(sock->send_queue), send_buf, len);

  if (bytes_read == 0) {
    // no packet for send
    return;
  }

  uip_send(send_buf, len);
}



//get the socket id by the local tcp port
static int get_socket_from_port(ushort_t lport) {
  int i;
  
  for (i = 0; i < MAX_SOCKS; i++){
    if (sockets[i].con->lport == lport) 
      return i;
  }
  
  return -1;
}




void
socket_appcall(void)
{
  int sockfd; 

  sockfd = get_socket_from_port(uip_conn->lport);

  if (sockfd == -1) {
    return;
  }

  if(uip_connected()) {
    connected(sockfd);
  }
  
  if(uip_closed() ||uip_aborted() ||uip_timedout()) {
     closed(sockfd);
     return;
  }
  
  if(uip_acked()) {
     acked(sockfd);
  }
  
  if(uip_newdata()) {
     newdata(sockfd);
  }
  
  if(uip_rexmit() ||
     uip_newdata() ||
     uip_acked() ||
     uip_connected() ||
     uip_poll()) {
     senddata(sockfd);
  }
}



int Packet_Received(struct NE2K_Packet_Info * info, uchar_t * pkt) {
  int i;
  
  uip_len = info->size; 

  for (i = 0; i < info->size; i++) {
    uip_buf[i] = *(pkt+i);
  }

  Free(pkt);

  if (BUF->type == htons(UIP_ETHTYPE_ARP)) {
    uip_arp_arpin();

    if (uip_len > 0) {
      NE2K_Transmit(uip_len);
    }					

  } else {

    uip_arp_ipin();
    uip_input();

    if (uip_len > 0) {
      uip_arp_out();
      NE2K_Transmit(uip_len);
    }
  }			  
  return 0;
}