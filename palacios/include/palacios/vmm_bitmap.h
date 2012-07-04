/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Jack Lange <jacklange@cs.pitt.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jacklange@cs.pitt.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */



#ifndef __VMM_BITMAP_H__
#define __VMM_BITMAP_H__

#ifdef __V3VEE__
#include <palacios/vmm_types.h>
#include <palacios/vmm_lock.h>



struct v3_bitmap {
    v3_lock_t lock; 
    int num_bits;      // number of valid bit positions in the bitmap
    uint8_t * bits;   // actual bitmap. Dynamically allocated... ugly
};


int v3_bitmap_init(struct v3_bitmap * bitmap, int num_bits);
void v3_bitmap_deinit(struct v3_bitmap * bitmap);
int v3_bitmap_reset(struct v3_bitmap * bitmap);

int v3_bitmap_set(struct v3_bitmap * bitmap, int index);
int v3_bitmap_clear(struct v3_bitmap * bitmap, int index);
int v3_bitmap_check(struct v3_bitmap * bitmap, int index);

int v3_bitmap_count(struct v3_bitmap * bitmap);
int v3_bitmap_copy(struct v3_bitmap * dst, struct v3_bitmap * src);

#endif

#endif
