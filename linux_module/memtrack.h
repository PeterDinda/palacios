/*
 * Palacios Memory Tracking Userland Interface
 * Copyright (c) 2014 Peter Dinda <pdinda@northwestern.edu>
 */

#ifndef __PALACIOS_MEMTRACK_H__
#define __PALACIOS_MEMTRACK_H__

#include <palacios/vmm_mem_track.h>

#define V3_VM_MEM_TRACK_SIZE 300
#define V3_VM_MEM_TRACK_CMD  301
#define V3_VM_MEM_TRACK_SNAP 302


// Used to get needed info to allocate requests
struct v3_mem_track_sizes {
    uint64_t  num_cores;
    uint64_t  num_pages;
};

// These are sent for start and stop requests
struct v3_mem_track_cmd {
    enum { V3_MEM_TRACK_START, V3_MEM_TRACK_STOP} request;
    struct v3_vm_mem_track                        config;  // for "start"
};


// A snapshot request consists of the v3_mem_track_snapshot to fill out

#endif
