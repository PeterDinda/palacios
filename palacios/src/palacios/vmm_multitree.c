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


#include <palacios/vmm_multitree.h>
#include <palacios/vmm_string.h>

#include <palacios/vmm_rbtree.h>

static inline 
struct v3_mtree * __insert_mtree_node(struct v3_mtree * root, struct v3_mtree * node) {
    struct rb_node ** p = &(root->child.rb_node);
    struct rb_node * parent = NULL;
    struct v3_mtree * tmp_node;

    while (*p) {
	int ret = 0;
	parent = *p;
	tmp_node = rb_entry(parent, struct v3_mtree, tree_node);

	ret = strcmp(node->name, tmp_node->name);

	if (ret < 0) {
	    p = &(*p)->rb_left;
	} else if (ret > 0) {
	    p = &(*p)->rb_right;
	} else {
	    return tmp_node;
	}
    }

    rb_link_node(&(node->tree_node), parent, p);
  
    return NULL;
}



struct v3_mtree * v3_mtree_create_node(struct v3_mtree * root, char * name) {
    struct v3_mtree * node = (struct v3_mtree *)V3_Malloc(sizeof(struct v3_mtree));
    struct v3_mtree * ret = NULL;


    memset(node, 0, sizeof(struct v3_mtree));
    strncpy(node->name, name, 50);

    if ((ret = __insert_mtree_node(root, node))) {
	V3_Free(node);
	return NULL;
    }


    v3_rb_insert_color(&(node->tree_node), &(root->child));

    return node;
}


struct v3_mtree * v3_mtree_create_subtree(struct v3_mtree * root, char * name) {
    struct v3_mtree * node = v3_mtree_create_node(root, name);

    PrintDebug("Creating Subtree %s\n", name);

    if (node == NULL) {
	return NULL;
    }

    node->subtree = 1;

    return node;
}


struct v3_mtree * v3_mtree_create_value(struct v3_mtree * root, char * name, 
					uint64_t size, void * value) {
    struct v3_mtree * node = v3_mtree_create_node(root, name);

    PrintDebug("Creating value %s\n", name);


    if (node == NULL) {
	return NULL;
    }

    node->size = size;
    node->value = value;

    return node;
}



struct v3_mtree * v3_mtree_find_node(struct v3_mtree * root, char * name) {
    struct rb_node * n = root->child.rb_node;
    struct v3_mtree * tmp_node = NULL;

    if (root->subtree == 0) {
	PrintError("Searching for node on a non-root mtree (search=%s), root=%s\n", name, root->name);
	return NULL;
    }
   
    while (n) {
	int ret = 0;
	tmp_node = rb_entry(n, struct v3_mtree, tree_node);
	ret = strcmp(tmp_node->name, name);

	if (ret < 0) {
	    n = n->rb_left;
	} else if (ret > 0) {
	    n = n->rb_right;
	} else {
	    return tmp_node;
	}	
    }

    return NULL;
}


struct v3_mtree * v3_mtree_find_subtree(struct v3_mtree * root, char * name) {
    struct v3_mtree * node = v3_mtree_find_node(root, name);
    
    if (node->subtree == 0) {
	return NULL;
    }

    return node;
}


struct v3_mtree * v3_mtree_find_value(struct v3_mtree * root, char * name) {
    struct v3_mtree * node= v3_mtree_find_node(root, name);
    
    if (node->subtree == 1) {
	return NULL;
    }

    return node;
}
