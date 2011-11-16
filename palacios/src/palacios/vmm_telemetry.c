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

#include <palacios/vmm_types.h>
#include <palacios/vmm_telemetry.h>
#include <palacios/svm_handler.h>
#include <palacios/vmm_rbtree.h>
#include <palacios/vmm_sprintf.h>


#ifdef V3_CONFIG_TELEMETRY_GRANULARITY
#define DEFAULT_GRANULARITY V3_CONFIG_TELEMETRY_GRANULARITY
#else 
#define DEFAULT_GRANULARITY 50000
#endif



struct telemetry_cb {
    
    void (*telemetry_fn)(struct v3_vm_info * vm, void * private_data, char * hdr);

    void * private_data;
    struct list_head cb_node;
};


struct exit_event {
    uint_t exit_code;
    uint_t cnt;
    uint64_t handler_time;

    struct rb_node tree_node;
};


static int free_callback(struct v3_vm_info * vm, struct telemetry_cb * cb);
static int free_exit(struct guest_info * core, struct exit_event * event);


void v3_init_telemetry(struct v3_vm_info * vm) {
    struct v3_telemetry_state * telemetry = &(vm->telemetry);

    telemetry->invoke_cnt = 0;
    telemetry->granularity = DEFAULT_GRANULARITY;

    telemetry->prev_tsc = 0;

    INIT_LIST_HEAD(&(telemetry->cb_list));
}

void v3_deinit_telemetry(struct v3_vm_info * vm) {
    struct telemetry_cb * cb = NULL;
    struct telemetry_cb * tmp = NULL;

    list_for_each_entry_safe(cb, tmp, &(vm->telemetry.cb_list), cb_node) {
	free_callback(vm, cb);
    }
}


void v3_init_core_telemetry(struct guest_info * core) {
    struct v3_core_telemetry * telemetry = &(core->core_telem);

    telemetry->exit_cnt = 0;
    telemetry->vmm_start_tsc = 0;

    telemetry->vm_telem = &(core->vm_info->telemetry);

    telemetry->exit_root.rb_node = NULL;
}

void v3_deinit_core_telemetry(struct guest_info * core) {
    struct rb_node * node = v3_rb_first(&(core->core_telem.exit_root));
    struct exit_event * evt = NULL;

    while (node) {
	evt = rb_entry(node, struct exit_event, tree_node);
	node = v3_rb_next(node);

	free_exit(core, evt);
    }
}



static inline struct exit_event * __insert_event(struct guest_info * info, 
						 struct exit_event * evt) {
    struct rb_node ** p = &(info->core_telem.exit_root.rb_node);
    struct rb_node * parent = NULL;
    struct exit_event * tmp_evt = NULL;

    while (*p) {
	parent = *p;
	tmp_evt = rb_entry(parent, struct exit_event, tree_node);

	if (evt->exit_code < tmp_evt->exit_code) {
	    p = &(*p)->rb_left;
	} else if (evt->exit_code > tmp_evt->exit_code) {
	    p = &(*p)->rb_right;
	} else {
	    return tmp_evt;
	}
    }
    rb_link_node(&(evt->tree_node), parent, p);

    return NULL;
}

static inline struct exit_event * insert_event(struct guest_info * info, 
					       struct exit_event * evt) {
    struct exit_event * ret;

    if ((ret = __insert_event(info, evt))) {
	return ret;
    }

    v3_rb_insert_color(&(evt->tree_node), &(info->core_telem.exit_root));

    return NULL;
}


static struct exit_event * get_exit(struct guest_info * info, uint_t exit_code) {
    struct rb_node * n = info->core_telem.exit_root.rb_node;
    struct exit_event * evt = NULL;

    while (n) {
	evt = rb_entry(n, struct exit_event, tree_node);
    
	if (exit_code < evt->exit_code) {
	    n = n->rb_left;
	} else if (exit_code > evt->exit_code) {
	    n = n->rb_right;
	} else {
	    return evt;
	}
    }

    return NULL;
}


static inline struct exit_event * create_exit(uint_t exit_code) {
    struct exit_event * evt = V3_Malloc(sizeof(struct exit_event));

    evt->exit_code = exit_code;
    evt->cnt = 0;
    evt->handler_time = 0;

    return evt;
}



static int free_exit(struct guest_info * core, struct exit_event * evt) {
    v3_rb_erase(&(evt->tree_node), &(core->core_telem.exit_root));
    V3_Free(evt);
    return 0;
}


void v3_telemetry_start_exit(struct guest_info * info) {
    rdtscll(info->core_telem.vmm_start_tsc);
}


void v3_telemetry_end_exit(struct guest_info * info, uint_t exit_code) {
    struct v3_core_telemetry * telemetry = &(info->core_telem);
    struct exit_event * evt = NULL;
    uint64_t end_tsc = 0;

    rdtscll(end_tsc);

    evt = get_exit(info, exit_code);

    if (evt == NULL) {
	evt = create_exit(exit_code);
	insert_event(info, evt);
    }

    evt->handler_time += end_tsc - telemetry->vmm_start_tsc;

    evt->cnt++;
    telemetry->exit_cnt++;



    // check if the exit count has expired
    if ((telemetry->exit_cnt % telemetry->vm_telem->granularity) == 0) {
	v3_print_telemetry(info->vm_info, info);
    }
}




