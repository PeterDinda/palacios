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


#include <palacios/vmm_socket.h>
#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_types.h>


struct v3_socket_hooks * sock_hooks = 0;

void V3_Init_Sockets(struct v3_socket_hooks * hooks) {
    sock_hooks = hooks;
    PrintDebug("V3 sockets inited\n");

    return;
}



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

    for(n = 0; n < 4; n++) {
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
