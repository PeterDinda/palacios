/* 
 * DebugFS interface
 * (c) Jack Lange, 2011
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <interfaces/inspector.h>

#include "palacios.h"
#include "vm.h"
#include "linux-exts.h"

static struct dentry * v3_dir = NULL;





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


static int inspect_vm(struct v3_guest * guest, unsigned int cmd, unsigned long arg,
		      void * priv_data) {
    v3_inspect_node_t * root = v3_get_inspection_root(guest->v3_ctx);
    struct dentry * guest_dir = NULL;


    if (root == NULL) {
	printk("No inspection root found\n");
    	return -1;
    }

    guest_dir = debugfs_create_dir(guest->name, v3_dir);

    if (IS_ERR(guest_dir)) {
	printk("Error Creating inspector tree for VM \"%s\"\n", guest->name);
	return -1;
    }

    dfs_register_tree(guest_dir, root);
    return 0;
}



static int init_inspector( void ) {

    v3_dir = debugfs_create_dir("v3vee", NULL);

    if (IS_ERR(v3_dir)) {
	printk("Error creating v3vee debugfs directory\n");
	return -1;
    }

    return 0;
}


static int deinit_inspector( void ) {
    debugfs_remove(v3_dir);
    return 0;
}


static int guest_init(struct v3_guest * guest, void ** vm_data) {

    add_guest_ctrl(guest, V3_VM_INSPECT, inspect_vm, NULL);
    return 0;
}

static int guest_deinit(struct v3_guest * guest, void * vm_data) {
    
    return 0;
}


struct linux_ext inspector_ext = {
    .name = "INSPECTOR",
    .init = init_inspector, 
    .deinit = deinit_inspector,
    .guest_init = guest_init, 
    .guest_deinit = guest_deinit
};


register_extension(&inspector_ext);
