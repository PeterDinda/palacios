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



#include <palacios/vmm_events.h>
#include <palacios/vm_guest.h>



int v3_init_events(struct v3_vm_info * vm) {
    struct v3_event_map * map = &(vm->event_map);
    int i = 0;

    map->events = V3_Malloc(sizeof(struct list_head) * (V3_EVENT_INVALID+1));

    if (map->events == NULL) {
	PrintError(vm, VCORE_NONE, "Error: could not allocate event map\n");
	return -1;
    }

    // dead code if there are no events, but this is correct
    for (i = 0; i < V3_EVENT_INVALID; i++) {
	INIT_LIST_HEAD(&(map->events[i]));
    }

    return 0;
}

int v3_deinit_events(struct v3_vm_info * vm) {
    struct v3_event_map * map = &(vm->event_map);
    int i = 0;


    // dead code if there are no events, but this is correct
    for (i = 0; i < V3_EVENT_INVALID; i++) {
	if (!list_empty(&(map->events[i]))) {
	    struct v3_notifier * tmp_notifier = NULL;
	    struct v3_notifier * safe_notifier = NULL;
	    PrintError(vm, VCORE_NONE, "Found orphan notifier for event %d. Probable memory leak detected.\n", i);
	    
	    list_for_each_entry_safe(tmp_notifier, safe_notifier, &(map->events[i]), node) {
		list_del(&(tmp_notifier->node));
		V3_Free(tmp_notifier);
	    }
	}
    }


    V3_Free(map->events);

    return 0;

}


struct v3_notifier * v3_subscribe_event(struct v3_vm_info * vm, 
		     v3_event_type_t event_type, 
		     void (*notify)(struct guest_info * core, 
				    v3_event_type_t event_type,
				    void * priv_data, 
				    void * event_data),
		     void * priv_data, 
		     struct guest_info * current_core) {
    struct v3_event_map * map = &(vm->event_map);
    struct v3_notifier * notifier = NULL;

    if (event_type >= V3_EVENT_INVALID) {
	PrintError(vm, VCORE_NONE, "Tried to request illegal event (%d)\n", event_type);
	return NULL;
    }

    notifier = V3_Malloc(sizeof(struct v3_notifier));

    if (notifier == NULL) {
	PrintError(vm, VCORE_NONE, "Error: Could not allocate notifier\n");
	return NULL;
    }

    memset(notifier, 0, sizeof(struct v3_notifier));
    
    notifier->notify = notify;
    notifier->priv_data = priv_data;
    notifier->event_type = event_type;

    while (v3_raise_barrier(vm, current_core) == -1);
    list_add(&(notifier->node), &(map->events[event_type]));
    v3_lower_barrier(vm);

    return notifier;;
}


int v3_unsubscribe_event(struct v3_vm_info * vm, struct v3_notifier * notifier, 
			 struct guest_info * current_core) {
    struct v3_event_map * map = &(vm->event_map);
    struct v3_notifier * tmp_notifier = NULL;
    struct v3_notifier * safe_notifier = NULL;    

    if (notifier == NULL) {
	PrintError(vm, VCORE_NONE, "Could not unsubscribe invalid event notifier\n");
	return -1;
    }
    
    if (notifier->event_type >= V3_EVENT_INVALID) {
	PrintError(vm, VCORE_NONE, "Could not unsubscribe from invalid event\n");
	return -1;
    }

    while (v3_raise_barrier(vm, current_core) == -1);
    list_for_each_entry_safe(tmp_notifier, safe_notifier, &(map->events[notifier->event_type]), node) {
	if (tmp_notifier == notifier) {
	    list_del(&(tmp_notifier->node));
	    V3_Free(tmp_notifier);
	}
    }
    v3_lower_barrier(vm);

    return 0;
}
