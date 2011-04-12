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


#include <interfaces/vmm_socket.h>
#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>

static struct v3_socket_hooks * sock_hooks = 0;


uint32_t v3_inet_addr(const char * ip_str) {
    uint32_t val;
    int base, n, c;
    uint32_t parts[4];
    uint32_t * pp = parts;

    c = *ip_str;
    for (;;) {
	/*
	 * Collect number up to ``.''.
	 * Values are specified as for C:
	 * 0x=hex, 0=octal, 1-9=decimal.
	 */
	if (!isdigit(c)) {
	    return (0);
	}

	val = 0;
	base = 10;

	if (c == '0') {
	    c = *++ip_str;
	    if ((c == 'x') || (c == 'X')) {
		base = 16;
		c = *++ip_str;
	    } else {
		base = 8;
	    }
	}

	for (;;) {
	    if (isdigit(c)) {
		val = (val * base) + (int)(c - '0');
		c = *++ip_str;
	    } else if ((base == 16) && (isxdigit(c))) {
		val = (val << 4) | (int)(c + 10 - (islower(c) ? 'a' : 'A'));
		c = *++ip_str;
	    } else {
		break;
	    }
	}

	if (c == '.') {
	    /*
	     * Internet format:
	     *  a.b.c.d
	     *  a.b.c   (with c treated as 16 bits)
	     *  a.b (with b treated as 24 bits)
	     */
	    if (pp >= parts + 3) {
		return 0;
	    }

	    *pp++ = val;
	    c = *++ip_str;
	} else {
	    break;
	}
    }

    /*
     * Check for trailing characters.
     */
    if ( (c != '\0') && 
	 ( (!isprint(c)) || (!isspace(c)) ) ) {
	return 0;
    }

    /*
     * Concoct the address according to
     * the number of parts specified.
     */
    n = pp - parts + 1;

    switch (n) {
	case 0:
	    return 0;       /* initial nondigit */

	case 1:             /* a -- 32 bits */
	    break;

	case 2:             /* a.b -- 8.24 bits */
	    if (val > 0xffffffUL) {
		return 0;
	    }

	    val |= parts[0] << 24;
	    break;

	case 3:             /* a.b.c -- 8.8.16 bits */
	    if (val > 0xffff) {
		return 0;
	    }

	    val |= (parts[0] << 24) | (parts[1] << 16);
	    break;

	case 4:             /* a.b.c.d -- 8.8.8.8 bits */
	    if (val > 0xff) {
		return 0;
	    }

	    val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
	    break;
    }
  
    if (val) {
	return v3_htonl(val);
    }
  
    return -1;
}


char * v3_inet_ntoa(uint32_t addr) {
    static char str[16];
    char inv[3];
    char * rp;
    uint8_t * ap;
    uint8_t rem;
    uint8_t n;
    uint8_t i;

    rp = str;
    ap = (uint8_t *)&addr;

    for (n = 0; n < 4; n++) {
	i = 0;

	do {
	    rem = *ap % (uint8_t)10;

	    *ap /= (uint8_t)10;

	    inv[i++] = '0' + rem;
	} while(*ap);

	while(i--) {
	    *rp++ = inv[i];
	}

	*rp++ = '.';
	ap++;
    }

    *--rp = 0;

    return str;
}





uint16_t v3_htons(uint16_t n) {
    return (((n & 0xff) << 8) | ((n & 0xff00) >> 8));
}


uint16_t v3_ntohs(uint16_t n) {
    return v3_htons(n);
}


uint32_t v3_htonl(uint32_t n) {
    return (((n & 0xff) << 24) |
	    ((n & 0xff00) << 8) |
	    ((n & 0xff0000UL) >> 8) |
	    ((n & 0xff000000UL) >> 24));
}


uint32_t v3_ntohl(uint32_t n) {
  return v3_htonl(n);
}


v3_sock_t v3_create_udp_socket(struct v3_vm_info * vm) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->udp_socket);
    void * priv_data = NULL;

    if (vm) {
	priv_data = vm->host_priv_data;
    }
    
    return sock_hooks->udp_socket(0, 0, priv_data);
}

v3_sock_t v3_create_tcp_socket(struct v3_vm_info * vm) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->tcp_socket);
    void * priv_data = NULL;

    if (vm) {
	priv_data = vm->host_priv_data;
    }

    return sock_hooks->tcp_socket(0, 1, 0, priv_data);
}

void v3_socket_close(v3_sock_t sock) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->close);

    sock_hooks->close(sock);
}

int v3_socket_bind(const v3_sock_t sock, uint16_t port) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->bind);

    return sock_hooks->bind(sock, port);
}

int v3_socket_listen(const v3_sock_t sock, int backlog) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->listen);

    return sock_hooks->listen(sock, backlog);
}

v3_sock_t v3_socket_accept(const v3_sock_t sock, uint32_t * remote_ip, uint32_t * port) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->accept);

    return sock_hooks->accept(sock, remote_ip, port);
}

int v3_connect_to_ip(const v3_sock_t sock, const uint32_t hostip, const uint16_t port) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->connect_to_ip);
    
    return sock_hooks->connect_to_ip(sock, hostip, port);
}

int v3_connect_to_host(const v3_sock_t sock, const char * hostname, const uint16_t port) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->connect_to_host);

    return sock_hooks->connect_to_host(sock, hostname, port);
}

int v3_socket_send(const v3_sock_t sock, const uint8_t * buf, const uint32_t len) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->send);

    return sock_hooks->send(sock, buf, len);
}

int v3_socket_recv(const v3_sock_t sock, uint8_t * buf, const uint32_t len) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->recv);

    return sock_hooks->recv(sock, buf, len);
}

int v3_socket_send_to_host(const v3_sock_t sock, const char * hostname, const uint16_t port, 
			   const uint8_t * buf, const uint32_t len) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->sendto_host);

    return sock_hooks->sendto_host(sock, hostname, port, buf, len);
}

int v3_socket_send_to_ip(const v3_sock_t sock, const uint32_t ip, const uint16_t port, 
			 const uint8_t * buf, const uint32_t len) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->sendto_ip);

    return sock_hooks->sendto_ip(sock, ip, port, buf, len);
}

int v3_socket_recv_from_host(const v3_sock_t sock, const char * hostname, const uint16_t port, 
			     uint8_t * buf, const uint32_t len) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->recvfrom_host);

    return sock_hooks->recvfrom_host(sock, hostname, port, buf, len);
}

int v3_socket_recv_from_ip(const v3_sock_t sock, const uint32_t ip, const uint16_t port, 
			   uint8_t * buf, const uint32_t len) {
    V3_ASSERT(sock_hooks);
    V3_ASSERT(sock_hooks->recvfrom_ip);

    return sock_hooks->recvfrom_ip(sock, ip, port, buf, len);
}




void V3_Init_Sockets(struct v3_socket_hooks * hooks) {
    sock_hooks = hooks;
    PrintDebug("V3 sockets inited\n");

    return;
}
