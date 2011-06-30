/*
 * Ringbuffer implementation
 * (c) Lei Xia  2010
 */
 

#include <linux/errno.h>
#include <linux/percpu.h>
#include <linux/sched.h>

#include "palacios.h"
#include "util-ringbuffer.h"

void init_ringbuf(struct ringbuf * ring, unsigned int size) {
    ring->buf = kmalloc(size, GFP_KERNEL);
    ring->size = size;
  
    ring->start = 0;
    ring->end = 0;
    ring->current_len = 0;
}

struct ringbuf * create_ringbuf(unsigned int size) {
    struct ringbuf * ring = (struct ringbuf *)kmalloc(sizeof(struct ringbuf), GFP_KERNEL);
    init_ringbuf(ring, size);

    return ring;
}

void free_ringbuf(struct ringbuf * ring) {
    kfree(ring->buf);
    kfree(ring);
}

static inline unsigned char * get_read_ptr(struct ringbuf * ring) {
    return (unsigned char *)(ring->buf + ring->start);
}


static inline unsigned char * get_write_ptr(struct ringbuf * ring) {
    return (unsigned char *)(ring->buf + ring->end);
}

static inline int get_read_section_size(struct ringbuf * ring) {
    return ring->size - ring->start;
}


static inline int get_write_section_size(struct ringbuf * ring) {
    return ring->size - ring->end;
}


static inline int is_read_loop(struct ringbuf * ring, unsigned int len) {
    if ((ring->start >= ring->end) && (ring->current_len > 0)) {
	// end is past the end of the buffer
	if (get_read_section_size(ring) < len) {
	    return 1;
	}
    }
    return 0;
}


static inline int is_write_loop(struct ringbuf * ring, unsigned int len) {
    if ((ring->end >= ring->start) && (ring->current_len < ring->size)) {
	// end is past the end of the buffer
	if (get_write_section_size(ring) < len) {
	    return 1;
	}
    }
    return 0;
}


static inline int ringbuf_avail_space(struct ringbuf * ring) {
    return ring->size - ring->current_len;
}


int ringbuf_data_len(struct ringbuf * ring) {
    return ring->current_len;
}


static inline int ringbuf_capacity(struct ringbuf * ring) {
    return ring->size;
}


int ringbuf_read(struct ringbuf * ring, unsigned char * dst, unsigned int len) {
    int read_len = 0;
    int ring_data_len = ring->current_len;

    read_len = (len > ring_data_len) ? ring_data_len : len;

    if (is_read_loop(ring, read_len)) {
	int section_len = get_read_section_size(ring);

	memcpy(dst, get_read_ptr(ring), section_len);
	memcpy(dst + section_len, ring->buf, read_len - section_len);
    
	ring->start = read_len - section_len;
    } else {
	memcpy(dst, get_read_ptr(ring), read_len);
    
	ring->start += read_len;
    }

    ring->current_len -= read_len;

    return read_len;
}


#if 0

static int ringbuf_peek(struct ringbuf * ring, unsigned char * dst, unsigned int len) {
    int read_len = 0;
    int ring_data_len = ring->current_len;

    read_len = (len > ring_data_len) ? ring_data_len : len;

    if (is_read_loop(ring, read_len)) {
	int section_len = get_read_section_size(ring);

	memcpy(dst, get_read_ptr(ring), section_len);
	memcpy(dst + section_len, ring->buf, read_len - section_len);
    } else {
	memcpy(dst, get_read_ptr(ring), read_len);
    }

    return read_len;
}


static int ringbuf_delete(struct ringbuf * ring, unsigned int len) {
    int del_len = 0;
    int ring_data_len = ring->current_len;

    del_len = (len > ring_data_len) ? ring_data_len : len;

    if (is_read_loop(ring, del_len)) {
	int section_len = get_read_section_size(ring);
	ring->start = del_len - section_len;
    } else {
	ring->start += del_len;
    }

    ring->current_len -= del_len;
    return del_len;
}
#endif

int ringbuf_write(struct ringbuf * ring, unsigned char * src, unsigned int len) {
    int write_len = 0;
    int ring_avail_space = ring->size - ring->current_len;
  
    write_len = (len > ring_avail_space) ? ring_avail_space : len;

    if (is_write_loop(ring, write_len)) {
	int section_len = get_write_section_size(ring);
    
	memcpy(get_write_ptr(ring), src, section_len);
	ring->end = 0;

	memcpy(get_write_ptr(ring), src + section_len, write_len - section_len);

	ring->end += write_len - section_len;
    } else {
	memcpy(get_write_ptr(ring), src, write_len);

	ring->end += write_len;
    }

    ring->current_len += write_len;

    return write_len;
}

