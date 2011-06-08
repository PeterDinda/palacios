
#include "linux-exts.h"

/* 
 * This is a place holder to ensure that the _lnx_exts section gets created by gcc
 */
static struct {} null_ext  __attribute__((__used__))                    \
    __attribute__((unused, __section__ ("_lnx_exts"),                \
                   aligned(sizeof(void *))));




int init_vm_extensions(struct v3_guest * guest) {
    extern struct linux_ext * __start__lnx_exts;
    extern struct linux_ext * __stop__lnx_exts;
    struct linux_ext * tmp_ext = __start__lnx_exts;
    int i = 0;

    while (tmp_ext != __stop__lnx_exts) {
	printk("Registering Linux Extension (%s)\n", tmp_ext->name);
	tmp_ext->init();

	tmp_ext = &(__start__lnx_exts[++i]);
    }
    
    return 0;

}

int init_lnx_extensions( void ) {
    extern struct linux_ext * __start__lnx_exts;
    extern struct linux_ext * __stop__lnx_exts;
    struct linux_ext * tmp_ext = __start__lnx_exts;
    int i = 0;

    while (tmp_ext != __stop__lnx_exts) {
	printk("Registering Linux Extension (%s)\n", tmp_ext->name);
	tmp_ext->init();

	tmp_ext = &(__start__lnx_exts[++i]);
    }
    
    return 0;
}


int deinit_lnx_extensions( void ) {
    extern struct linux_ext * __start__lnx_exts;
    extern struct linux_ext * __stop__lnx_exts;
    struct linux_ext * tmp_ext = __start__lnx_exts;
    int i = 0;

    while (tmp_ext != __stop__lnx_exts) {
	printk("Cleaning up Linux Extension (%s)\n", tmp_ext->name);
	tmp_ext->deinit();

	tmp_ext = &(__start__lnx_exts[++i]);
    }
    
    return 0;
}
