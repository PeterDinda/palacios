/* 
 * DebugFS interface
 * (c) Jack Lange, 2011
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <palacios/vmm_inspector.h>

#include "palacios.h"

struct dentry * v3_dir = NULL;


int palacios_init_debugfs( void ) {

    v3_dir = debugfs_create_dir("v3vee", NULL);

    if (IS_ERR(v3_dir)) {
	printk("Error creating v3vee debugfs directory\n");
	return -1;
    }

    return 0;
}


int palacios_deinit_debugfs( void ) {
    debugfs_remove(v3_dir);
    return 0;
}



static int dfs_register_tree(struct dentry * dir, v3_inspect_node_t * root) {
    v3_inspect_node_t * tmp_node = v3_inspection_first_child(root);
    struct v3_inspection_value tmp_value;

    while (tmp_node) {
	tmp_value = v3_inspection_value(tmp_node);

	if (tmp_value.size == 0) {
	    struct dentry * new_dir = debugfs_create_dir(tmp_value.name, dir);
	    dfs_register_tree(new_dir, tmp_node);
	} else if (tmp_value.size == 1) {
	    debugfs_create_u8(tmp_value.name, 0644, dir, (u8 *)tmp_value.value);
	} else if (tmp_value.size == 2) {
	    debugfs_create_u16(tmp_value.name, 0644, dir, (u16 *)tmp_value.value);
	} else if (tmp_value.size == 4) {
	    debugfs_create_u32(tmp_value.name, 0644, dir, (u32 *)tmp_value.value);
	} else if (tmp_value.size == 8) {
	    debugfs_create_u64(tmp_value.name, 0644, dir, (u64 *)tmp_value.value);
	} else {

	    // buffer
	}

	tmp_node = v3_inspection_node_next(tmp_node);

    }

    return 0;
}


int dfs_register_vm(struct v3_guest * guest) {
    v3_inspect_node_t * root = v3_get_inspection_root(guest->v3_ctx);

    if (root == NULL) {
	printk("No inspection root found\n");
    	return -1;
    }

    dfs_register_tree(v3_dir, root);
    return 0;
}
