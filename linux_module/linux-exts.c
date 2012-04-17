#include "linux-exts.h"

/* 
 * This is a place holder to ensure that the _lnx_exts section gets created by gcc
 */

static struct {} null_ext  __attribute__((__used__))                    \
    __attribute__((unused, __section__ ("_lnx_exts"),			\
		   aligned(sizeof(void *))));



/*                 */
/* Global controls */
/*                 */

struct rb_root global_ctrls;

static inline struct global_ctrl * __insert_global_ctrl(struct global_ctrl * ctrl) {
    struct rb_node ** p = &(global_ctrls.rb_node);
    struct rb_node * parent = NULL;
    struct global_ctrl * tmp_ctrl = NULL;

    while (*p) {
        parent = *p;
        tmp_ctrl = rb_entry(parent, struct global_ctrl, tree_node);

        if (ctrl->cmd < tmp_ctrl->cmd) {
            p = &(*p)->rb_left;
        } else if (ctrl->cmd > tmp_ctrl->cmd) {
            p = &(*p)->rb_right;
        } else {
            return tmp_ctrl;
        }
    }

    rb_link_node(&(ctrl->tree_node), parent, p);

    return NULL;
}



int add_global_ctrl(unsigned int cmd, 
                   int (*handler)(unsigned int cmd, unsigned long arg)) {
    struct global_ctrl * ctrl = kmalloc(sizeof(struct global_ctrl), GFP_KERNEL);

    if (ctrl == NULL) {
        printk("Error: Could not allocate global ctrl %d\n", cmd);
        return -1;
    }

    ctrl->cmd = cmd;
    ctrl->handler = handler;

    if (__insert_global_ctrl(ctrl) != NULL) {
        printk("Could not insert guest ctrl %d\n", cmd);
        kfree(ctrl);
        return -1;
    }
    
    rb_insert_color(&(ctrl->tree_node), &(global_ctrls));

    return 0;
}


struct global_ctrl * get_global_ctrl(unsigned int cmd) {
    struct rb_node * n = global_ctrls.rb_node;
    struct global_ctrl * ctrl = NULL;

    while (n) {
        ctrl = rb_entry(n, struct global_ctrl, tree_node);

        if (cmd < ctrl->cmd) {
            n = n->rb_left;
        } else if (cmd > ctrl->cmd) {
            n = n->rb_right;
        } else {
            return ctrl;
        }
    }

    return NULL;
}





/*             */
/* VM Controls */
/*             */

struct vm_ext {
    struct linux_ext * impl;
    void * vm_data;
    struct list_head node;
};


void * get_vm_ext_data(struct v3_guest * guest, char * ext_name) {
    struct vm_ext * ext = NULL;

    list_for_each_entry(ext, &(guest->exts), node) {
	if (strncmp(ext->impl->name, ext_name, strlen(ext->impl->name)) == 0) {
	    return ext->vm_data;
	}
    }

    return NULL;
}


int init_vm_extensions(struct v3_guest * guest) {
    extern struct linux_ext * __start__lnx_exts[];
    extern struct linux_ext * __stop__lnx_exts[];
    struct linux_ext * ext_impl = __start__lnx_exts[0];
    int i = 0;

    while (ext_impl != __stop__lnx_exts[0]) {
	struct vm_ext * ext = NULL;

	if (ext_impl->guest_init == NULL) {
	    // We can have global extensions without per guest state
	    ext_impl = __start__lnx_exts[++i];
	    continue;
	}
	
	INFO("Registering Linux Extension (%s)\n", ext_impl->name);

	ext = palacios_alloc(sizeof(struct vm_ext));
	
	if (!ext) {
	    WARNING("Error allocating VM extension (%s)\n", ext_impl->name);
	    return -1;
	}

	ext->impl = ext_impl;

	ext_impl->guest_init(guest, &(ext->vm_data));

	list_add(&(ext->node), &(guest->exts));

	ext_impl = __start__lnx_exts[++i];
    }
    
    return 0;
}



int deinit_vm_extensions(struct v3_guest * guest) {
    struct vm_ext * ext = NULL;
    struct vm_ext * tmp = NULL;

    list_for_each_entry_safe(ext, tmp, &(guest->exts), node) {
	if (ext->impl->guest_deinit) {
	    ext->impl->guest_deinit(guest, ext->vm_data);
	} else {
	    WARNING("WARNING: Extension %s, does not have a guest deinit function\n", ext->impl->name);
	}

	list_del(&(ext->node));
	palacios_free(ext);
    }

    return 0;
}


int init_lnx_extensions( void ) {
    extern struct linux_ext * __start__lnx_exts[];
    extern struct linux_ext * __stop__lnx_exts[];
    struct linux_ext * tmp_ext = __start__lnx_exts[0];
    int i = 0;

    while (tmp_ext != __stop__lnx_exts[0]) {

	DEBUG("tmp_ext=%p\n", tmp_ext);

	if (tmp_ext->init != NULL) {
	    INFO("Registering Linux Extension (%s)\n", tmp_ext->name);
	    tmp_ext->init();
	}

	tmp_ext = __start__lnx_exts[++i];
    }
    
    return 0;
}


int deinit_lnx_extensions( void ) {
    extern struct linux_ext * __start__lnx_exts[];
    extern struct linux_ext * __stop__lnx_exts[];
    struct linux_ext * tmp_ext = __start__lnx_exts[0];
    int i = 0;

    while (tmp_ext != __stop__lnx_exts[0]) {
	INFO("Cleaning up Linux Extension (%s)\n", tmp_ext->name);

	if (tmp_ext->deinit != NULL) {
	    tmp_ext->deinit();
	} else {
	    WARNING("WARNING: Extension %s does not have a global deinit function\n", tmp_ext->name);
	}

	tmp_ext = __start__lnx_exts[++i];
    }
    
    return 0;
}

