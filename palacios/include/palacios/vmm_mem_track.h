/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2014, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_MEM_TRACK_H__
#define __VMM_MEM_TRACK_H__

struct guest_info;
struct v3_vm_info;


// Currently, only V3_MEM_TRACK_ACCESS is supported. ..
#define V3_MEM_TRACK_NONE   0
#define V3_MEM_TRACK_READ   1
#define V3_MEM_TRACK_WRITE  2
#define V3_MEM_TRACK_EXEC   4
#define V3_MEM_TRACK_ACCESS (V3_MEM_TRACK_READ | V3_MEM_TRACK_WRITE | V3_MEM_TRACK_EXEC)

typedef uint32_t v3_mem_track_access_t;
typedef enum {V3_MEM_TRACK_ONESHOT, V3_MEM_TRACK_PERIODIC } v3_mem_track_reset_t;


// each VM contains this
struct v3_vm_mem_track {
    int                    started;
    v3_mem_track_access_t  access_type;
    v3_mem_track_reset_t   reset_type;
    
    uint64_t               period;  // or the interval for oneshot (in cycles) (0=continuous)
};

// each core contains this
struct v3_core_mem_track {
    uint64_t               start_time;     // cycle count when we started
    uint64_t               end_time;       // ... ended

    uint64_t               num_pages;      // the GPA in 4K pages (size of bitmap in bits)

#define SET_BIT(x,pos) do { (x)[(pos)/8] |= 1 << ((pos)%8); } while (0)
#define CLR_BIT(x,pos) do { (x)[(pos)/8] &= ~(1 << ((pos)%8);) } while (0)
#define GET_BIT(x,pos) ((x)[(pos)/8] >> ((pos)%8) & 0x1)

    uint8_t                *access_bitmap; // the only one so far (1 bit per page)
};


// self-contained info
typedef struct mem_track_snapshot {
    v3_mem_track_access_t  access_type;
    v3_mem_track_reset_t   reset_type;
    uint64_t               period;

    uint32_t               num_cores;      // how many cores
    struct v3_core_mem_track core[0];      // one per core
} v3_mem_track_snapshot;



int v3_mem_track_init(struct v3_vm_info *vm);
int v3_mem_track_deinit(struct v3_vm_info *vm);

int v3_mem_track_start(struct v3_vm_info *vm, v3_mem_track_access_t access, v3_mem_track_reset_t reset, uint64_t period);
int v3_mem_track_stop(struct v3_vm_info *vm);

int v3_mem_track_get_sizes(struct v3_vm_info *vm, uint64_t *num_cores, uint64_t *num_pages);


v3_mem_track_snapshot *v3_mem_track_take_snapshot(struct v3_vm_info *vm);
void v3_mem_track_free_snapshot(v3_mem_track_snapshot *snapshot);

// call with interrupts on...
void v3_mem_track_entry(struct guest_info *core);
void v3_mem_track_exit(struct guest_info *core);



#endif
