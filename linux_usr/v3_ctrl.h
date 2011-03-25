/* 
 * V3 Control header file 
 * (c) Jack lange, 2010
 */

#ifndef _v3_ctrl_h
#define _v3_ctrl_h

#define V3_START_GUEST 10
#define V3_ADD_MEMORY 50
#define V3_START_NETWORK 60

#define V3_VM_CONSOLE_CONNECT 20
#define V3_VM_SERIAL_CONNECT 21
#define V3_VM_STOP 22

static const char * v3_dev = "/dev/v3vee";

struct v3_guest_img {
    unsigned long long size;
    void * guest_data;
    char name[128];
};


struct v3_mem_region {
    unsigned long long base_addr;
    unsigned long long num_pages;
};

#endif
