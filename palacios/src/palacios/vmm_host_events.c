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

#include <palacios/vmm.h>
#include <palacios/vmm_host_events.h>
#include <palacios/vm_guest.h>

int v3_init_host_events(struct v3_vm_info * vm) {
    struct v3_host_events * host_evts = &(vm->host_event_hooks);

    INIT_LIST_HEAD(&(host_evts->keyboard_events));
    INIT_LIST_HEAD(&(host_evts->mouse_events));
    INIT_LIST_HEAD(&(host_evts->timer_events));
    INIT_LIST_HEAD(&(host_evts->serial_events));
    INIT_LIST_HEAD(&(host_evts->console_events));

    return 0;
}

int v3_deinit_host_events(struct v3_vm_info * vm) {
    struct v3_host_events * host_evts = &(vm->host_event_hooks);
    struct v3_host_event_hook * hook = NULL;
    struct v3_host_event_hook * tmp = NULL;

    list_for_each_entry_safe(hook, tmp, &(host_evts->keyboard_events), link) {
	list_del(&(hook->link));
	V3_Free(hook);
    }

    list_for_each_entry_safe(hook, tmp, &(host_evts->mouse_events), link) {
	list_del(&(hook->link));
	V3_Free(hook);
    }


    list_for_each_entry_safe(hook, tmp, &(host_evts->timer_events), link) {
	list_del(&(hook->link));
	V3_Free(hook);
    }


    list_for_each_entry_safe(hook, tmp, &(host_evts->serial_events), link) {
	list_del(&(hook->link));
	V3_Free(hook);
    }


    list_for_each_entry_safe(hook, tmp, &(host_evts->console_events), link) {
	list_del(&(hook->link));
	V3_Free(hook);
    }

    return 0;
}


int v3_hook_host_event(struct v3_vm_info * vm, 
		       v3_host_evt_type_t event_type, 
		       union v3_host_event_handler cb, 
		       void * private_data) {
  
    struct v3_host_events * host_evts = &(vm->host_event_hooks);
    struct v3_host_event_hook * hook = NULL;

    hook = (struct v3_host_event_hook *)V3_Malloc(sizeof(struct v3_host_event_hook));
    if (hook == NULL) {
	PrintError("Could not allocate event hook\n");
	return -1;
    }

    hook->cb = cb;
    hook->private_data = private_data;

    switch (event_type)  {
	case HOST_KEYBOARD_EVT:
	    list_add(&(hook->link), &(host_evts->keyboard_events));
	    break;
	case HOST_MOUSE_EVT:
	    list_add(&(hook->link), &(host_evts->mouse_events));
	    break;
	case HOST_TIMER_EVT:
	    list_add(&(hook->link), &(host_evts->timer_events));
	    break;
	case HOST_SERIAL_EVT:
	    list_add(&(hook->link), &(host_evts->serial_events));
	    break;
	case HOST_CONSOLE_EVT:
	    list_add(&(hook->link), &(host_evts->console_events));
	    break;
    }

    return 0;
}


int v3_deliver_keyboard_event(struct v3_vm_info * vm, 
			      struct v3_keyboard_event * evt) {
    struct v3_host_events * host_evts = NULL;
    struct v3_host_event_hook * hook = NULL;


    host_evts = &(vm->host_event_hooks);

    if (vm->run_state != VM_RUNNING) {
	return -1;
    }

    list_for_each_entry(hook, &(host_evts->keyboard_events), link) {
	if (hook->cb.keyboard_handler(vm, evt, hook->private_data) == -1) {
	    return -1;
	}
    }

    return 0;
}


int v3_deliver_mouse_event(struct v3_vm_info * vm, 
			   struct v3_mouse_event * evt) {
    struct v3_host_events * host_evts = NULL;
    struct v3_host_event_hook * hook = NULL;


    host_evts = &(vm->host_event_hooks);

    if (vm->run_state != VM_RUNNING) {
	return -1;
    }

    list_for_each_entry(hook, &(host_evts->mouse_events), link) {
	if (hook->cb.mouse_handler(vm, evt, hook->private_data) == -1) {
	    return -1;
	}
    }

    return 0;
}


int v3_deliver_timer_event(struct v3_vm_info * vm, 
			   struct v3_timer_event * evt) {
    struct v3_host_events * host_evts = NULL;
    struct v3_host_event_hook * hook = NULL;


    host_evts = &(vm->host_event_hooks);

    if (vm->run_state != VM_RUNNING) {
	return -1;
    }

    list_for_each_entry(hook, &(host_evts->timer_events), link) {
	if (hook->cb.timer_handler(vm, evt, hook->private_data) == -1) {
	    return -1;
	}
    }

    return 0;
}

int v3_deliver_serial_event(struct v3_vm_info * vm, 
			    struct v3_serial_event * evt) {
    struct v3_host_events * host_evts = NULL;
    struct v3_host_event_hook * hook = NULL;


    host_evts = &(vm->host_event_hooks);

    if (vm->run_state != VM_RUNNING) {
	return -1;
    }

    list_for_each_entry(hook, &(host_evts->serial_events), link) {
	if (hook->cb.serial_handler(vm, evt, hook->private_data) == -1) {
	    return -1;
	}
    }

    return 0;
}



int v3_deliver_console_event(struct v3_vm_info * vm, 
			     struct v3_console_event * evt) {
    struct v3_host_events * host_evts = NULL;
    struct v3_host_event_hook * hook = NULL;


    host_evts = &(vm->host_event_hooks);

    if (vm->run_state != VM_RUNNING) {
	return -1;
    }

    list_for_each_entry(hook, &(host_evts->console_events), link) {
	if (hook->cb.console_handler(vm, evt, hook->private_data) == -1) {
	    return -1;
	}
    }

    return 0;
}

