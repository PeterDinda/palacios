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


#ifndef __VMM_MULTITREE_H__
#define __VMM_MULTITREE_H__

#ifdef __V3VEE__

#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_rbtree.h>



struct v3_mtree {
    char name[50];

    union {
	uint8_t flags;
	struct {
	    uint8_t subtree        : 1;
	    uint8_t rsvd           : 7;
	} __attribute__((packed));
    } __attribute__((packed));

    uint8_t user_flags;

    uint64_t size;

    union {
	struct rb_root child;
	void * value;
    } __attribute__((packed));

    struct rb_node tree_node;
};





struct v3_mtree * v3_mtree_create_node(struct v3_mtree * root, char * name);
struct v3_mtree * v3_mtree_create_value(struct v3_mtree * root, char * name, 
					uint64_t size, void * value);
struct v3_mtree * v3_mtree_create_subtree(struct v3_mtree * root, char * name);

struct v3_mtree * v3_mtree_find_node(struct v3_mtree * root, char * name);
struct v3_mtree * v3_mtree_find_subtree(struct v3_mtree * root, char * name);
struct v3_mtree * v3_mtree_find_value(struct v3_mtree * root, char * name);


struct v3_mtree * v3_mtree_first_child(struct v3_mtree * root);
struct v3_mtree * v3_mtree_next_node(struct v3_mtree * node);

void v3_mtree_free_tree(struct v3_mtree * root);
void v3_mtree_free_node(struct v3_mtree * root, char * name);




#endif

#endif
