/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_HOST_EVENTS_H__
#define __VMM_HOST_EVENTS_H__


struct v3_keyboard_event {
    unsigned char status;
    unsigned char scan_code;
};

struct v3_mouse_event {
    unsigned char data[3];
};

struct v3_timer_event {
    unsigned int period_us;
};

struct v3_serial_event {
    unsigned char data[128];
    unsigned int len;
};

struct v3_console_event {
    unsigned int cmd;
};

struct v3_packet_event {
    unsigned char * pkt;
    unsigned int size;
};

#ifdef __V3VEE__

#include <palacios/vmm_list.h>

struct v3_vm_info;

typedef enum { HOST_KEYBOARD_EVT, 
	       HOST_MOUSE_EVT, 
	       HOST_TIMER_EVT,
	       HOST_CONSOLE_EVT,
	       HOST_SERIAL_EVT} v3_host_evt_type_t;


union v3_host_event_handler {
    int (*keyboard_handler)(struct v3_vm_info * vm, struct v3_keyboard_event * evt, void * priv_data);
    int (*mouse_handler)(struct v3_vm_info * vm, struct v3_mouse_event * evt, void * priv_data);
    int (*timer_handler)(struct v3_vm_info * vm, struct v3_timer_event * evt, void * priv_data);
    int (*serial_handler)(struct v3_vm_info * vm, struct v3_serial_event * evt, void * priv_data);
    int (*console_handler)(struct v3_vm_info * vm, struct v3_console_event * evt, void * priv_data);
};


struct v3_host_event_hook {
    union v3_host_event_handler cb;
    void * private_data;
    struct list_head link;
};



struct v3_host_events {
    struct list_head keyboard_events;
    struct list_head mouse_events;
    struct list_head timer_events;
    struct list_head serial_events;
    struct list_head console_events;
};



int v3_init_host_events(struct v3_vm_info * vm);
int v3_deinit_host_events(struct v3_vm_info * vm);

#define V3_HOST_EVENT_HANDLER(cb) ((union v3_host_event_handler)cb)

int v3_hook_host_event(struct v3_vm_info * vm, 
		       v3_host_evt_type_t event_type, 
		       union v3_host_event_handler cb, 
		       void * private_data);

#endif /* ! __V3VEE__ */



int v3_deliver_keyboard_event(struct v3_vm_info * vm, struct v3_keyboard_event * evt);
int v3_deliver_mouse_event(struct v3_vm_info * vm, struct v3_mouse_event * evt);
int v3_deliver_timer_event(struct v3_vm_info * vm, struct v3_timer_event * evt);
int v3_deliver_serial_event(struct v3_vm_info * vm, struct v3_serial_event * evt);
int v3_deliver_console_event(struct v3_vm_info * vm, struct v3_console_event * evt);


#endif
