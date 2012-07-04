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

#include <palacios/vmm_bitmap.h>
#include <palacios/vmm.h>


int v3_bitmap_init(struct v3_bitmap * bitmap, int num_bits) {
    int num_bytes = (num_bits / 8) + ((num_bits % 8) > 0);

    v3_lock_init(&(bitmap->lock));
    bitmap->num_bits = num_bits;
    bitmap->bits = V3_Malloc(num_bytes);


    if (bitmap->bits == NULL) {
	PrintError("Could not allocate bitmap of %d bits\n", num_bits);
	return -1;
    }
    
    memset(bitmap->bits, 0, num_bytes);

    return 0;
}


void v3_bitmap_deinit(struct v3_bitmap * bitmap) {
    v3_lock_deinit(&(bitmap->lock));
    V3_Free(bitmap->bits);
}


int v3_bitmap_reset(struct v3_bitmap * bitmap) {
    int num_bytes = (bitmap->num_bits / 8) + ((bitmap->num_bits % 8) > 0);

    memset(bitmap->bits, 0, num_bytes);

    return 0;
}

int v3_bitmap_set(struct v3_bitmap * bitmap, int index) {
    int major = index / 8;
    int minor = index % 8;
    int old_val = 0;
    uint32_t flags = 0;

    if (index > (bitmap->num_bits - 1)) {
	PrintError("Index out of bitmap range: (pos = %d) (num_bits = %d)\n", 
		   index, bitmap->num_bits);
	return -1;
    }


    flags = v3_lock_irqsave(bitmap->lock);

    old_val = (bitmap->bits[major] & (0x1 << minor));
    bitmap->bits[major] |= (0x1 << minor);

    v3_unlock_irqrestore(bitmap->lock, flags);

    return old_val;
}


int v3_bitmap_clear(struct v3_bitmap * bitmap, int index) {
    int major = index / 8;
    int minor = index % 8;
    int old_val = 0;
    uint32_t flags = 0;

    if (index > (bitmap->num_bits - 1)) {
	PrintError("Index out of bitmap range: (pos = %d) (num_bits = %d)\n", 
		   index, bitmap->num_bits);
	return -1;
    }

    flags = v3_lock_irqsave(bitmap->lock);

    old_val = (bitmap->bits[major] & (0x1 << minor));
    bitmap->bits[major] &= ~(0x1 << minor);

    v3_unlock_irqrestore(bitmap->lock, flags);

    return old_val;
}

int v3_bitmap_check(struct v3_bitmap * bitmap, int index) {
    int major = index / 8;
    int minor = index % 8;

    if (index > (bitmap->num_bits - 1)) {
	PrintError("Index out of bitmap range: (pos = %d) (num_bits = %d)\n", 
		   index, bitmap->num_bits);
	return -1;
    }

    return ((bitmap->bits[major] & (0x1 << minor)) != 0);
}


int v3_bitmap_count(struct v3_bitmap * bitmap) {

    int cnt = 0;
    int i;
    uint8_t x;
    uint8_t * bytes = bitmap->bits;
    int num_bytes = (bitmap->num_bits / 8) + ((bitmap->num_bits % 8) > 0);

    for (i=0; i < num_bytes; i++) {
        x = bytes[i];
        while (x) { 
	    cnt += (x & 0x1);
	    x>>=1;
	}
    }     
    
    return cnt;
}

int v3_bitmap_copy(struct v3_bitmap * dst, struct v3_bitmap * src) {
    
    if (src->num_bits != dst->num_bits) {
        PrintError("src and dst must be the same size.\n");
	return -1;    
    }
    
    int num_bytes = (src->num_bits / 8) + ((src->num_bits % 8)!=0);
    
    memcpy(dst->bits,src->bits,num_bytes);
    
    return 0;
}
