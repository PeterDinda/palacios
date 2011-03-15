/*
 * Palacios VM Stream Console interface
 * (c) Jack Lange, 2010
 */

#ifndef __PALACIOS_CONSOLE_H__
#define __PALACIOS_CONSOLE_H__


#include <linux/spinlock.h>
#include <linux/interrupt.h>


struct palacios_console {
    struct gen_queue * queue;
    spinlock_t lock;

    int open;
    int connected;

    wait_queue_head_t intr_queue;

    unsigned int width;
    unsigned int height;

    struct v3_guest * guest;
};



struct v3_guest;


int connect_console(struct v3_guest * guest);

int palacios_init_console( void );


#endif
