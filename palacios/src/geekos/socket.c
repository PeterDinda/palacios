#include <geekos/socket.h>
#include <geekos/malloc.h>
#include <geekos/ne2k.h>
#include <uip/uip.h>
#include <uip/uip_arp.h>
#include <geekos/int.h>
#include <geekos/vmm_stubs.h>
#include <geekos/queue.h>


// for some reason, there are compile warnings without these

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

#define MAX_SOCKS 1024

struct socket sockets[MAX_SOCKS];

struct gen_queue in_packets;

struct sock_packet {
	int size;
	uchar_t *data;
};

int Packet_Received(struct NE2K_Packet_Info* info, uchar_t *pkt) ;

void init_network() {
   int i = 0;
   
   init_queue(&in_packets);

   for (i = 0; i < MAX_SOCKS; i++) {
      sockets[i].in_use = 0;
      init_queue(&(sockets[i].send_queue));
      init_queue(&(sockets[i].recv_queue));
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
  sockets[sockfd].in_use = 0;
  return 0;
}


struct socket * get_socket_from_fd(int fd) {
  return &(sockets[fd]);
}




int connect(const uchar_t ip_addr[4], ushort_t port) {
  int sockfd = -1;
  sockfd = allocate_socket_fd();
  uip_ipaddr_t ipaddr;
  
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
      //devicedriver_send();
      NE2K_Transmit(uip_len);
    }
  }
  for(i = 0; i < UIP_UDP_CONNS; i++) {
    uip_udp_periodic(i);
    if(uip_len > 0) {
      //devicedriver_send();
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
static void senddata(int sockfd){
  uchar_t *bufptr;
  int len = 0;
  
  bufptr = uip_appdata;
  
  if(len < uip_mss()) {
    // memcpy(bufptr, data, len);
  } else {
    
  }
  //uip_send(uip_appdata,len);
}



//get the socket id by the local tcp port
static int  get_socket_from_port(ushort_t lport) {
  int i;
  
  for (i = 0; i<MAX_SOCKS; i++){
    if (sockets[i].con->lport == lport) {
      return i;
    }
  }
  
  return -1;
}




void socket_appcall(void) {
  int sockfd; 
  
  sockfd = get_socket_from_port(uip_conn->lport);

  if (sockfd == -1) return;
    
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



int Packet_Received(struct NE2K_Packet_Info* info, uchar_t *pkt) {
  struct sock_packet next;
  int i;
  
  next.size = info->size;
  next.data = (uchar_t *)Malloc(next.size);
  
  if (next.data == NULL) {
    return 1;
  }

  //uip_len = info->size;  
  
  for(i = 0; i < info->size; i++) {
    *((next.data) + i) = *(pkt + i);
  }
  Free(pkt);
  
  Disable_Interrupts();
  enqueue(&in_packets, &next);
  Enable_Interrupts();
  
  //triger_receiver_interrupt();
  
  return 0;
}


void int_handler_packet_receive(struct Interrupt_State * state){
  //device driver got a incoming packet and enqueue that packet to the receive queue
  struct sock_packet * next_packet; 
  void * pkt;
  int i; 
  
  while(1){ 
    //currently disable interrupt because no lock for the queue
    Disable_Interrupts();
    pkt = dequeue(&in_packets);
    Enable_Interrupts();
    
    if (pkt == NULL) {
      break;
    }

    //there are new packets in the receiver queue
    next_packet = (struct sock_packet *)pkt;
    uip_len = next_packet->size;  
    
    for(i = 0; i < uip_len; i++) {
      uip_buf[i] = *((next_packet->data) + i);
    }
    
    Free(next_packet->data);
    Free(next_packet);
    
    if(BUF->type == htons(UIP_ETHTYPE_ARP)) {
      uip_arp_arpin();
      if (uip_len > 0){
	//ethernet_devicedriver_send();
	NE2K_Transmit(uip_len);
      }					
    } else {
      uip_arp_ipin();
      uip_input();
      if(uip_len > 0) {
	uip_arp_out();
	//ethernet_devicedriver_send();
	NE2K_Transmit(uip_len);
      }
    }			  
  }	
}
