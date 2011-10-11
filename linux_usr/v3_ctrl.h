/* 
 * V3 Control header file 
 * (c) Jack lange, 2010
 */

#ifndef _v3_ctrl_h
#define _v3_ctrl_h

#define V3_CREATE_GUEST 12
#define V3_FREE_GUEST 13


#define V3_VM_PAUSE 23
#define V3_VM_CONTINUE 24

#define V3_VM_LAUNCH 25
#define V3_VM_STOP 26
#define V3_VM_LOAD 27
#define V3_VM_SAVE 28

#define V3_ADD_MEMORY 50

#define V3_VM_CONSOLE_CONNECT 20
#define V3_VM_SERIAL_CONNECT 21

#define V3_VM_MOVE_CORE 33

static const char * v3_dev = "/dev/v3vee";

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

#endif
