/* 
 * VM specific Controls
 * (c) Jack Lange, 2010
 */

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/percpu.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <linux/module.h>

#include <palacios/vmm.h>
#include <palacios/vmm_host_events.h>

#include "palacios.h"
#include "vm.h"
#include "linux-exts.h"


struct vm_ctrl {
    unsigned int cmd;

    int (*handler)(struct v3_guest * guest, 
		   unsigned int cmd, unsigned long arg, 
		   void * priv_data);

    void * priv_data;

    struct rb_node tree_node;
};


static inline struct vm_ctrl * __insert_ctrl(struct v3_guest * vm, 
					     struct vm_ctrl * ctrl) {
    struct rb_node ** p = &(vm->vm_ctrls.rb_node);
    struct rb_node * parent = NULL;
    struct vm_ctrl * tmp_ctrl = NULL;

    while (*p) {
	parent = *p;
	tmp_ctrl = rb_entry(parent, struct vm_ctrl, tree_node);

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



int add_guest_ctrl(struct v3_guest * guest, unsigned int cmd, 
		   int (*handler)(struct v3_guest * guest, 
				  unsigned int cmd, unsigned long arg, 
				  void * priv_data),
		   void * priv_data) {
    struct vm_ctrl * ctrl = palacios_alloc(sizeof(struct vm_ctrl));

    if (ctrl == NULL) {
	WARNING("Error: Could not allocate vm ctrl %d\n", cmd);
	return -1;
    }

    ctrl->cmd = cmd;
    ctrl->handler = handler;
    ctrl->priv_data = priv_data;

    if (__insert_ctrl(guest, ctrl) != NULL) {
	WARNING("Could not insert guest ctrl %d\n", cmd);
	palacios_free(ctrl);
	return -1;
    }
    
    rb_insert_color(&(ctrl->tree_node), &(guest->vm_ctrls));

    return 0;
}




static struct vm_ctrl * get_ctrl(struct v3_guest * guest, unsigned int cmd) {
    struct rb_node * n = guest->vm_ctrls.rb_node;
    struct vm_ctrl * ctrl = NULL;

    while (n) {
	ctrl = rb_entry(n, struct vm_ctrl, tree_node);

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

int remove_guest_ctrl(struct v3_guest * guest, unsigned int cmd) {
    struct vm_ctrl * ctrl = get_ctrl(guest, cmd);

    if (ctrl == NULL) {
	INFO("Could not find control (%d) to remove\n", cmd);
	return -1;
    }

    rb_erase(&(ctrl->tree_node), &(guest->vm_ctrls));

    kfree(ctrl);

    return 0;
}

static void free_guest_ctrls(struct v3_guest * guest) {
    struct rb_node * node = rb_first(&(guest->vm_ctrls));
    struct vm_ctrl * ctrl = NULL;
    struct rb_node * tmp_node = NULL;

    while (node) {
	ctrl = rb_entry(node, struct vm_ctrl, tree_node);
	tmp_node = node;
	node = rb_next(node);
	
	WARNING("Cleaning up guest ctrl that was not removed explicitly (%d)\n", ctrl->cmd);

	kfree(ctrl);
    }
}






extern struct class * v3_class;


static long v3_vm_ioctl(struct file * filp,
			unsigned int ioctl, unsigned long arg) {

    struct v3_guest * guest = filp->private_data;

    INFO("V3 IOCTL %d\n", ioctl);

    switch (ioctl) {
	case V3_VM_LAUNCH: {
	    NOTICE("palacios: launching vm\n");

	    if (v3_start_vm(guest->v3_ctx, (0x1 << num_online_cpus()) - 1) < 0) { 
		WARNING("palacios: launch of vm failed\n");
		return -1;
	    }
    	    
	    break;
	}
	case V3_VM_STOP: {
	    NOTICE("Stopping VM (%s) (%p)\n", guest->name, guest);

	    if (irqs_disabled()) {
		ERROR("WHAT!!?? IRQs are disabled??\n");
		break;
	    }

	    v3_stop_vm(guest->v3_ctx);
	    break;
	}
	case V3_VM_PAUSE: {
	    NOTICE("Pausing VM (%s)\n", guest->name);
	    v3_pause_vm(guest->v3_ctx);
	    break;
	}
	case V3_VM_CONTINUE: {
	    NOTICE("Continuing VM (%s)\n", guest->name);
	    v3_continue_vm(guest->v3_ctx);
	    break;
	}
	case V3_VM_SIMULATE: {
	    NOTICE("Simulating VM (%s) for %lu msecs\n", guest->name, arg);
	    v3_simulate_vm(guest->v3_ctx, arg);
	    break;
	}


#ifdef V3_CONFIG_CHECKPOINT
	case V3_VM_SAVE: {
	    struct v3_chkpt_info chkpt;
	    void __user * argp = (void __user *)arg;

	    memset(&chkpt, 0, sizeof(struct v3_chkpt_info));

	    if (copy_from_user(&chkpt, argp, sizeof(struct v3_chkpt_info))) {
		WARNING("Copy from user error getting checkpoint info\n");
		return -EFAULT;
	    }
	    
	    NOTICE("Saving Guest to %s:%s\n", chkpt.store, chkpt.url);

	    if (v3_save_vm(guest->v3_ctx, chkpt.store, chkpt.url) == -1) {
		WARNING("Error checkpointing VM state\n");
		return -EFAULT;
	    }
	    
	    break;
	}
	case V3_VM_LOAD: {
	    struct v3_chkpt_info chkpt;
	    void __user * argp = (void __user *)arg;

	    memset(&chkpt, 0, sizeof(struct v3_chkpt_info));

	    if (copy_from_user(&chkpt, argp, sizeof(struct v3_chkpt_info))) {
		WARNING("Copy from user error getting checkpoint info\n");
		return -EFAULT;
	    }
	    
	    NOTICE("Loading Guest to %s:%s\n", chkpt.store, chkpt.url);

	    if (v3_load_vm(guest->v3_ctx, chkpt.store, chkpt.url) == -1) {
		WARNING("Error Loading VM state\n");
		return -EFAULT;
	    }
	    
	    break;
	}
#endif

#ifdef V3_CONFIG_LIVE_MIGRATION  
	case V3_VM_SEND: {
	    struct v3_chkpt_info chkpt_info;
	    void __user * argp = (void __user *)arg;
	    
	    memset(&chkpt_info,0, sizeof(struct v3_chkpt_info));
	    
	    if(copy_from_user(&chkpt_info, argp, sizeof(struct v3_chkpt_info))){
		WARNING("Copy from user error getting checkpoint info\n");
		return -EFAULT;
	    }
	    
	    
	    NOTICE("Sending (live-migrating) Guest to %s:%s\n",chkpt_info.store, chkpt_info.url); 
	    
	    if (v3_send_vm(guest->v3_ctx, chkpt_info.store, chkpt_info.url) == -1) {
		WARNING("Error sending VM\n");
		return -EFAULT;
	    }
	    
	    break;
	}

	case V3_VM_RECEIVE: {
	    struct v3_chkpt_info chkpt_info;
	    void __user * argp = (void __user *)arg;
	    
	    memset(&chkpt_info,0, sizeof(struct v3_chkpt_info));

	    if(copy_from_user(&chkpt_info, argp, sizeof(struct v3_chkpt_info))){
		WARNING("Copy from user error getting checkpoint info\n");
		return -EFAULT;
	    }
	    
	    
	    NOTICE("Receiving (live-migrating) Guest to %s:%s\n",chkpt_info.store, chkpt_info.url);
	    
	    if (v3_receive_vm(guest->v3_ctx, chkpt_info.store, chkpt_info.url) == -1) {
		WARNING("Error receiving VM\n");
		return -EFAULT;
	    }
	    
	    break;
	}
#endif

	case V3_VM_DEBUG: {
	    struct v3_debug_cmd cmd;
	    struct v3_debug_event evt;
	    void __user * argp = (void __user *)arg;	    

	    memset(&cmd, 0, sizeof(struct v3_debug_cmd));
	    
	    if (copy_from_user(&cmd, argp, sizeof(struct v3_debug_cmd))) {
		ERROR("Error: Could not copy debug command from user space\n");
		return -EFAULT;
	    }

	    evt.core_id = cmd.core;
	    evt.cmd = cmd.cmd;

	    INFO("Debugging VM\n");

	    if (v3_deliver_debug_event(guest->v3_ctx, &evt) == -1) {
		ERROR("Error could not deliver debug cmd\n");
		return -EFAULT;
	    }

	    break;
	}
	case V3_VM_MOVE_CORE: {
	    struct v3_core_move_cmd cmd;
	    void __user * argp = (void __user *)arg;

	    memset(&cmd, 0, sizeof(struct v3_core_move_cmd));
	    
	    if (copy_from_user(&cmd, argp, sizeof(struct v3_core_move_cmd))) {
		WARNING("copy from user error getting migrate command...\n");
		return -EFAULT;
	    }
	
	    INFO("moving guest %s vcore %d to CPU %d\n", guest->name, cmd.vcore_id, cmd.pcore_id);

	    v3_move_vm_core(guest->v3_ctx, cmd.vcore_id, cmd.pcore_id);

	    break;
	}
	default: {
	    struct vm_ctrl * ctrl = get_ctrl(guest, ioctl);

	    if (ctrl) {
		return ctrl->handler(guest, ioctl, arg, ctrl->priv_data);
	    }
	    
	    
	    WARNING("\tUnhandled ctrl cmd: %d\n", ioctl);
	    return -EINVAL;
	}
    }

    return 0;
}

static int v3_vm_open(struct inode * inode, struct file * filp) {
    struct v3_guest * guest = container_of(inode->i_cdev, struct v3_guest, cdev);
    filp->private_data = guest;
    return 0;
}


static ssize_t v3_vm_read(struct file * filp, char __user * buf, size_t size, loff_t * offset) {
    
    return 0;
}


static ssize_t v3_vm_write(struct file * filp, const char __user * buf, size_t size, loff_t * offset) {

    return 0;
}


static struct file_operations v3_vm_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = v3_vm_ioctl,
    .compat_ioctl = v3_vm_ioctl,
    .open = v3_vm_open,
    .read = v3_vm_read, 
    .write = v3_vm_write,
};


extern u32 pg_allocs;
extern u32 pg_frees;
extern u32 mallocs;
extern u32 frees;

int create_palacios_vm(struct v3_guest * guest)  {
    int err;

    if (init_vm_extensions(guest) < 0) {
        WARNING("palacios: failed to initialize extensions\n");
        return -1;
    }

    guest->v3_ctx = v3_create_vm(guest->img, (void *)guest, guest->name);

    if (guest->v3_ctx == NULL) { 
	WARNING("palacios: failed to create vm\n");
        goto out_err;
    }

    NOTICE("Creating VM device: Major %d, Minor %d\n", MAJOR(guest->vm_dev), MINOR(guest->vm_dev));

    cdev_init(&(guest->cdev), &v3_vm_fops);

    guest->cdev.owner = THIS_MODULE;
    guest->cdev.ops = &v3_vm_fops;


    INFO("Adding VM device\n");
    err = cdev_add(&(guest->cdev), guest->vm_dev, 1);

    if (err) {
	WARNING("Fails to add cdev\n");
        goto out_err1;
    }

    if (device_create(v3_class, NULL, guest->vm_dev, guest, "v3-vm%d", MINOR(guest->vm_dev)) == NULL){
	WARNING("Fails to create device\n");
        goto out_err2;
    }

    NOTICE("palacios: vm created at /dev/v3-vm%d\n", MINOR(guest->vm_dev));

    return 0;

out_err2:
    cdev_del(&(guest->cdev));
out_err1:
    v3_free_vm(guest->v3_ctx);
out_err:
    deinit_vm_extensions(guest);
    return -1;
}





int free_palacios_vm(struct v3_guest * guest) {

    v3_free_vm(guest->v3_ctx);

    device_destroy(v3_class, guest->vm_dev);

    cdev_del(&(guest->cdev));

    free_guest_ctrls(guest);


    vfree(guest->img);
    palacios_free(guest);

    return 0;
}
