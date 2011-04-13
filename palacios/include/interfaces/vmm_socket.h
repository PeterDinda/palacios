/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMM_SOCKET_H__
#define __VMM_SOCKET_H__

#include <palacios/vmm.h>


#ifdef __V3VEE__


typedef void * v3_sock_t;

v3_sock_t v3_create_udp_socket(struct v3_vm_info * vm);
v3_sock_t v3_create_tcp_socket(struct v3_vm_info * vm);

void v3_socket_close(v3_sock_t sock);

int v3_socket_bind(v3_sock_t sock, uint16_t port);
int v3_socket_listen(const v3_sock_t sock, int backlog);
v3_sock_t v3_socket_accept(const v3_sock_t sock, uint32_t * remote_ip, uint32_t * port);

int v3_connect_to_ip(const v3_sock_t sock, const uint32_t hostip, const uint16_t port);
int v3_connect_to_host(const v3_sock_t sock, const char * hostname, const uint16_t port);


int v3_socket_send(const v3_sock_t sock, const uint8_t * buf, const uint32_t len);
int v3_socket_recv(const v3_sock_t sock, uint8_t * buf, const uint32_t len);


int v3_socket_send_to_host(const v3_sock_t sock, const char * hostname, const uint16_t port, 
			   const uint8_t * buf, const uint32_t len);
int v3_socket_send_to_ip(const v3_sock_t sock, const uint32_t ip, const uint16_t port, 
			 const uint8_t * buf, const uint32_t len);
int v3_socket_recv_from_host(const v3_sock_t sock, const char * hostname, const uint16_t port, 
			     uint8_t * buf, const uint32_t len);
int v3_socket_recv_from_ip(const v3_sock_t sock, const uint32_t ip, const uint16_t port, 
			   uint8_t * buf, const uint32_t len);


#define V3_Select_Socket(rset,wset,eset,tv) ({				\
	    extern struct v3_socket_hooks * sock_hooks;			\
	    int ret = -1;						\
	    if ((sock_hooks) && (sock_hooks)->select) {			\
		ret = (sock_hooks)->select(rset, wset, eset, tv);	\
	    }								\
	    ret;							\
	})







#define V3_SOCK_SET(n, p)  ((p)->fd_bits[(n) / 8] |=  (1 << ((n) & 7)))
#define V3_SOCK_CLR(n, p)  ((p)->fd_bits[(n) / 8] &= ~(1 << ((n) & 7)))
#define V3_SOCK_ISSET(n,p) ((p)->fd_bits[(n) / 8] &   (1 << ((n) & 7)))
#define V3_SOCK_ZERO(p)    memset((void *)(p), 0, sizeof(*(p)))


uint32_t v3_inet_addr(const char * ip_str);
char * v3_inet_ntoa(uint32_t addr);
uint16_t v3_htons(uint16_t s);
uint16_t v3_ntohs(uint16_t s);
uint32_t v3_htonl(uint32_t s);
uint32_t v3_ntohl(uint32_t s);




#endif


struct v3_timeval {
    long    tv_sec;         /* seconds */
    long    tv_usec;        /* and microseconds */
};


#define V3_SOCK_SETSIZE    1000

typedef struct v3_sock_set {
    /* This format needs to match the standard posix FD_SET format, so it can be cast */
    unsigned char fd_bits [(V3_SOCK_SETSIZE + 7) / 8];
} v3_sock_set;



struct v3_socket_hooks {
    /* Socket creation routines */
    void *(*tcp_socket)(const int bufsize, const int nodelay, const int nonblocking, void * priv_data);
    void *(*udp_socket)(const int bufsize, const int nonblocking, void * priv_data);

    /* Socket Destruction */
    void (*close)(void * sock);

    /* Network Server Calls */
    int (*bind)(const void * sock, const int port);

    int (*listen)(const void * sock, int backlog);
  
    void *(*accept)(const void * sock, unsigned int * remote_ip, unsigned int * port);
    /* This going to suck */
    int (*select)(struct v3_sock_set * rset, \
		  struct v3_sock_set * wset, \
		  struct v3_sock_set * eset, \
		  struct v3_timeval tv);

    /* Connect calls */
    int (*connect_to_ip)(const void * sock, const int hostip, const int port);
    int (*connect_to_host)(const void * sock, const char * hostname, const int port);

    /* TCP Data Transfer */
    int (*send)(const void * sock, const char * buf, const int len);
    int (*recv)(const void * sock, char * buf, const int len);
  
    /* UDP Data Transfer */
    int (*sendto_host)(const void * sock, const char * hostname, const int port, 
		       const char * buf, const int len);
    int (*sendto_ip)(const void * sock, const int ip_addr, const int port, 
		     const char * buf, const int len);
  
    int (*recvfrom_host)(const void * sock, const char * hostname, const int port, 
			 char * buf, const int len);
    int (*recvfrom_ip)(const void * sock, const int ip_addr, const int port, 
		       char * buf, const int len);
};


extern void V3_Init_Sockets(struct v3_socket_hooks * hooks);

#endif
