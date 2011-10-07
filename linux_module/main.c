/* 
   Palacios main control interface
   (c) Jack Lange, 2010
 */


#include <linux/module.h>
#include <linux/errno.h>
#include <linux/percpu.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>

#include <linux/io.h>

#include <linux/file.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>

#include "palacios.h"
#include "mm.h"
#include "vm.h"

#include "linux-exts.h"



MODULE_LICENSE("GPL");

int mod_allocs = 0;
int mod_frees = 0;


static int v3_major_num = 0;

static struct v3_guest * guest_map[MAX_VMS] = {[0 ... MAX_VMS - 1] = 0};

struct class * v3_class = NULL;
static struct cdev ctrl_dev;

static int register_vm(struct v3_guest * guest) {
    int i = 0;

    for (i = 0; i < MAX_VMS; i++) {
	if (guest_map[i] == NULL) {
	    guest_map[i] = guest;
	    return i;
	}
    }

    return -1;
}



static long v3_dev_ioctl(struct file * filp,
			 unsigned int ioctl, unsigned long arg) {
    void __user * argp = (void __user *)arg;
    printk("V3 IOCTL %d\n", ioctl);


    switch (ioctl) {
	case V3_CREATE_GUEST:{
	    int vm_minor = 0;
	    struct v3_guest_img user_image;
	    struct v3_guest * guest = kmalloc(sizeof(struct v3_guest), GFP_KERNEL);

	    if (IS_ERR(guest)) {
		printk("Palacios: Error allocating Kernel guest_image\n");
		return -EFAULT;
	    }

	    memset(guest, 0, sizeof(struct v3_guest));

	    printk("Palacios: Creating V3 Guest...\n");

	    vm_minor = register_vm(guest);

	    if (vm_minor == -1) {
		printk("Palacios Error: Too many VMs are currently running\n");
		return -EFAULT;
	    }

	    guest->vm_dev = MKDEV(v3_major_num, vm_minor);

	    if (copy_from_user(&user_image, argp, sizeof(struct v3_guest_img))) {
		printk("Palacios Error: copy from user error getting guest image...\n");
		return -EFAULT;
	    }

	    guest->img_size = user_image.size;

	    printk("Palacios: Allocating kernel memory for guest image (%llu bytes)\n", user_image.size);
	    guest->img = vmalloc(guest->img_size);

	    if (IS_ERR(guest->img)) {
		printk("Palacios Error: Could not allocate space for guest image\n");
		return -EFAULT;
	    }

	    if (copy_from_user(guest->img, user_image.guest_data, guest->img_size)) {
		printk("Palacios: Error loading guest data\n");
		return -EFAULT;
	    }	   

	    strncpy(guest->name, user_image.name, 127);

	    INIT_LIST_HEAD(&(guest->exts));

	    if (create_palacios_vm(guest) == -1) {
		printk("Palacios: Error creating guest\n");
		return -EFAULT;
	    }

	    return vm_minor;
	    break;
	}
	case V3_FREE_GUEST: {
	    unsigned long vm_idx = arg;
	    struct v3_guest * guest = guest_map[vm_idx];

	    printk("Freeing VM (%s) (%p)\n", guest->name, guest);

	    free_palacios_vm(guest);
	    guest_map[vm_idx] = NULL;
	    break;
	}
	case V3_ADD_MEMORY: {
	    struct v3_mem_region mem;
	    
	    memset(&mem, 0, sizeof(struct v3_mem_region));
	    
	    if (copy_from_user(&mem, argp, sizeof(struct v3_mem_region))) {
		printk("copy from user error getting mem_region...\n");
		return -EFAULT;
	    }

	    printk("Adding %llu pages to Palacios memory\n", mem.num_pages);

	    if (add_palacios_memory(mem.base_addr, mem.num_pages) == -1) {
		printk("Error adding memory to Palacios\n");
		return -EFAULT;
	    }

	    break;
	}

	default: 
	    printk("\tUnhandled\n");
	    return -EINVAL;
    }

    return 0;
}



static struct file_operations v3_ctrl_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = v3_dev_ioctl,
    .compat_ioctl = v3_dev_ioctl,
};



static int __init v3_init(void) {
    dev_t dev = MKDEV(0, 0); // We dynamicallly assign the major number
    int ret = 0;


    palacios_init_mm();


    // Initialize Palacios
    
    palacios_vmm_init();


    // initialize extensions
    init_lnx_extensions();


    v3_class = class_create(THIS_MODULE, "vms");
    if (IS_ERR(v3_class)) {
	printk("Failed to register V3 VM device class\n");
	return PTR_ERR(v3_class);
    }

    printk("intializing V3 Control device\n");

    ret = alloc_chrdev_region(&dev, 0, MAX_VMS + 1, "v3vee");

    if (ret < 0) {
	printk("Error registering device region for V3 devices\n");
	goto failure2;
    }

    v3_major_num = MAJOR(dev);

    dev = MKDEV(v3_major_num, MAX_VMS + 1);

    
    printk("Creating V3 Control device: Major %d, Minor %d\n", v3_major_num, MINOR(dev));
    cdev_init(&ctrl_dev, &v3_ctrl_fops);
    ctrl_dev.owner = THIS_MODULE;
    ctrl_dev.ops = &v3_ctrl_fops;
    cdev_add(&ctrl_dev, dev, 1);
    
    device_create(v3_class, NULL, dev, NULL, "v3vee");

    if (ret != 0) {
	printk("Error adding v3 control device\n");
	goto failure1;
    }



    return 0;

 failure1:
    unregister_chrdev_region(MKDEV(v3_major_num, 0), MAX_VMS + 1);
 failure2:
    class_destroy(v3_class);

    return ret;
}


static void __exit v3_exit(void) {
    extern u32 pg_allocs;
    extern u32 pg_frees;
    extern u32 mallocs;
    extern u32 frees;


    // should probably try to stop any guests



    dev_t dev = MKDEV(v3_major_num, MAX_VMS + 1);

    printk("Removing V3 Control device\n");


    palacios_vmm_exit();

    printk("Palacios Mallocs = %d, Frees = %d\n", mallocs, frees);
    printk("Palacios Page Allocs = %d, Page Frees = %d\n", pg_allocs, pg_frees);

    unregister_chrdev_region(MKDEV(v3_major_num, 0), MAX_VMS + 1);

    cdev_del(&ctrl_dev);

    device_destroy(v3_class, dev);
    class_destroy(v3_class);


    deinit_lnx_extensions();

    palacios_deinit_mm();

    printk("Palacios Module Mallocs = %d, Frees = %d\n", mod_allocs, mod_frees);
}



module_init(v3_init);
module_exit(v3_exit);



void * trace_malloc(size_t size, gfp_t flags) {
    void * addr = NULL;

    mod_allocs++;
    addr = kmalloc(size, flags);

    return addr;
}


void trace_free(const void * objp) {
    mod_frees++;
    kfree(objp);
}
