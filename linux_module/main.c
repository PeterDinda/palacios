/* 
   Palacios main control interface
   (c) Jack Lange, 2010
 */


#include <linux/module.h>
#include <linux/moduleparam.h>
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

#include <linux/proc_fs.h>

#include <palacios/vmm.h>

#include "palacios.h"
#include "mm.h"
#include "vm.h"

#include "linux-exts.h"



MODULE_LICENSE("GPL");

// Module parameter
int cpu_list[NR_CPUS] = {};
int cpu_list_len = 0;
module_param_array(cpu_list, int, &cpu_list_len, 0644);
MODULE_PARM_DESC(cpu_list, "Comma-delimited list of CPUs that Palacios will run on");

int mod_allocs = 0;
int mod_frees = 0;


static int v3_major_num = 0;

static struct v3_guest * guest_map[MAX_VMS] = {[0 ... MAX_VMS - 1] = 0};
static struct proc_dir_entry *dir = 0;

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
    DEBUG("V3 IOCTL %d\n", ioctl);


    switch (ioctl) {
	case V3_CREATE_GUEST:{
	    int vm_minor = 0;
	    struct v3_guest_img user_image;
	    struct v3_guest * guest = palacios_alloc(sizeof(struct v3_guest));

	    if (IS_ERR(guest)) {
		ERROR("Palacios: Error allocating Kernel guest_image\n");
		return -EFAULT;
	    }

	    memset(guest, 0, sizeof(struct v3_guest));

	    INFO("Palacios: Creating V3 Guest...\n");

	    vm_minor = register_vm(guest);

	    if (vm_minor == -1) {
		ERROR("Palacios Error: Too many VMs are currently running\n");
		goto out_err;
	    }

	    guest->vm_dev = MKDEV(v3_major_num, vm_minor);

	    if (copy_from_user(&user_image, argp, sizeof(struct v3_guest_img))) {
		ERROR("Palacios Error: copy from user error getting guest image...\n");
		goto out_err1;
	    }

	    guest->img_size = user_image.size;

	    DEBUG("Palacios: Allocating kernel memory for guest image (%llu bytes)\n", user_image.size);
	    guest->img = vmalloc(guest->img_size);

	    if (IS_ERR(guest->img)) {
		ERROR("Palacios Error: Could not allocate space for guest image\n");
		goto out_err1;
	    }

	    if (copy_from_user(guest->img, user_image.guest_data, guest->img_size)) {
		ERROR("Palacios: Error loading guest data\n");
		goto out_err2;
	    }	   

	    strncpy(guest->name, user_image.name, 127);

	    INIT_LIST_HEAD(&(guest->exts));

	    if (create_palacios_vm(guest) == -1) {
		ERROR("Palacios: Error creating guest\n");
		goto out_err2;
	    }

	    return vm_minor;


out_err2:
            vfree(guest->img);
out_err1:
            guest_map[vm_minor] = NULL; 
out_err:
            palacios_free(guest);

            return -1;

	    break;
	}
	case V3_FREE_GUEST: {
	    unsigned long vm_idx = arg;
	    struct v3_guest * guest = guest_map[vm_idx];

	    if (!guest) {
		ERROR("No VM at index %ld\n",vm_idx);
		return -1;
	    }

	    INFO("Freeing VM (%s) (%p)\n", guest->name, guest);

	    free_palacios_vm(guest);
	    guest_map[vm_idx] = NULL;
	    break;
	}
	case V3_ADD_MEMORY: {
	    struct v3_mem_region mem;
	    
	    memset(&mem, 0, sizeof(struct v3_mem_region));
	    
	    if (copy_from_user(&mem, argp, sizeof(struct v3_mem_region))) {
		ERROR("copy from user error getting mem_region...\n");
		return -EFAULT;
	    }

	    DEBUG("Adding %llu pages to Palacios memory\n", mem.num_pages);

	    if (add_palacios_memory(mem.base_addr, mem.num_pages) == -1) {
		ERROR("Error adding memory to Palacios\n");
		return -EFAULT;
	    }

	    break;
	}


	default: {
	    struct global_ctrl * ctrl = get_global_ctrl(ioctl);
	    
	    if (ctrl) {
		return ctrl->handler(ioctl, arg);
	    }

	    WARNING("\tUnhandled global ctrl cmd: %d\n", ioctl);

	    return -EINVAL;
	}
    }

    return 0;
}



