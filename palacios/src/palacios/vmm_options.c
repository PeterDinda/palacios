/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2013, Patrick G. Bridges <bridges@cs.unm.edu> 
 * Copyright (c) 2013, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Patrick G. Bridges <bridges@cs.unm.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmm.h>
#include <palacios/vmm_config.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_options.h>

/* Options are space-separated values of the form "X=Y", for example
 * scheduler=EDF CPUs=1,2,3,4
 * THe following code pushes them into a hashtable for each of access
 * by other code. Storage is allocated for keys and values as part
 * of this process. XXX Need a way to deallocate this storage if the 
 * module is removed XXX
 */
static char *option_storage;
static struct hashtable *option_table;
static char *truevalue = "true";

static uint_t option_hash_fn(addr_t key) {
    char * name = (char *)key;
    return v3_hash_buffer((uint8_t *)name, strlen(name));
}
static int option_eq_fn(addr_t key1, addr_t key2) {
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;

    return (strcmp(name1, name2) == 0);
}

// need to allocate these cleanly and separately so we can easily recover the
// storage
static int option_insert(char *key, char *val)
{
    char *k, *v;

    k = V3_Malloc(strlen(key)+1);

    if (!k) { 
	return -1;
    }
    
    v = V3_Malloc(strlen(val)+1);

    if (!v) { 
	V3_Free(k);
	return -1;
    }

    strcpy(k,key);
    strcpy(v,val);

    if (!v3_htable_insert(option_table, (addr_t)k, (addr_t)v)) {
	V3_Free(v);
	V3_Free(k);
	return -1;
    }

    return 0;
}


void v3_parse_options(char *options)
{
    char *currKey = NULL, *currVal = NULL;
    int parseKey = 1;
    int len;
    char *c;

    option_table = v3_create_htable(0, option_hash_fn, option_eq_fn);

    if (!options) {
	return; 
    }

    len = strlen(options);
    option_storage = V3_Malloc(len + 1);
    strcpy(option_storage, options);
    c = option_storage;

    while (c && *c) {
	/* Skip whitespace */
        if ((*c == ' ')
	    || (*c == '\t') || (*c == ',')) {
	    *c = 0;
	    if (currKey) {
		if (!currVal) {
		    currVal = truevalue;
		}
		option_insert(currKey, currVal);
		parseKey = 1;
		currKey = NULL;
		currVal = NULL;
	    } 
	    c++;
	} else if (parseKey) {
	    if (!currKey) {
		currKey = c;
	    } 
	    if (*c == '=') {
	        parseKey = 0;
		*c = 0;
	    }
	    c++;
	} else /* !parseKey */ {
	    if (!currVal) {
		currVal = c;
	    }
	    c++;
	}
    }
    if (currKey) {
	if (!currVal) {
	    currVal = truevalue;
	} 
	option_insert(currKey, currVal);
    }
    
    V3_Free(option_storage);

    return;
}

char *v3_lookup_option(char *key) {
    return (char *)v3_htable_search(option_table, (addr_t)(key));
}


void v3_deinit_options()
{
    v3_free_htable(option_table,1,1); // will free keys and values
}
