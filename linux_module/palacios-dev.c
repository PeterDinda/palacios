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
#include "palacios-mm.h"
#include "palacios-vm.h"
#include "palacios-serial.h"
#include "palacios-vnet.h"

#include "linux-exts.h"


#ifdef V3_CONFIG_KEYED_STREAMS
#include "palacios-keyed-stream.h"
#endif


MODULE_LICENSE("GPL");

int mod_allocs = 0;
int mod_frees = 0;


static int v3_major_num = 0;

static u8 v3_minor_map[MAX_VMS / 8] = {[0 ... (MAX_VMS / 8) - 1] = 0}; 


struct class * v3_class = NULL;
static struct cdev ctrl_dev;

static int register_vm( void ) {
    int i, j = 0;
    int avail = 0;

    for (i = 0; i < sizeof(v3_minor_map); i++) {
	if (v3_minor_map[i] != 0xff) {
	    for (j = 0; j < 8; j++) {
		if (!(v3_minor_map[i] & (0x1 << j))) {
		    avail = 1;
		    v3_minor_map[i] |= (0x1 << j);
		    break;
		}
	    }
	
	    if (avail == 1) {
		break;
	    }
	}
    }

    if (avail == 0) {
	return -1;
    }
	
    return (i * 8) + j;
}



static long v3_dev_ioctl(struct file * filp,
			 unsigned int ioctl, unsigned long arg) {
    void __user * argp = (void __user *)arg;
    printk("V3 IOCTL %d\n", ioctl);


    switch (ioctl) {
	case V3_START_GUEST:{
	    int vm_minor = 0;
	    struct v3_guest_img user_image;
	    struct v3_guest * guest = kmalloc(sizeof(struct v3_guest), GFP_KERNEL);

	    if (IS_ERR(guest)) {
		printk("Error allocating Kernel guest_image\n");
		return -EFAULT;
	    }

	    memset(guest, 0, sizeof(struct v3_guest));

	    printk("Starting V3 Guest...\n");

	    vm_minor = register_vm();

	    if (vm_minor == -1) {
		printk("Too many VMs are currently running\n");
		return -EFAULT;
	    }

	    guest->vm_dev = MKDEV(v3_major_num, vm_minor);

	    if (copy_from_user(&user_image, argp, sizeof(struct v3_guest_img))) {
		printk("copy from user error getting guest image...\n");
		return -EFAULT;
	    }

	    guest->img_size = user_image.size;

	    printk("Allocating kernel memory for guest image (%llu bytes)\n", user_image.size);
	    guest->img = vmalloc(guest->img_size);

	    if (IS_ERR(guest->img)) {
		printk("Error: Could not allocate space for guest image\n");
		return -EFAULT;
	    }

	    if (copy_from_user(guest->img, user_image.guest_data, guest->img_size)) {
		printk("Error loading guest data\n");
		return -EFAULT;
	    }	   

	    strncpy(guest->name, user_image.name, 127);

	    printk("Launching VM\n");

	    INIT_LIST_HEAD(&(guest->exts));


#ifdef V3_CONFIG_HOST_DEVICE
	    INIT_LIST_HEAD(&(guest->hostdev.devs));
#endif
	    init_completion(&(guest->start_done));
	    init_completion(&(guest->thread_done));

	    { 
		struct task_struct * launch_thread = NULL;
		// At some point we're going to want to allow the user to specify a CPU mask
		// But for now, well just launch from the local core, and rely on the global cpu mask

		preempt_disable();
		launch_thread = kthread_create(start_palacios_vm, guest, guest->name);
		
		if (IS_ERR(launch_thread)) {
		    preempt_enable();
		    printk("Palacios error creating launch thread for vm (%s)\n", guest->name);
		    return -EFAULT;
		}

		kthread_bind(launch_thread, smp_processor_id());
		preempt_enable();

		wake_up_process(launch_thread);
	    }

	    wait_for_completion(&(guest->start_done));

	    return guest->vm_dev;
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




#ifdef V3_CONFIG_KEYED_STREAMS
    palacios_init_keyed_streams();
#endif

#ifdef V3_CONFIG_GRAPHICS_CONSOLE
    palacios_init_graphics_console();
#endif

#ifdef V3_CONFIG_VNET
    palacios_vnet_init();
#endif

#ifdef V3_CONFIG_HOST_DEVICE
    palacios_init_host_dev();
#endif

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



#ifdef V3_CONFIG_VNET
    palacios_vnet_deinit();
#endif

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