static struct file_operations v3_ctrl_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = v3_dev_ioctl,
    .compat_ioctl = v3_dev_ioctl,
};



struct proc_dir_entry *palacios_get_procdir(void) 
{
    return dir;
}

static int read_guests(char * buf, char ** start, off_t off, int count,
		       int * eof, void * data)
{
    int len = 0;
    unsigned int i = 0;
    
    for(i = 0; i < MAX_VMS; i++) {
	if (guest_map[i] != NULL) {
	    if (len<count) { 
		len += snprintf(buf+len, count-len,
				"%s\t/dev/v3-vm%d\n", 
				guest_map[i]->name, i);
	    }
	}
    }
    
    return len;
}

static int show_mem(char * buf, char ** start, off_t off, int count,
		    int * eof, void * data)
{
    int len = 0;
    
    len = snprintf(buf,count, "%p\n", (void *)get_palacios_base_addr());
    len += snprintf(buf+len,count-len, "%lld\n", get_palacios_num_pages());
    
    return len;
}


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
	ERROR("Failed to register V3 VM device class\n");
	return PTR_ERR(v3_class);
    }

    INFO("intializing V3 Control device\n");

    ret = alloc_chrdev_region(&dev, 0, MAX_VMS + 1, "v3vee");

    if (ret < 0) {
	ERROR("Error registering device region for V3 devices\n");
	goto failure2;
    }

    v3_major_num = MAJOR(dev);

    dev = MKDEV(v3_major_num, MAX_VMS + 1);

    
    DEBUG("Creating V3 Control device: Major %d, Minor %d\n", v3_major_num, MINOR(dev));
    cdev_init(&ctrl_dev, &v3_ctrl_fops);
    ctrl_dev.owner = THIS_MODULE;
    ctrl_dev.ops = &v3_ctrl_fops;
    cdev_add(&ctrl_dev, dev, 1);
    
    device_create(v3_class, NULL, dev, NULL, "v3vee");

    if (ret != 0) {
	ERROR("Error adding v3 control device\n");
	goto failure1;
    }

    dir = proc_mkdir("v3vee", NULL);
    if(dir) {
	struct proc_dir_entry *entry;

	entry = create_proc_read_entry("v3-guests", 0444, dir, 
				       read_guests, NULL);
        if (entry) {
	    INFO("/proc/v3vee/v3-guests successfully created\n");
	} else {
	    ERROR("Could not create proc entry\n");
	    goto failure1;
	}
	
	entry = create_proc_read_entry("v3-mem", 0444, dir,
				       show_mem, NULL);
	if (entry) {
	    INFO("/proc/v3vee/v3-mem successfully added\n");
	} else {
	    ERROR("Could not create proc entry\n");
	    goto failure1;
	}
    } else {
	ERROR("Could not create proc entry\n");
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
    int i = 0;
    struct v3_guest * guest;
    dev_t dev;


    /* Stop and free any running VMs */ 
    for (i = 0; i < MAX_VMS; i++) {
	if (guest_map[i] != NULL) {
                guest = (struct v3_guest *)guest_map[i];

                if (v3_stop_vm(guest->v3_ctx) < 0) 
                        ERROR("Couldn't stop VM %d\n", i);

                free_palacios_vm(guest);
                guest_map[i] = NULL;
	}
    }

    dev = MKDEV(v3_major_num, MAX_VMS + 1);

    INFO("Removing V3 Control device\n");


    palacios_vmm_exit();

    DEBUG("Palacios Mallocs = %d, Frees = %d\n", mallocs, frees);
    DEBUG("Palacios Page Allocs = %d, Page Frees = %d\n", pg_allocs, pg_frees);

    unregister_chrdev_region(MKDEV(v3_major_num, 0), MAX_VMS + 1);

    cdev_del(&ctrl_dev);

    device_destroy(v3_class, dev);
    class_destroy(v3_class);


    deinit_lnx_extensions();

    palacios_deinit_mm();

    remove_proc_entry("v3-guests", dir);
    remove_proc_entry("v3-mem", dir);
    remove_proc_entry("v3vee", NULL);

    DEBUG("Palacios Module Mallocs = %d, Frees = %d\n", mod_allocs, mod_frees);
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
