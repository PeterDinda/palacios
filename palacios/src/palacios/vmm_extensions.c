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

#include <palacios/vmm_extensions.h>

#include <palacios/vmm_hashtable.h>


static struct hashtable * ext_table = NULL;


static uint_t ext_hash_fn(addr_t key) {
    char * name = (char *)key;
    return v3_hash_buffer((uint8_t *)name, strlen(name));
}

static int ext_eq_fn(addr_t key1, addr_t key2) {
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;

    return (strcmp(name1, name2) == 0);
}



int V3_init_extensions() {
    extern struct v3_extension_impl * __start__v3_extensions[];
    extern struct v3_extension_impl * __stop__v3_extensions[];
    struct v3_extension_impl ** tmp_ext = __start__v3_extensions;
    int i = 0;

    ext_table = v3_create_htable(0, ext_hash_fn, ext_eq_fn);

    while (tmp_ext != __stop__v3_extensions) {
	V3_Print("Registering Extension (%s)\n", (*tmp_ext)->name);

	if (v3_htable_search(ext_table, (addr_t)((*tmp_ext)->name))) {
	    PrintError("Multiple instances of Extension (%s)\n", (*tmp_ext)->name);
	    return -1;
	}

	if (v3_htable_insert(ext_table, (addr_t)((*tmp_ext)->name), (addr_t)(*tmp_ext)) == 0) {
	    PrintError("Could not register Extension (%s)\n", (*tmp_ext)->name);
	    return -1;
	}

	tmp_ext = &(__start__v3_extensions[++i]);
    }

    return 0;
}


int V3_deinit_extensions() {
    v3_free_htable(ext_table, 0, 0);
    return 0;
}


