/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_EVENTS_H__
#define __VMM_EVENTS_H__

#ifdef __V3VEE__

#include <palacios/vmm_types.h>
#include <palacios/vmm_list.h>

struct guest_info;
struct v3_vm_info;

typedef enum {        
  /* First event must be zero */
  V3_EVENT_INVALID /* This entry must always be last */
} v3_event_type_t;


struct v3_event_map {
    struct list_head * events; // array of events

};


int v3_init_events(struct v3_vm_info * vm);
int v3_deinit_events(struct v3_vm_info * vm);


struct v3_notifier {

    v3_event_type_t event_type;

    void (*notify)(struct guest_info * core, 
		   v3_event_type_t event_type,
		   void * priv_data, 
		   void * event_data);
    void * priv_data;
    
    struct list_head node;

};


struct v3_notifier * v3_subscribe_event(struct v3_vm_info * vm, 
				       v3_event_type_t event_type, 
				       void (*notify)(struct guest_info * core, 
						      v3_event_type_t event_type,
						      void * priv_data, 
						      void * event_data),
				       void * priv_data, 
				       struct guest_info * current_core);

int v3_unsubscribe_event(struct v3_vm_info * vm, struct v3_notifier * notifier, 
			 struct guest_info * current_core);



#include <palacios/vm_guest.h>

#ifdef __VM_EVENTS_H2___ /* Just ignore the man behind the curtain.... */

static void inline v3_dispatch_event(struct guest_info * core, 
				     v3_event_type_t event_type, 
				     void * event_data) {
    struct v3_notifier * tmp_notifier = NULL;

    if (event_type >= V3_EVENT_INVALID) {
	PrintError(info->vm_info, info, "Tried to dispatch illegal event (%d)\n", event_type);
	return;
    }

    list_for_each_entry(tmp_notifier, &(core->vm_info->event_map.events[event_type]), node) {
	tmp_notifier->notify(core, event_type, tmp_notifier->priv_data, event_data);
    }

}

#endif
#define __VM_EVENTS_H2___





#endif

#endif
