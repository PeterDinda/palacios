/* 
 * Palacios Stream interface
 * (c) Lei Xia, 2010
 */

#ifndef __PALACIOS_STREAM_H__
#define __PALACIOS_STREAM_H__

#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include "palacios.h"
#include "palacios-ringbuffer.h"

#define _V3VEE_
//#include <palacios/vmm_ringbuffer.h>
#undef _V3VEE_

#define STREAM_BUF_SIZE 1024
#define STREAM_NAME_LEN 128

struct stream_buffer {
    char name[STREAM_NAME_LEN];
    struct ringbuf * buf;

    wait_queue_head_t intr_queue;
    spinlock_t lock;

    struct v3_guest * guest;
    struct list_head stream_node;
};


void palacios_init_stream(void);
void palacios_deinit_stream(void);

int stream_dequeue(struct stream_buffer * stream, char * buf, int len);
int stream_datalen(struct stream_buffer * stream);

struct stream_buffer * find_stream_by_name(struct v3_guest * guest, const char * name);

int open_stream(const char * name);
#endif

