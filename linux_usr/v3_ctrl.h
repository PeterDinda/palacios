/* 
 * V3 Control header file 
 * (c) Jack lange, 2010
 */

#ifndef _v3_ctrl_h
#define _v3_ctrl_h

#include <stdio.h>
#include <stdlib.h>
#include "ezxml.h"

/* Global Control IOCTLs */
#define V3_CREATE_GUEST 12
#define V3_FREE_GUEST 13

#define V3_ADD_MEMORY 50
#define V3_RESET_MEMORY 51
#define V3_REMOVE_MEMORY 52

#define V3_ADD_PCI_HW_DEV 55
#define V3_ADD_PCI_USER_DEV 56

#define V3_DVFS_CTRL  60

/* VM Specific IOCTLs */

/* VM Specific ioctls */
#define V3_VM_CONSOLE_CONNECT 20
#define V3_VM_SERIAL_CONNECT 21
#define V3_VM_PAUSE 23
#define V3_VM_CONTINUE 24

#define V3_VM_LAUNCH 25
#define V3_VM_STOP 26
#define V3_VM_LOAD 27
#define V3_VM_SAVE 28
#define V3_VM_SIMULATE 29
#define V3_VM_INSPECT 30
#define V3_VM_DEBUG 31


#define V3_VM_MOVE_CORE 33

#define V3_VM_SEND    34
#define V3_VM_RECEIVE 35

#define V3_VM_MOVE_MEM 36

#define V3_VM_RESET 40

#define V3_VM_FB_INPUT 257
#define V3_VM_FB_QUERY 258

#define V3_VM_HOST_DEV_CONNECT 10245
#define V3_VM_KSTREAM_USER_CONNECT 11245


static const char * v3_dev = "/dev/v3vee";

struct v3_guest_img {
    unsigned long long size;
    void * guest_data;
    char name[128];
} __attribute__((packed));


typedef enum { PREALLOCATED=0,          // user space-allocated (e.g. hot remove)
	       REQUESTED,               // kernel will attempt allocation (anywhere)
	       REQUESTED32,             // kernel will attempt allocation (<4GB)
} v3_mem_region_type_t;

struct v3_mem_region {
    v3_mem_region_type_t type;         // 
    int                  node;         // numa node for REQUESTED (-1 = any)
    unsigned long long   base_addr;    // region start (hpa) for PREALLOCATED
    unsigned long long   num_pages;    // size for PREALLOCATED or request size for REQUESTED
                                       // should be power of 2 and > V3_CONFIG_MEM_BLOCK
} __attribute__((packed));


struct v3_core_move_cmd{
    unsigned short vcore_id;
    unsigned short pcore_id;
} __attribute__((packed));

struct v3_mem_move_cmd{
    unsigned long long gpa;
    unsigned short     pcore_id;
} __attribute__((packed));

struct v3_debug_cmd {
    unsigned int core; 
    unsigned int cmd;
} __attribute__((packed));

struct v3_chkpt_info {
    char store[128];
    char url[256]; /* This might need to be bigger... */
    unsigned long long opts;
#define V3_CHKPT_OPT_NONE         0
#define V3_CHKPT_OPT_SKIP_MEM     1  // don't write memory to store
#define V3_CHKPT_OPT_SKIP_DEVS    2  // don't write devices to store
#define V3_CHKPT_OPT_SKIP_CORES   4  // don't write core arch ind data to store
#define V3_CHKPT_OPT_SKIP_ARCHDEP 8  // don't write core arch dep data to store
} __attribute__((packed));

struct v3_reset_cmd {
#define V3_RESET_VM_ALL    0
#define V3_RESET_VM_HRT    1
#define V3_RESET_VM_ROS    2
#define V3_RESET_VM_CORE_RANGE  3
    unsigned int type;
    unsigned int first_core;  // for CORE_RANGE
    unsigned int num_cores;   // for CORE_RANGE
} __attribute__((packed));


struct v3_hw_pci_dev {
    char url[128];
    unsigned int bus;
    unsigned int dev;
    unsigned int func;
} __attribute__((packed));

#define V3VEE_STR "\n\n"                         \
                  "The V3Vee Project (c) 2012\n" \
                  "\thttp://v3vee.org\n"         \
                  "\n\n"
                   
#define v3_usage(fmt, args...)                                \
{                                                             \
    printf(("\nUsage: %s " fmt V3VEE_STR), argv[0], ##args);  \
    exit(0);                                                  \
}


int v3_dev_ioctl (int req, void * arg);
int v3_vm_ioctl  (const char * filename,
                  int req,
                  void * arg);
void * v3_mmap_file (const char * filename, int prot, int flags);
int v3_read_file (int fd, int size, unsigned char * buf);

int launch_vm (const char * filename);
int stop_vm   (const char * filename);

unsigned long v3_hash_buffer (unsigned char * msg, unsigned int len);

/* XML-related structs */
struct cfg_value {
    char * tag;
    char * value;
};

struct xml_option {
    char * tag;
    ezxml_t location;
    struct xml_option * next;
};


struct file_info {
    int size;
    char filename[2048];
    char id[256];
};

struct mem_file_hdr {
    unsigned int file_idx;
    unsigned int file_size;
    unsigned long long file_offset;
    unsigned long file_hash;
};


#endif
