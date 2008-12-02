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

#include <geekos/ring_buffer.h>
#include <geekos/malloc.h>
#include <geekos/ktypes.h>
#include <geekos/debug.h>


void init_ring_buffer(struct ring_buffer * ring, uint_t size) {
  ring->buf = Malloc(size);
  ring->size = size;
  
  ring->start = 0;
  ring->end = 0;
  ring->current_len = 0;
}


struct ring_buffer * create_ring_buffer(uint_t size) {
  struct ring_buffer * ring = (struct ring_buffer *)Malloc(sizeof(struct ring_buffer));

  init_ring_buffer(ring, size);

  return ring;
}

void free_ring_buffer(struct ring_buffer * ring) {
  Free(ring->buf);
  Free(ring);
}





static inline uchar_t * get_read_ptr(struct ring_buffer * ring) {
  return (uchar_t *)(ring->buf + ring->start);
}

static inline uchar_t * get_write_ptr(struct ring_buffer * ring) {
  return (uchar_t *)(ring->buf + ring->end);
}


static inline int get_read_section_size(struct ring_buffer * ring) {
  return ring->size - ring->start;
}

static inline int get_write_section_size(struct ring_buffer * ring) {
  return ring->size - ring->end;
}

static inline int is_read_loop(struct ring_buffer * ring, uint_t len) {
  if ((ring->start >= ring->end) && (ring->current_len > 0)) {
    // end is past the end of the buffer
    if (get_read_section_size(ring) < len) {
      return 1;
    }
  }
  return 0;
}


static inline int is_write_loop(struct ring_buffer * ring, uint_t len) {
  if ((ring->end >= ring->start) && (ring->current_len < ring->size)) {
    // end is past the end of the buffer
    if (get_write_section_size(ring) < len) {
      return 1;
    }
  }
  return 0;
}


int rb_data_len(struct ring_buffer * ring) {
  return ring->current_len;
}

int rb_capacity(struct ring_buffer * ring) {
  return ring->size;
}

int rb_read(struct ring_buffer * ring, char * dst, uint_t len) {
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




int rb_peek(struct ring_buffer * ring, char * dst, uint_t len) {
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


int rb_delete(struct ring_buffer * ring, uint_t len) {
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


int rb_write(struct ring_buffer * ring, char * src, uint_t len) {
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


void print_ring_buffer(struct ring_buffer * ring) {
  int ctr = 0;
  
  for (ctr = 0; ctr < ring->current_len; ctr++) {
    int index = (ctr + ring->start) % ring->size;

    PrintBoth("Entry %d (index=%d): %c\n", ctr, index, ring->buf[index]);
  }
}
