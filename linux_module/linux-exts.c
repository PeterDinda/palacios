
#include "linux-exts.h"

/* 
 * This is a place holder to ensure that the _lnx_exts section gets created by gcc
 */


static struct {} null_ext  __attribute__((__used__))                    \
    __attribute__((unused, __section__ ("_lnx_exts"),			\
		   aligned(sizeof(void *))));

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
	
	printk("Registering Linux Extension (%s)\n", ext_impl->name);

	ext = kmalloc(sizeof(struct vm_ext), GFP_KERNEL);
	
	if (!ext) {
	    printk("Error allocating VM extension (%s)\n", ext_impl->name);
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
	    printk("WARNING: Extension %s, does not have a guest deinit function\n", ext->impl->name);
	}

	list_del(&(ext->node));
	kfree(ext);
    }

    return 0;
}

int init_lnx_extensions( void ) {
    extern struct linux_ext * __start__lnx_exts[];
    extern struct linux_ext * __stop__lnx_exts[];
    struct linux_ext * tmp_ext = __start__lnx_exts[0];
    int i = 0;

    while (tmp_ext != __stop__lnx_exts[0]) {

	printk("tmp_ext=%p\n", tmp_ext);

	if (tmp_ext->init != NULL) {
	    printk("Registering Linux Extension (%s)\n", tmp_ext->name);
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
	printk("Cleaning up Linux Extension (%s)\n", tmp_ext->name);
	if (tmp_ext->deinit != NULL) {
	    tmp_ext->deinit();
	} else {
	    printk("WARNING: Extension %s does not have a global deinit function\n", tmp_ext->name);
	}

	tmp_ext = __start__lnx_exts[++i];
    }
    
    return 0;
}
