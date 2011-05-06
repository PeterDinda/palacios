#ifndef _PALACIOS_H
#define _PALACIOS_H

#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/slab.h>

#ifdef V3_CONFIG_CONSOLE
#include "palacios-console.h"
#endif

#ifdef V3_CONFIG_GRAPHICS_CONSOLE
#include "palacios-graphics-console.h"
#endif

#ifdef V3_CONFIG_HOST_DEVICE
#include "palacios-host-dev.h"
#endif


/* Global Control IOCTLs */
#define V3_START_GUEST 10
#define V3_ADD_MEMORY 50
#define V3_START_NETWORK 60

/* VM Specific IOCTLs */
#define V3_VM_CONSOLE_CONNECT 20
#define V3_VM_STREAM_CONNECT 21
#define V3_VM_STOP 22

#define V3_VM_FB_INPUT 256+1
#define V3_VM_FB_QUERY 256+2

#define V3_VM_HOST_DEV_CONNECT 512+1


struct v3_guest_img {
    unsigned long long size;
    void * guest_data;
    char name[128];
};

struct v3_mem_region {
    unsigned long long base_addr;
    unsigned long long num_pages;
};

struct v3_network {
    unsigned char socket;
    unsigned char packet;
    unsigned char vnet;
};

void * trace_malloc(size_t size, gfp_t flags);
void trace_free(const void * objp);


struct v3_guest {
    void * v3_ctx;

    void * img; 
    u32 img_size;

    char name[128];

    struct list_head files;
    struct list_head streams;
    struct list_head sockets;

#ifdef V3_CONFIG_CONSOLE
    struct palacios_console console;
#endif

#ifdef V3_CONFIG_CONSOLE
    struct palacios_graphics_console graphics_console;
#endif

#ifdef V3_CONFIG_HOST_DEVICE
    struct palacios_host_dev hostdev;
#endif


    struct completion start_done;
    struct completion thread_done;

    dev_t vm_dev; 
    struct cdev cdev;
};

// For now MAX_VMS must be a multiple of 8
// This is due to the minor number bitmap
#define MAX_VMS 32





extern void send_key_to_palacios(unsigned char status, unsigned char scan_code);


int palacios_vmm_init( void );
int palacios_vmm_exit( void );



#endif
