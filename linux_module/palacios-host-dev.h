/*
 * Palacios Host Device Interface + User-space interface 
 * (c) Peter Dinda, 2011
 */

#ifndef __PALACIOS_HOST_DEV_H__
#define __PALACIOS_HOST_DEV_H__

#include <linux/spinlock.h>
#include <linux/list.h>
#include "palacios-host-dev-user.h"

struct v3_guest;

/*
   This is the collection of host devices that
   a single guest has
*/
struct palacios_host_dev {
    spinlock_t      lock;
    struct list_head devs;
};



int connect_host_dev(struct v3_guest * guest, char *url);

int palacios_init_host_dev( void );


#endif
