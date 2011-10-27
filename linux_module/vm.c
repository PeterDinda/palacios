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

#include <palacios/vmm.h>

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
    struct vm_ctrl * ctrl = kmalloc(sizeof(struct vm_ctrl), GFP_KERNEL);

    if (ctrl == NULL) {
	printk("Error: Could not allocate vm ctrl %d\n", cmd);
	return -1;
    }

    ctrl->cmd = cmd;
    ctrl->handler = handler;
    ctrl->priv_data = priv_data;

    if (__insert_ctrl(guest, ctrl) != NULL) {
	printk("Could not insert guest ctrl %d\n", cmd);
	kfree(ctrl);
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







extern struct class * v3_class;


static long v3_vm_ioctl(struct file * filp,
			unsigned int ioctl, unsigned long arg) {

    struct v3_guest * guest = filp->private_data;

    printk("V3 IOCTL %d\n", ioctl);

    switch (ioctl) {
	case V3_VM_LAUNCH: {
	    printk("palacios: launching vm\n");

	    if (v3_start_vm(guest->v3_ctx, (0x1 << num_online_cpus()) - 1) < 0) { 
		printk("palacios: launch of vm failed\n");
		return -1;
	    }
    	    
	    break;
	}
	case V3_VM_STOP: {
	    printk("Stopping VM (%s) (%p)\n", guest->name, guest);

	    if (irqs_disabled()) {
		printk("WHAT!!?? IRQs are disabled??\n");
		break;
	    }

	    v3_stop_vm(guest->v3_ctx);
	    break;
	}
	case V3_VM_PAUSE: {
	    printk("Pausing VM (%s)\n", guest->name);
	    v3_pause_vm(guest->v3_ctx);
	    break;
	}
	case V3_VM_CONTINUE: {
	    printk("Continuing VM (%s)\n", guest->name);
	    v3_continue_vm(guest->v3_ctx);
	    break;
	}
#ifdef V3_CONFIG_CHECKPOINT
	case V3_VM_SAVE: {
	    struct v3_chkpt_info chkpt;
	    void __user * argp = (void __user *)arg;

	    memset(&chkpt, 0, sizeof(struct v3_chkpt_info));

	    if (copy_from_user(&chkpt, argp, sizeof(struct v3_chkpt_info))) {
		printk("Copy from user error getting checkpoint info\n");
		return -EFAULT;
	    }
	    
	    printk("Saving Guest to %s:%s\n", chkpt.store, chkpt.url);

	    if (v3_save_vm(guest->v3_ctx, chkpt.store, chkpt.url) == -1) {
		printk("Error checkpointing VM state\n");
		return -EFAULT;
	    }
	    
	    break;
	}
	case V3_VM_LOAD: {
	    struct v3_chkpt_info chkpt;
	    void __user * argp = (void __user *)arg;

	    memset(&chkpt, 0, sizeof(struct v3_chkpt_info));

	    if (copy_from_user(&chkpt, argp, sizeof(struct v3_chkpt_info))) {
		printk("Copy from user error getting checkpoint info\n");
		return -EFAULT;
	    }
	    
	    printk("Loading Guest to %s:%s\n", chkpt.store, chkpt.url);

	    if (v3_load_vm(guest->v3_ctx, chkpt.store, chkpt.url) == -1) {
		printk("Error Loading VM state\n");
		return -EFAULT;
	    }
	    
	    break;
	}
#endif
	case V3_VM_MOVE_CORE: {
	    struct v3_core_move_cmd cmd;
	    void __user * argp = (void __user *)arg;

	    memset(&cmd, 0, sizeof(struct v3_core_move_cmd));
	    
	    if (copy_from_user(&cmd, argp, sizeof(struct v3_core_move_cmd))) {
		printk("copy from user error getting migrate command...\n");
		return -EFAULT;
	    }
	
	    printk("moving guest %s vcore %d to CPU %d\n", guest->name, cmd.vcore_id, cmd.pcore_id);

	    v3_move_vm_core(guest->v3_ctx, cmd.vcore_id, cmd.pcore_id);

	    break;
	}
	default: {
	    struct vm_ctrl * ctrl = get_ctrl(guest, ioctl);

	    if (ctrl) {
		return ctrl->handler(guest, ioctl, arg, ctrl->priv_data);
	    }
	    
	    
	    printk("\tUnhandled ctrl cmd: %d\n", ioctl);
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

    init_vm_extensions(guest);

    guest->v3_ctx = v3_create_vm(guest->img, (void *)guest, guest->name);

    if (guest->v3_ctx == NULL) { 
	printk("palacios: failed to create vm\n");
	return -1;
    }


    printk("Creating VM device: Major %d, Minor %d\n", MAJOR(guest->vm_dev), MINOR(guest->vm_dev));

    cdev_init(&(guest->cdev), &v3_vm_fops);

    guest->cdev.owner = THIS_MODULE;
    guest->cdev.ops = &v3_vm_fops;


    printk("Adding VM device\n");
    err = cdev_add(&(guest->cdev), guest->vm_dev, 1);

    if (err) {
	printk("Fails to add cdev\n");
	v3_free_vm(guest->v3_ctx);
	return -1;
    }

    if (device_create(v3_class, NULL, guest->vm_dev, guest, "v3-vm%d", MINOR(guest->vm_dev)) == NULL){
	printk("Fails to create device\n");
	cdev_del(&(guest->cdev));
	v3_free_vm(guest->v3_ctx);
	return -1;
    }

    printk("palacios: vm created at /dev/v3-vm%d\n", MINOR(guest->vm_dev));

    return 0;
}





int free_palacios_vm(struct v3_guest * guest) {

    v3_free_vm(guest->v3_ctx);

    device_destroy(v3_class, guest->vm_dev);

    cdev_del(&(guest->cdev));

    vfree(guest->img);
    kfree(guest);

    return 0;
}
