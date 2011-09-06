/* 
 * DebugFS interface
 * (c) Patrick Bridges and Philip Soltero, 2011
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <interfaces/vmm_mcheck.h>

#include "palacios.h"
#include "vm.h"
#include "linux-exts.h"

#define SCRUBBER_MCE 0x1
#define V3_VM_INJECT_SCRUBBER_MCE (10224+20)

static int inject_mce(struct v3_guest * guest, unsigned int cmd, unsigned long arg,
                      void * priv_data)
{
    unsigned long type = (unsigned long)priv_data;
    switch ( type ) {
	case SCRUBBER_MCE:
	    return v3_mcheck_inject_scrubber_mce((struct v3_vm_info *)guest->v3_ctx, 0, arg);
	    break;
	default:
	    // TODO: How to print an error in the host OS?
	    //PrintError("Injection of unknown machine check type %lu requested.\n", type);
	    return -1;
	    break;
    }
}

static int guest_init(struct v3_guest * guest, void ** vm_data) {

    add_guest_ctrl(guest, V3_VM_INJECT_SCRUBBER_MCE, inject_mce, (void *)SCRUBBER_MCE);
    return 0;
}

static int guest_deinit(struct v3_guest * guest, void * vm_data) {
    
    return 0;
}


struct linux_ext mcheck_ext = {
    .name = "MACHINE CHECK",
    .init = NULL,
    .deinit = NULL,
    .guest_init = guest_init, 
    .guest_deinit = guest_deinit
};


register_extension(&mcheck_ext);
