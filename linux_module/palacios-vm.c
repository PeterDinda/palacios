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


#include <palacios/vmm.h>

#include "palacios.h"
#include "palacios-vm.h"

#ifdef V3_CONFIG_STREAM
#include "palacios-stream.h"
#endif

#ifdef V3_CONFIG_CONSOLE
#include "palacios-console.h"
#endif

#ifdef V3_CONFIG_EXT_INSPECTOR
#include "palacios-inspector.h"
#endif



extern struct class * v3_class;
#define STREAM_NAME_LEN 128

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

	case V3_VM_CONSOLE_CONNECT: {
#ifdef V3_CONFIG_CONSOLE
	    return connect_console(guest);
#else
	    printk("Console support not available\n");
	    return -EFAULT;
#endif
	    break;
	}
	case V3_VM_STREAM_CONNECT: {
#ifdef V3_CONFIG_STREAM
	    void __user * argp = (void __user *)arg;
	    char path_name[STREAM_NAME_LEN];

	    if (copy_from_user(path_name, argp, STREAM_NAME_LEN)) {
		printk("%s(%d): copy from user error...\n", __FILE__, __LINE__);
		return -EFAULT;
	    }

	    return open_stream(path_name);
#else
	    printk("Stream support Not available\n");
	    return -EFAULT;
#endif
	    break;
	}

	default: 
	    printk("\tUnhandled\n");
	    return -EINVAL;
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



extern int vm_running;
extern u32 pg_allocs;
extern u32 pg_frees;
extern u32 mallocs;
extern u32 frees;

int start_palacios_vm(void * arg)  {
    struct v3_guest * guest = (struct v3_guest *)arg;
    int err;

    lock_kernel();
    daemonize(guest->name);
//    allow_signal(SIGKILL);
    unlock_kernel();
    

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
	return -1;
    }

    if (device_create(v3_class, NULL, guest->vm_dev, guest, "v3-vm%d", MINOR(guest->vm_dev)) == NULL){
	printk("Fails to create device\n");
	return -1;
    }

    complete(&(guest->start_done));

    printk("palacios: launching vm\n");   



#if V3_CONFIG_EXT_INSPECTOR
    inspect_vm(guest);
#endif


    if (v3_start_vm(guest->v3_ctx, 0xffffffff) < 0) { 
	printk("palacios: launch of vm failed\n");
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
