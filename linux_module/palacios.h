#ifndef _PALACIOS_H
#define _PALACIOS_H

#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/slab.h>


/* Global Control IOCTLs */
#define V3_CREATE_GUEST 12
#define V3_FREE_GUEST 13

#define V3_ADD_MEMORY 50

/* VM Specific IOCTLs */
#define V3_VM_CONSOLE_CONNECT 20


#define V3_VM_PAUSE 23
#define V3_VM_CONTINUE 24

#define V3_VM_LAUNCH 25
#define V3_VM_STOP 26
#define V3_VM_LOAD 27
#define V3_VM_SAVE 28

#define V3_VM_INSPECT 30

#define V3_VM_MOVE_CORE 33

#define V3_VM_FB_INPUT (256+1)
#define V3_VM_FB_QUERY (256+2)

#define V3_VM_HOST_DEV_CONNECT (10244+1)

#define V3_VM_KSTREAM_USER_CONNECT (11244+1)


struct v3_guest_img {
    unsigned long long size;
    void * guest_data;
    char name[128];
} __attribute__((packed));

struct v3_mem_region {
    unsigned long long base_addr;
    unsigned long long num_pages;
} __attribute__((packed));

struct v3_core_move_cmd{
    unsigned short vcore_id;
    unsigned short pcore_id;
} __attribute__((packed));

struct v3_chkpt_info {
    char store[128];
    char url[256]; /* This might need to be bigger... */
} __attribute__((packed));



void * trace_malloc(size_t size, gfp_t flags);
void trace_free(const void * objp);


struct v3_guest {
    void * v3_ctx;

    void * img; 
    u32 img_size;

    char name[128];


    struct rb_root vm_ctrls;
    struct list_head exts;

    dev_t vm_dev; 
    struct cdev cdev;
};

// For now MAX_VMS must be a multiple of 8
// This is due to the minor number bitmap
#define MAX_VMS 32



int palacios_vmm_init( void );
int palacios_vmm_exit( void );



#endif
