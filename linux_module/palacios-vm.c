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

#include <linux/smp_lock.h>
#include <linux/file.h>
#include <linux/spinlock.h>
#include <linux/rbtree.h>

#include <palacios/vmm.h>

#include "palacios.h"
#include "palacios-vm.h"
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



#ifdef V3_CONFIG_EXT_INSPECTOR
#include "palacios-inspector.h"
#endif

#ifdef V3_CONFIG_GRAPHICS_CONSOLE
#include "palacios-graphics-console.h"
#endif

#ifdef V3_CONFIG_HOST_DEVICE
#include "palacios-host-dev.h"
#define HOST_DEV_URL_LEN 256
#endif

extern struct class * v3_class;


static long v3_vm_ioctl(struct file * filp,
			unsigned int ioctl, unsigned long arg) {

    struct v3_guest * guest = filp->private_data;

    printk("V3 IOCTL %d\n", ioctl);

    switch (ioctl) {

	case V3_VM_STOP: {
	    printk("Stopping VM\n");
	    stop_palacios_vm(guest);
	    break;
	}



	case V3_VM_HOST_DEV_CONNECT: {
#ifdef V3_CONFIG_HOST_DEVICE
	    void __user * argp = (void __user *)arg;
	    char host_dev_url[HOST_DEV_URL_LEN];

	    if (copy_from_user(host_dev_url, argp, HOST_DEV_URL_LEN)) {
		printk("copy from user error getting url for host device connect...\n");
		return -EFAULT;
	    }

	    return connect_host_dev(guest,host_dev_url);
#else
	    printk("palacios: Host device support not available\n");
	    return -EFAULT;
#endif
	    break;
	}

	case V3_VM_FB_INPUT: 
#ifdef V3_CONFIG_GRAPHICS_CONSOLE
	    return palacios_graphics_console_user_input(&(guest->graphics_console),
							(struct v3_fb_input __user *) arg) ;
#else
	    return -EFAULT;
#endif
	    break;
	    
	case V3_VM_FB_QUERY: 
#ifdef V3_CONFIG_GRAPHICS_CONSOLE
	    return palacios_graphics_console_user_query(&(guest->graphics_console),
							(struct v3_fb_query_response __user *) arg);
#else
	    return -EFAULT;
#endif
	    break;


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

int start_palacios_vm(void * arg)  {
    struct v3_guest * guest = (struct v3_guest *)arg;
    int err;

    lock_kernel();
    daemonize(guest->name);
    // allow_signal(SIGKILL);
    unlock_kernel();
    
    init_vm_extensions(guest);

    guest->v3_ctx = v3_create_vm(guest->img, (void *)guest, guest->name);

    if (guest->v3_ctx == NULL) { 
	printk("palacios: failed to create vm\n");
	complete(&(guest->start_done));
	return -1;
    }

    // init linux extensions
    

#ifdef V3_CONFIG_EXT_INSPECTOR
    inspect_vm(guest);
#endif

    printk("Creating VM device: Major %d, Minor %d\n", MAJOR(guest->vm_dev), MINOR(guest->vm_dev));

    cdev_init(&(guest->cdev), &v3_vm_fops);

    guest->cdev.owner = THIS_MODULE;
    guest->cdev.ops = &v3_vm_fops;


    printk("Adding VM device\n");
    err = cdev_add(&(guest->cdev), guest->vm_dev, 1);

    if (err) {
	printk("Fails to add cdev\n");
	v3_free_vm(guest->v3_ctx);
	complete(&(guest->start_done));
	return -1;
    }

    if (device_create(v3_class, NULL, guest->vm_dev, guest, "v3-vm%d", MINOR(guest->vm_dev)) == NULL){
	printk("Fails to create device\n");
	cdev_del(&(guest->cdev));
	v3_free_vm(guest->v3_ctx);
	complete(&(guest->start_done));
	return -1;
    }

    complete(&(guest->start_done));

    printk("palacios: launching vm\n");

    if (v3_start_vm(guest->v3_ctx, 0xffffffff) < 0) { 
	printk("palacios: launch of vm failed\n");
	device_destroy(v3_class, guest->vm_dev);
	cdev_del(&(guest->cdev));
	v3_free_vm(guest->v3_ctx);
	return -1;
    }
    
    complete(&(guest->thread_done));

    printk("palacios: vm completed.  returning.\n");

    return 0;
}




int stop_palacios_vm(struct v3_guest * guest) {

    v3_stop_vm(guest->v3_ctx);

    wait_for_completion(&(guest->thread_done));

    v3_free_vm(guest->v3_ctx);
    
    device_destroy(v3_class, guest->vm_dev);

    cdev_del(&(guest->cdev));

    kfree(guest->img);
    kfree(guest);

    return 0;
}
