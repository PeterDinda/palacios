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


#ifdef CONFIG_TELEMETRY_GRANULARITY
#define DEFAULT_GRANULARITY CONFIG_TELEMETRY_GRANULARITY
#else 
#define DEFAULT_GRANULARITY 50000
#endif



struct telemetry_cb {
    
    void (*telemetry_fn)(struct guest_info * info, void * private_data, char * hdr);

    void * private_data;
    struct list_head cb_node;
};


struct exit_event {
    uint_t exit_code;
    uint_t cnt;
    uint64_t handler_time;

    struct rb_node tree_node;
};


void v3_init_telemetry(struct guest_info * info) {
    struct v3_telemetry_state * telemetry = &(info->telemetry);

    telemetry->exit_cnt = 0;
    telemetry->vmm_start_tsc = 0;
    telemetry->prev_tsc = 0;
    telemetry->invoke_cnt = 0;
    telemetry->granularity = DEFAULT_GRANULARITY;


    telemetry->exit_root.rb_node = NULL;
    INIT_LIST_HEAD(&(telemetry->cb_list));
}



static inline struct exit_event * __insert_event(struct guest_info * info, 
						 struct exit_event * evt) {
    struct rb_node ** p = &(info->telemetry.exit_root.rb_node);
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

    v3_rb_insert_color(&(evt->tree_node), &(info->telemetry.exit_root));

    return NULL;
}


static struct exit_event * get_exit(struct guest_info * info, uint_t exit_code) {
    struct rb_node * n = info->telemetry.exit_root.rb_node;
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

void v3_telemetry_start_exit(struct guest_info * info) {
    rdtscll(info->telemetry.vmm_start_tsc);
}


void v3_telemetry_end_exit(struct guest_info * info, uint_t exit_code) {
    struct v3_telemetry_state * telemetry = &(info->telemetry);
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
    if ((telemetry->exit_cnt % telemetry->granularity) == 0) {
	v3_print_telemetry(info);
    }
}




void v3_add_telemetry_cb(struct guest_info * info, 
			 void (*telemetry_fn)(struct guest_info * info, void * private_data, char * hdr),
			 void * private_data) {
    struct v3_telemetry_state * telemetry = &(info->telemetry);
    struct telemetry_cb * cb = (struct telemetry_cb *)V3_Malloc(sizeof(struct telemetry_cb));

    cb->private_data = private_data;
    cb->telemetry_fn = telemetry_fn;

    list_add(&(cb->cb_node), &(telemetry->cb_list));
}



void v3_print_telemetry(struct guest_info * info) {
    struct v3_telemetry_state * telemetry = &(info->telemetry);
    uint64_t invoke_tsc = 0;
    char hdr_buf[32];

    rdtscll(invoke_tsc);

    snprintf(hdr_buf, 32, "telem.%d>", telemetry->invoke_cnt++);

    V3_Print("%stelemetry window tsc cnt: %d\n", hdr_buf, (uint32_t)(invoke_tsc - telemetry->prev_tsc));

    // Exit Telemetry
    {
	struct exit_event * evt = NULL;
	struct rb_node * node = v3_rb_first(&(info->telemetry.exit_root));
	
	do {
	    evt = rb_entry(node, struct exit_event, tree_node);
	    const char * code_str = vmexit_code_to_str(evt->exit_code);
	    
	    V3_Print("%s%s:%sCnt=%u,%sAvg. Time=%u\n", 
		     hdr_buf,
		     code_str,
		     (strlen(code_str) > 13) ? "\t" : "\t\t",
		     evt->cnt,
		     (evt->cnt >= 100) ? "\t" : "\t\t",
		     (uint32_t)(evt->handler_time / evt->cnt));
	} while ((node = v3_rb_next(node)));
    }


    // Registered callbacks
    {
	struct telemetry_cb * cb = NULL;

	list_for_each_entry(cb, &(telemetry->cb_list), cb_node) {
	    cb->telemetry_fn(info, cb->private_data, hdr_buf);
	}
    }

    telemetry->prev_tsc = invoke_tsc;

    V3_Print("%s Telemetry done\n", hdr_buf);

}
