/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it under the terms of the GNU General Public License
 * Version 2 (GPLv2).  The accompanying COPYING file contains the
 * full text of the license.
 */

#ifndef __PALACIOS_RING_BUFFER_H__
#define __PALACIOS_RING_BUFFER_H__

struct ringbuf {
    unsigned char * buf;
    unsigned int size;

    unsigned int start;
    unsigned int end;
    unsigned int current_len;
};


struct ringbuf * create_ringbuf(unsigned int size);
void free_ringbuf(struct ringbuf * ring);
int ringbuf_read(struct ringbuf * ring, unsigned char * dst, unsigned int len);
int ringbuf_write(struct ringbuf * ring, unsigned char * src, unsigned int len);
int ringbuf_data_len(struct ringbuf * ring);

#endif

