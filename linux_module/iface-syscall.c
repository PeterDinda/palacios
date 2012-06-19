/*
 * Linux interface for control of the fast system call
 * exiting utility
 *
 * (c) Kyle C. Hale 2012
 *
 */

#include <linux/uaccess.h>
#include <linux/module.h>

#include <gears/syscall_hijack.h>

#include "palacios.h"
#include "vm.h"
#include "linux-exts.h"

#include "iface-syscall.h"


static int vm_syscall_ctrl (struct v3_guest * guest, unsigned int cmd, unsigned long arg, void * priv_data) {
    struct v3_syscall_cmd syscall_cmd;
    int ret;

    if (copy_from_user(&syscall_cmd, (void __user *)arg, sizeof(struct v3_syscall_cmd))) {
        ERROR("palacios: error copying syscall command from userspace\n");
        return -EFAULT;
    }
    
    switch (syscall_cmd.cmd) {

        case SYSCALL_OFF: {
            ret = v3_syscall_off(guest->v3_ctx, syscall_cmd.syscall_nr);
            if (ret < 0) {
                ERROR("palacios: error deactivating syscall exiting for syscall nr: %d\n", syscall_cmd.syscall_nr);
            }
            break;
        }
        case SYSCALL_ON: {
            ret = v3_syscall_on(guest->v3_ctx, syscall_cmd.syscall_nr);
            if (ret < 0) {
                ERROR("palacios: error activating syscall exiting for syscall nr: %d\n", syscall_cmd.syscall_nr);
            }
            break;
        }
        case SYSCALL_STAT: {
            ret = v3_syscall_stat(guest->v3_ctx, syscall_cmd.syscall_nr);  
            if (ret == SYSCALL_OFF)
                INFO("palacios: exiting for syscall #%d is OFF\n", syscall_cmd.syscall_nr);
            else if (ret == SYSCALL_ON) 
                INFO("palacios: exiting for syscall #%d is ON\n", syscall_cmd.syscall_nr);
            else 
                INFO("palacios: error stating syscall nr: %d\n", syscall_cmd.syscall_nr);
            break;
        }
        default:
            ERROR("palacios: error - invalid syscall command\n");
            return -1;
    }
      
    return ret;
}


static int init_syscall_ctrl (void) {
    return 0;
}


static int deinit_syscall_ctrl (void) { 
    return 0;
}


static int guest_init_syscall_ctrl (struct v3_guest * guest, void ** vm_data) {
    add_guest_ctrl(guest, V3_VM_SYSCALL_CTRL, vm_syscall_ctrl, NULL);
    return 0;
}


static int guest_deinit_syscall_ctrl (struct v3_guest * guest, void * vm_data) {
    return 0;
}


static struct linux_ext syscall_ctrl_ext = {
    .name = "SYSCALL_CTRL",
    .init = init_syscall_ctrl,
    .deinit = deinit_syscall_ctrl,
    .guest_init = guest_init_syscall_ctrl,
    .guest_deinit = guest_deinit_syscall_ctrl
};

register_extension(&syscall_ctrl_ext);
