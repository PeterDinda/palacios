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

#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__


#include <geekos/ktypes.h>


struct ring_buffer {
  uchar_t * buf;
  uint_t size;

  uint_t start;
  uint_t end;
  uint_t current_len;

};


void init_ring_buffer(struct ring_buffer * ring, uint_t size);
struct ring_buffer * create_ring_buffer(uint_t size);

void free_ring_buffer(struct ring_buffer * ring);


int rb_read(struct ring_buffer * ring, char * dst, uint_t len);
int rb_peek(struct ring_buffer * ring, char * dst, uint_t len);
int rb_delete(struct ring_buffer * ring, uint_t len);
int rb_write(struct ring_buffer * ring, char * src, uint_t len);
int rb_data_len(struct ring_buffer * ring);
int rb_capacity(struct ring_buffer * ring);


void print_ring_buffer(struct ring_buffer * ring);


#endif // ! __RING_BUFFER_H__