void v3_add_telemetry_cb(struct v3_vm_info * vm, 
			 void (*telemetry_fn)(struct v3_vm_info * vm, void * private_data, char * hdr),
			 void * private_data) {
    struct v3_telemetry_state * telemetry = &(vm->telemetry);
    struct telemetry_cb * cb = (struct telemetry_cb *)V3_Malloc(sizeof(struct telemetry_cb));

    cb->private_data = private_data;
    cb->telemetry_fn = telemetry_fn;

    list_add(&(cb->cb_node), &(telemetry->cb_list));
}



static int free_callback(struct v3_vm_info * vm, struct telemetry_cb * cb) {
    list_del(&(cb->cb_node));
    V3_Free(cb);

    return 0;
}


static void telemetry_header(struct v3_vm_info *vm, char *hdr_buf, int len)
{
    struct v3_telemetry_state * telemetry = &(vm->telemetry);
    snprintf(hdr_buf, len, "telem.%d>", telemetry->invoke_cnt);
}

static void print_telemetry_start(struct v3_vm_info *vm, char *hdr_buf)
{
    struct v3_telemetry_state * telemetry = &(vm->telemetry);
    uint64_t invoke_tsc = 0;
    rdtscll(invoke_tsc);
    V3_Print("%stelemetry window tsc cnt: %d\n", hdr_buf, (uint32_t)(invoke_tsc - telemetry->prev_tsc));
    telemetry->prev_tsc = invoke_tsc;
}

static void print_telemetry_end(struct v3_vm_info *vm, char *hdr_buf)
{
    V3_Print("%s Telemetry done\n", hdr_buf);
}

static void print_core_telemetry(struct guest_info * core, char *hdr_buf)
{
    struct exit_event * evt = NULL;
    struct rb_node * node = v3_rb_first(&(core->core_telem.exit_root));

    V3_Print("Exit information for Core %d\n", core->vcpu_id);
    
    if (!node) { 
    	V3_Print("No information yet for this core\n");
    	return;
    }

    do {
	 evt = rb_entry(node, struct exit_event, tree_node);
	 const char * code_str = vmexit_code_to_str(evt->exit_code);
	    
	 V3_Print("%s%s:%sCnt=%u,%sAvg. Time=%u\n", 
		  hdr_buf, code_str,
		  (strlen(code_str) > 13) ? "\t" : "\t\t",
		  evt->cnt,
		  (evt->cnt >= 100) ? "\t" : "\t\t",
		  (uint32_t)(evt->handler_time / evt->cnt));
    } while ((node = v3_rb_next(node)));
    return;
}

void v3_print_core_telemetry(struct guest_info * core ) {
    struct v3_vm_info *vm = core->vm_info;
    struct v3_telemetry_state * telemetry = &(vm->telemetry);
    char hdr_buf[32];
    
    telemetry_header(vm, hdr_buf, 32);
    telemetry->invoke_cnt++; // XXX this increment isn't atomic and probably should be

    print_telemetry_start(vm, hdr_buf);
    print_core_telemetry(core, hdr_buf);
    print_telemetry_end(vm, hdr_buf);

    return;
}

static void telemetry_callbacks(struct v3_vm_info * vm, char *hdr_buf)
{
    struct v3_telemetry_state * telemetry = &(vm->telemetry);
    // Registered callbacks
    {
	struct telemetry_cb * cb = NULL;

	list_for_each_entry(cb, &(telemetry->cb_list), cb_node) {
	    cb->telemetry_fn(vm, cb->private_data, hdr_buf);
	}
    }
}

void v3_print_global_telemetry(struct v3_vm_info * vm) {
    struct v3_telemetry_state * telemetry = &(vm->telemetry);
    char hdr_buf[32];

    telemetry_header(vm, hdr_buf, 32);
    telemetry->invoke_cnt++; // XXX this increment isn't atomic and probably should be

    print_telemetry_start( vm, hdr_buf );
    telemetry_callbacks( vm, hdr_buf );
    print_telemetry_end( vm, hdr_buf );
}

void v3_print_telemetry(struct v3_vm_info * vm, struct guest_info * core )
{
    struct v3_telemetry_state * telemetry = &(vm->telemetry);
    char hdr_buf[32];
    
    telemetry_header(vm, hdr_buf, 32);
    telemetry->invoke_cnt++; // XXX this increment isn't atomic and probably should be

    print_telemetry_start(vm, hdr_buf);
    print_core_telemetry(core, hdr_buf);
    telemetry_callbacks(vm, hdr_buf);
    print_telemetry_end(vm, hdr_buf);

    return;
}
