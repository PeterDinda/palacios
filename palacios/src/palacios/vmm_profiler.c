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
#include <palacios/vmm_profiler.h>
#include <palacios/svm_handler.h>
#include <palacios/vmm_rbtree.h>


struct exit_event {
  uint_t exit_code;
  uint_t exit_count;
  uint_t handler_time;

  struct rb_node tree_node;
};


void v3_init_profiler(struct guest_info * info) {
  info->profiler.total_exits = 0;

  info->profiler.start_time = 0;
  info->profiler.end_time = 0;  

  info->profiler.root.rb_node = NULL;
}



static inline struct exit_event * __insert_event(struct guest_info * info, 
						 struct exit_event * evt) {
  struct rb_node ** p = &(info->profiler.root.rb_node);
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

  v3_rb_insert_color(&(evt->tree_node), &(info->profiler.root));

  return NULL;
}


static struct exit_event * get_exit(struct guest_info * info, uint_t exit_code) {
  struct rb_node * n = info->profiler.root.rb_node;
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
  evt->exit_count = 0;
  evt->handler_time = 0;

  return evt;
}

void v3_profile_exit(struct guest_info * info, uint_t exit_code) {
  uint_t time = (info->profiler.end_time - info->profiler.start_time);
  struct exit_event * evt = get_exit(info, exit_code);

  if (evt == NULL) {
    evt = create_exit(exit_code);
    insert_event(info, evt);
  }

  evt->handler_time += time;
  evt->exit_count++;
  
  info->profiler.total_exits++;
}


void v3_print_profile(struct guest_info * info) {
  struct exit_event * evt = NULL;
  struct rb_node * node = v3_rb_first(&(info->profiler.root));
  
  do {
    evt = rb_entry(node, struct exit_event, tree_node);
    const char * code_str = vmexit_code_to_str(evt->exit_code);

    PrintDebug("%s:%sCnt=%u,%sTime=%u\n", 
	       code_str,
	       (strlen(code_str) > 14) ? "\t" : "\t\t",
	       evt->exit_count,
	       (evt->exit_count >= 100) ? "\t" : "\t\t",
	       evt->handler_time);
	       
  } while ((node = v3_rb_next(node)));
}
