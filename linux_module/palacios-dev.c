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
#include "palacios-stream.h"
#include "palacios-file.h"
#include "palacios-serial.h"
#include "palacios-socket.h"
#include "palacios-vnet.h"
#include "palacios-packet.h"

#ifdef CONFIG_DEBUG_FS
#include "palacios-debugfs.h"
#endif

MODULE_LICENSE("GPL");

int mod_allocs = 0;
int mod_frees = 0;


static int v3_major_num = 0;

static u8 v3_minor_map[MAX_VMS / 8] = {[0 ... (MAX_VMS / 8) - 1] = 0}; 


struct class * v3_class = NULL;
static struct cdev ctrl_dev;

void * v3_base_addr = NULL;
unsigned int v3_pages = 0;

static int register_vm( void ) {
    int i, j = 0;
    int avail = 0;

    for (i = 0; i < sizeof(v3_minor_map); i++) {
	if (v3_minor_map[i] != 0xff) {
	    for (j = 0; j < 8; j++) {
		if (!v3_minor_map[i] & (0x1 << j)) {
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
	    guest->img = kmalloc(guest->img_size, GFP_KERNEL);

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

	    INIT_LIST_HEAD(&(guest->streams));
	    INIT_LIST_HEAD(&(guest->files));
	    INIT_LIST_HEAD(&(guest->sockets));
	    init_completion(&(guest->start_done));
	    init_completion(&(guest->thread_done));

	    kthread_run(start_palacios_vm, guest, guest->name);

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

	    // Mem test...
	    /*
	      {
	      void * vaddr = __va(alloc_palacios_pgs(131072, 4096));
	      memset(vaddr, 0xfe492fe2, mem.num_pages * 4096);
	      }
	    */

	    break;
	}

	case V3_START_NETWORK: {
            struct v3_network net;
            memset(&net, 0, sizeof(struct v3_network));
   
            if(copy_from_user(&net, argp, sizeof(struct v3_network))){
                printk("copy from user error getting network service requests ... \n");
                return -EFAULT;
            }
 
        #ifdef CONFIG_PALACIOS_SOCKET
            if(net.socket == 1){
                palacios_socket_init();
		printk("Started Palacios Socket\n");
            }
        #endif
        #ifdef CONFIG_PALACIOS_PACKET
            if(net.packet == 1){
                palacios_init_packet(NULL);
		printk("Started Palacios Direct Network Bridge\n");
            }
        #endif
        #ifdef CONFIG_PALACIOS_VNET
            if(net.vnet == 1){
                palacios_init_vnet();
		printk("Started Palacios VNET Service\n");
            }
        #endif
 
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


extern unsigned int v3_pages;
extern void * v3_base_addr;

static int __init v3_init(void) {
    dev_t dev = MKDEV(0, 0); // We dynamicallly assign the major number
    int ret = 0;


    palacios_init_mm();

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

    if ((v3_pages > 0) && (v3_base_addr != NULL)) {
	add_palacios_memory(__pa(v3_base_addr), v3_pages);
    }

    // Initialize Palacios
    
    palacios_vmm_init();

    palacios_init_stream();
    palacios_file_init();
    palacios_init_console();


#ifdef CONFIG_DEBUG_FS
    palacios_init_debugfs();
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



#ifdef CONFIG_DEBUG_FS
    palacios_deinit_debugfs();
#endif

    palacios_file_deinit();
    palacios_deinit_stream();

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
