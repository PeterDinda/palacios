/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Kyle C. Hale <kh@u.northwestern.edu> 
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __GUARD_MODS_H__
#define __GUARD_MODS_H__


#ifdef __V3VEE__

#define HCALL_INSTR_LEN 3
#define MAX_BORDER_NESTING 128

enum v3_mod_state {
    INIT,
    PRIV,
    NO_PRIV,
};

/* these need to be updated when
 * load address is received */
struct v3_entry_point {
    char * name;
    addr_t addr;
    uint8_t is_ret; /* indicates that this is a return point */

    struct v3_gm * gm;
    struct list_head er_node;
};


struct v3_gm {
    char * name;
    char * content_hash;
    ulong_t stack_hash;
    uint_t num_entries;
    uint_t hcall_offset;
    uint_t text_size;
    addr_t load_addr;           /* this is a GVA */
    ullong_t id;
    struct v3_entry_point * entry_points;
    enum v3_mod_state state;
    struct list_head priv_list; /* list of privileges associated with this GM */
    struct list_head mod_node;
    struct list_head er_list;
    void * private_data;
    int callback_nesting;  
    int kernel_nesting;
    addr_t r11_stack_kernel[MAX_BORDER_NESTING];
    addr_t r11_stack_callback[MAX_BORDER_NESTING];
    addr_t entry_rsp;
    addr_t exit_rsp;

};

#define V3_GUARD_INIT_HCALL  0x6000
#define V3_BIN_CALL_HCALL    0x6001
#define V3_BOUT_RET_HCALL    0x6002
#define V3_BOUT_CALL_HCALL   0x6003
#define V3_BIN_RET_HCALL     0x6004

struct v3_guarded_mods {
    struct list_head mod_list;
    struct hashtable * mod_id_table;
    struct hashtable * mod_name_table;
    struct hashtable * er_hash;   /* hash for quick lookups on valid entries/returns */
};


#endif /* __V3VEE__ */

unsigned long long
v3_register_gm  (void *  vm, 
                 char *  name,
                 char *  hash,
                 unsigned int hc_off,
                 unsigned int size,
                 unsigned int nentries,
                 unsigned int nprivs,
                 char ** priv_array,
                 void * private_data, 
                 void * entry_points);


#endif /* __GUARD_MODS_H__ */

