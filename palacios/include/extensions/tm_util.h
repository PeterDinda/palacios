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
 * Author:  Maciek Swiech <dotpyfe@u.northwestern.edu>
 *          Marcel Flores <marcel-flores@u.northwestern.edu>
 *          Zachary Bischof <zbischof@u.northwestern.edu>
 *          Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __TM_UTIL_H
#define __TM_UTIL_H

// new printing macros
// used like TM_ERR(core, ABORT, "couldnt wangle the dangle");

#define TM_ERR(core, label, msg, ...) \
    do {                              \
        typeof (core) _core = (core);  \
        PrintError(_core->vm_info, _core, "TM %10s | " msg , #label, ##__VA_ARGS__); \
    } while (0);

#define TM_DBG(core, label, msg, ...) \
    do {                              \
        typeof (core) _core = (core);  \
        PrintDebug(_core->vm_info, _core, "TM %10s | " msg , #label, ##__VA_ARGS__); \
    } while (0);

struct mem_op {
    addr_t   guest_addr;
    uint64_t data;
    int      current;

    struct list_head op_node;
};

void v3_clear_tm_lists(struct v3_trans_mem * tm);

// note memory location touched in the list, avoids duplicate creation
int add_mem_op_to_list(struct list_head * list, addr_t guest_addr);

// searches for address in the list, returns pointer to elt if found, null
struct mem_op * list_contains_guest_addr(struct list_head * list, addr_t guest_addr);

// checks for current = 0 in list, updates to new value from staging page
int update_list(struct v3_trans_mem * tm, struct list_head * list);

// writes value to staging page, sets current = 0
int stage_entry(struct v3_trans_mem * tm, struct list_head * list, addr_t guest_addr);

// adds entry to list if it doesnt exist, used in copying
int copy_add_entry(struct list_head * list, addr_t guest_addr, uint64_t data);

// if TM block succesfully finishes, commits the list
int commit_list(struct guest_info * core, struct v3_trans_mem * tm);

// copy other lists to core's global lists
int v3_copy_lists(struct guest_info *core);

// set TM_MODE to TM_ON
int v3_set_tm(struct v3_trans_mem * tm);

// set TM_MODE to TM_OFF, clear data structures
int v3_clr_tm(struct v3_trans_mem * tm);

// clear the vtlb on a core
int v3_clr_vtlb(struct guest_info *core);

// set TM_STATE to TM_ABORT
int v3_tm_set_abrt(struct v3_trans_mem * tm);

// free the staging page of the core
int v3_free_staging_page(struct v3_trans_mem * tm);

#endif
