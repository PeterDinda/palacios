/* 
   Palacios main control interface
   (c) Jack Lange, 2010
 */

#include <linux/kernel.h>
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
#include <linux/seq_file.h>

#include <palacios/vmm.h>

#include "palacios.h"
#include "mm.h"
#include "vm.h"
#include "numa.h"
#include "allow_devmem.h"
#include "memcheck.h"
#include "lockcheck.h"

#include "linux-exts.h"

MODULE_LICENSE("GPL");

// Module parameter
int cpu_list[NR_CPUS] = {};
int cpu_list_len = 0;
module_param_array(cpu_list, int, &cpu_list_len, 0644);
MODULE_PARM_DESC(cpu_list, "Comma-delimited list of CPUs that Palacios will run on");

static int allow_devmem = 0;
module_param(allow_devmem, int, 0);
MODULE_PARM_DESC(allow_devmem, "Allow general user-space /dev/mem access even if kernel is strict");

// Palacios options parameter
static char *options;
module_param(options, charp, 0);
MODULE_PARM_DESC(options, "Generic options to internal Palacios modules");


int mod_allocs = 0;
int mod_frees = 0;

static int v3_major_num = 0;

static struct v3_guest * guest_map[MAX_VMS] = {[0 ... MAX_VMS - 1] = 0};
static struct proc_dir_entry * palacios_proc_dir = NULL;

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
	    guest->img = palacios_valloc(guest->img_size);

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
            palacios_vfree(guest->img);
out_err1:
            guest_map[vm_minor] = NULL; 
out_err:
            palacios_free(guest);

            return -1;

	    break;
	}
	case V3_FREE_GUEST: {
	    unsigned long vm_idx = arg;
            struct v3_guest * guest;

            if (vm_idx > MAX_VMS) {
                ERROR("Invalid VM index: %ld\n", vm_idx);
                return -1;
            }

	    guest = guest_map[vm_idx];

	    if (!guest) {
		ERROR("No VM at index %ld\n",vm_idx);
		return -1;
	    }

	    INFO("Freeing VM (%s) (%p)\n", guest->name, guest);

	    if (free_palacios_vm(guest)<0) { 
		ERROR("Cannot free guest at index %ld\n",vm_idx);
		return -1;
	    }

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

	    if (add_palacios_memory(&mem) == -1) {
		ERROR("Error adding memory to Palacios\n");
		return -EFAULT;
	    }

	    break;
	}

	case V3_REMOVE_MEMORY: {
	    struct v3_mem_region mem;
	    
	    memset(&mem, 0, sizeof(struct v3_mem_region));
	    
	    if (copy_from_user(&mem, argp, sizeof(struct v3_mem_region))) {
		ERROR("copy from user error getting mem_region...\n");
		return -EFAULT;
	    }

	    DEBUG("Removing memory at address %p\n", (void*)(mem.base_addr));

	    if (remove_palacios_memory(&mem) == -1) {
		ERROR("Error removing memory from Palacios\n");
		return -EFAULT;
	    }

	    break;
	}
	    
	    

        case V3_RESET_MEMORY: {
	    DEBUG("Resetting memory\n");
            if (palacios_deinit_mm() == -1) {
                ERROR("Error deiniting the Palacios memory manager\n");
                return -EFAULT;
            }
	    if (palacios_init_mm()) { 
		ERROR("Error initing the Palacios memory manager\n");
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
    //    INFO("Returning procdir=%p\n",palacios_proc_dir);
    return palacios_proc_dir;
}


#define MAX_VCORES  256
#define MAX_REGIONS 1024



static int read_guests_details(struct seq_file *s, void *v)
{
    unsigned int i = 0;
    unsigned int j = 0;
    struct v3_vm_base_state *base=0;
    struct v3_vm_core_state *core=0;
    struct v3_vm_mem_state *mem=0;

    base = palacios_alloc(sizeof(struct v3_vm_base_state));
    
    if (!base) { 
      ERROR("No space for base state structure\n");
      goto out;
    }

    core = palacios_alloc(sizeof(struct v3_vm_core_state) + MAX_VCORES*sizeof(struct v3_vm_vcore_state));
    
    if (!core) { 
	ERROR("No space for core state structure\n");
	goto out;
    }
    
    mem = palacios_alloc(sizeof(struct v3_vm_mem_state) + MAX_REGIONS*sizeof(struct v3_vm_mem_region));
    
    if (!mem) { 
	ERROR("No space for memory state structure\n");
	goto out;
    }
    
    for(i = 0; i < MAX_VMS; i++) {
	if (guest_map[i] != NULL) {
	    seq_printf(s,
		       "---------------------------------------------------------------------------------------\n");
	    seq_printf(s, 
		       "Entry:        %d\n"
		       "Name:         %s\n"
		       "Device:       /dev/v3-vm%d\n", 
		       i,guest_map[i]->name, i);
	    
	    // Get extended data
	    core->num_vcores=MAX_VCORES; // max we can handle
	    mem->num_regions=MAX_REGIONS; // max we can handle
	    
	    if (v3_get_state_vm(guest_map[i]->v3_ctx, base, core, mem)) {
		ERROR("Cannot get VM info\n");
		seq_printf(s, "<unable to get data for this VM>\n");
	    } else {
		seq_printf(s, 
			   "State:        %s\n"
			   "Cores:        %lu\n"
			   "Regions:      %lu\n\n",
			   base->state==V3_VM_INVALID ? "INVALID" :
			   base->state==V3_VM_RUNNING ? "running" :
			   base->state==V3_VM_STOPPED ? "stopped" :
			   base->state==V3_VM_PAUSED ? "paused" :
			   base->state==V3_VM_ERROR ? "ERROR" :
			   base->state==V3_VM_SIMULATING ? "simulating" : "UNKNOWN",
			   core->num_vcores,
			   mem->num_regions);
		seq_printf(s, "Core States\n");
		
		for (j=0;j<core->num_vcores;j++) {
		    seq_printf(s,
			       "   vcore %u %s on pcore %lu %llu exits rip=0x%p %s %s %s\n",
			       j, 
			       core->vcore[j].state==V3_VCORE_INVALID ? "INVALID" :
			       core->vcore[j].state==V3_VCORE_RUNNING ? "running" :
			       core->vcore[j].state==V3_VCORE_STOPPED ? "stopped" : "UNKNOWN",
			       core->vcore[j].pcore,
			       core->vcore[j].num_exits,
			       core->vcore[j].last_rip,
			       core->vcore[j].cpu_mode==V3_VCORE_CPU_REAL ? "real" :
			       core->vcore[j].cpu_mode==V3_VCORE_CPU_PROTECTED ? "protected" :
			       core->vcore[j].cpu_mode==V3_VCORE_CPU_PROTECTED_PAE ? "protectedpae" :
			       core->vcore[j].cpu_mode==V3_VCORE_CPU_LONG ? "long" :
			       core->vcore[j].cpu_mode==V3_VCORE_CPU_LONG_32_COMPAT ? "long32" :
			       core->vcore[j].cpu_mode==V3_VCORE_CPU_LONG_16_COMPAT ? "long16" : "UNKNOWN",
			       core->vcore[j].mem_mode==V3_VCORE_MEM_MODE_PHYSICAL ? "physical" :
			       core->vcore[j].mem_mode==V3_VCORE_MEM_MODE_VIRTUAL ? "virtual" : "UNKNOWN",
			       core->vcore[j].mem_state==V3_VCORE_MEM_STATE_SHADOW ? "shadow" :
			       core->vcore[j].mem_state==V3_VCORE_MEM_STATE_NESTED ? "nested" : "UNKNOWN");
		}

		seq_printf(s, "\nMemory Regions\n");
		for (j=0;j<mem->num_regions;j++) { 
		    seq_printf(s,"   region %u has HPAs 0x%p-0x%p (node %d)\n",
			       j, mem->region[j].host_paddr, mem->region[j].host_paddr+mem->region[j].size,
			       numa_addr_to_node((uintptr_t)(mem->region[j].host_paddr)));
		}
	    }
	    seq_printf(s,
		       "---------------------------------------------------------------------------------------\n");
	}
    }
    
    
 out:
    if (mem) { palacios_free(mem); }
    if (core) { palacios_free(core); }
    if (base) { palacios_free(base); }
    
    return 0;
}

static int read_guests(struct seq_file *s, void *v)
{
    unsigned int i = 0;
    struct v3_vm_base_state *base=0;
    struct v3_vm_core_state *core=0;
    struct v3_vm_mem_state *mem=0;
    
    base = palacios_alloc(sizeof(struct v3_vm_base_state));
    
    if (!base) { 
      ERROR("No space for base state structure\n");
      goto out;
    }

    core = palacios_alloc(sizeof(struct v3_vm_core_state) + MAX_VCORES*sizeof(struct v3_vm_vcore_state));
    
    if (!core) { 
	ERROR("No space for core state structure\n");
	goto out;
    }
    
    mem = palacios_alloc(sizeof(struct v3_vm_mem_state) + MAX_REGIONS*sizeof(struct v3_vm_mem_region));
    
    if (!mem) { 
	ERROR("No space for memory state structure\n");
	goto out;
    }
    
    for(i = 0; i < MAX_VMS; i++) {
	if (guest_map[i] != NULL) {
	    seq_printf(s,"%s\t/dev/v3-vm%d", guest_map[i]->name, i);
	    // Get extended data
	    core->num_vcores=MAX_VCORES; // max we can handle
	    mem->num_regions=MAX_REGIONS; // max we can handle
	    
	    if (v3_get_state_vm(guest_map[i]->v3_ctx, base, core, mem)) {
		ERROR("Cannot get VM info\n");
		seq_printf(s, "\t<unable to get data for this VM>\n");
	    } else {
		seq_printf(s,"\t%s\t%lu vcores\t%lu regions\n",
			   base->state==V3_VM_INVALID ? "INVALID" :
			   base->state==V3_VM_RUNNING ? "running" :
			   base->state==V3_VM_STOPPED ? "stopped" :
			   base->state==V3_VM_PAUSED ? "paused" :
			   base->state==V3_VM_ERROR ? "ERROR" :
			   base->state==V3_VM_SIMULATING ? "simulating" : "UNKNOWN",
			   core->num_vcores,
			   mem->num_regions);
	    }
	}
    }
	
	
 out:
    if (mem) { palacios_free(mem); }
    if (core) { palacios_free(core); }
    if (base) { palacios_free(base); }
    
    return 0;
}


static int guests_short_proc_open(struct inode * inode, struct file * filp) 
{
    struct proc_dir_entry * proc_entry = PDE(inode);
    return single_open(filp, read_guests, proc_entry->data);
}

static int guests_full_proc_open(struct inode * inode, struct file * filp) 
{
    struct proc_dir_entry * proc_entry = PDE(inode);
    return single_open(filp, read_guests_details, proc_entry->data);
}




static struct file_operations guest_full_proc_ops = {
    .owner = THIS_MODULE,
    .open = guests_full_proc_open, 
    .read = seq_read,
    .llseek = seq_lseek, 
    .release = single_release,
};

static struct file_operations guest_short_proc_ops = {
    .owner = THIS_MODULE,
    .open = guests_short_proc_open, 
    .read = seq_read,
    .llseek = seq_lseek, 
    .release = single_release,
};

// Supply basic information that the user-space tools need
// to manipulate Palacios.   The current use case here is to 
// convey memory information
static int read_info(struct seq_file *s, void *v)
{
    uint64_t mem_block_size;
    int i,j;
    int max_node=-1;
    seq_printf(s,"kernel MAX_ORDER:\t%d\n",MAX_ORDER);
    seq_printf(s,"number of nodes:\t%d\n", numa_num_nodes());
    seq_printf(s,"number of cpus: \t%d\n", num_online_cpus());
    seq_printf(s,"\npalacios compiled mem_block_size:\t%d\n", V3_CONFIG_MEM_BLOCK_SIZE);
    if (!v3_lookup_option("mem_block_size")) { 
	mem_block_size = V3_CONFIG_MEM_BLOCK_SIZE;
    } else {
	if (strict_strtoull(v3_lookup_option("mem_block_size"), 0, &mem_block_size)) {
	    // huh?
	    mem_block_size=-1;
	}
    }
    seq_printf(s,"palacios run-time mem_block_size:\t%llu\n", mem_block_size);
    
    seq_printf(s,"\nCPU to node mappings\n");
    for (i=0;i<num_online_cpus();i++) { 
	seq_printf(s,"cpu %d -> node %d\n", i, numa_cpu_to_node(i));
	if (numa_cpu_to_node(i)>max_node) { 
	    max_node=numa_cpu_to_node(i);
	}
    }
    seq_printf(s,"\nNode to node distances\n");
    for (j=0;j<=max_node;j++) { 
	seq_printf(s,"   \t%2d", j);
    }
    seq_printf(s,"\n");
    for (i=0;i<=max_node;i++) { 
	seq_printf(s,"%2d ",i);
	for (j=0;j<=max_node;j++) { 
	    seq_printf(s,"\t%2d", numa_get_distance(i,j));
	}
	seq_printf(s,"\n");
    }
    seq_printf(s,"\nCPU to CPU distances\n");
    for (j=0;j<num_online_cpus();j++) { 
	seq_printf(s,"   \t%2d", j);
    }
    seq_printf(s,"\n");
    for (i=0;i<num_online_cpus();i++) { 
	seq_printf(s,"%2d ",i);
	for (j=0;j<num_online_cpus();j++) { 
	    seq_printf(s,"\t%2d", numa_get_distance(numa_cpu_to_node(i),numa_cpu_to_node(j)));
	}
	seq_printf(s,"\n");
    }
    return 0;
}

static int info_proc_open(struct inode * inode, struct file * filp) 
{
    struct proc_dir_entry * proc_entry = PDE(inode);
    return single_open(filp, read_info, proc_entry->data);
}



static struct file_operations info_proc_ops = {
    .owner = THIS_MODULE,
    .open = info_proc_open, 
    .read = seq_read,
    .llseek = seq_lseek, 
    .release = single_release,
};


static int __init v3_init(void) {

    dev_t dev = MKDEV(0, 0); // We dynamicallly assign the major number
    int ret = 0;

    LOCKCHECK_INIT();
    MEMCHECK_INIT();

    palacios_proc_dir = proc_mkdir("v3vee", NULL);
    if (!palacios_proc_dir) {
	ERROR("Could not create proc entry\n");
	ret = -1;
	goto failure1;
    }

    // this will populate the v3vee tree...
    if (palacios_init_mm()) { 
	goto failure2;
    }

    if (allow_devmem) {
      palacios_allow_devmem();
    }

    // numa is now a required interface and we need it
    // up before primary initiatilization
    palacios_init_numa();

    // Initialize Palacios
    palacios_vmm_init(options);

    // initialize extensions
    init_lnx_extensions();


    v3_class = class_create(THIS_MODULE, "vms");
    if (IS_ERR(v3_class)) {
	ERROR("Failed to register V3 VM device class\n");
	ret =  PTR_ERR(v3_class);
	goto failure3;
    }

    INFO("intializing V3 Control device\n");

    ret = alloc_chrdev_region(&dev, 0, MAX_VMS + 1, "v3vee");

    if (ret < 0) {
	ERROR("Error registering device region for V3 devices\n");
	goto failure4;
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
	goto failure5;
    }

    {
	struct proc_dir_entry *entry;

	entry = create_proc_entry("v3-guests", 0444, palacios_proc_dir);
        if (entry) {
	    entry->proc_fops = &guest_short_proc_ops;
	    INFO("/proc/v3vee/v3-guests successfully created\n");
	} else {
	    ERROR("Could not create proc entry\n");
	    goto failure6;
	}
	entry = create_proc_entry("v3-guests-details", 0444, palacios_proc_dir);
        if (entry) {
	    entry->proc_fops = &guest_full_proc_ops;
	    INFO("/proc/v3vee/v3-guests-details successfully created\n");
	} else {
	    ERROR("Could not create proc entry\n");
	    goto failure7;
	}

	entry = create_proc_entry("v3-info", 0444, palacios_proc_dir);
        if (entry) {
	    entry->proc_fops = &info_proc_ops;
	    INFO("/proc/v3vee/v3-info successfully created\n");
	} else {
	    ERROR("Could not create proc entry\n");
	    goto failure8;
	}


    }
	
    return 0;

 failure8:
    remove_proc_entry("v3-guests-details", palacios_proc_dir);
 failure7:
    remove_proc_entry("v3-guests", palacios_proc_dir);
 failure6:
    device_destroy(v3_class, dev);
 failure5:
    unregister_chrdev_region(MKDEV(v3_major_num, 0), MAX_VMS + 1);
 failure4:
    class_destroy(v3_class);
 failure3:
    if (allow_devmem) {
      palacios_restore_devmem();
    }
    palacios_deinit_mm();
 failure2:
    remove_proc_entry("v3vee", NULL);
 failure1:   
    MEMCHECK_DEINIT();
    LOCKCHECK_DEINIT();

    return ret;
}


static void __exit v3_exit(void) {
    extern u32 pg_allocs;
    extern u32 pg_frees;
    extern u32 mallocs;
    extern u32 frees;
    extern u32 vmallocs;
    extern u32 vfrees;
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

    palacios_deinit_numa();

    DEBUG("Palacios Mallocs = %d, Frees = %d\n", mallocs, frees);
    DEBUG("Palacios Vmallocs = %d, Vfrees = %d\n", vmallocs, vfrees);
    DEBUG("Palacios Page Allocs = %d, Page Frees = %d\n", pg_allocs, pg_frees);

    unregister_chrdev_region(MKDEV(v3_major_num, 0), MAX_VMS + 1);

    cdev_del(&ctrl_dev);

    device_destroy(v3_class, dev);
    class_destroy(v3_class);


    deinit_lnx_extensions();

    if (allow_devmem) {
      palacios_restore_devmem();
    }

    palacios_deinit_mm();

    remove_proc_entry("v3-info", palacios_proc_dir);
    remove_proc_entry("v3-guests-details", palacios_proc_dir);
    remove_proc_entry("v3-guests", palacios_proc_dir);
    remove_proc_entry("v3vee", NULL);

    DEBUG("Palacios Module Mallocs = %d, Frees = %d\n", mod_allocs, mod_frees);
    
    MEMCHECK_DEINIT();
    LOCKCHECK_DEINIT();
}



module_init(v3_init);
module_exit(v3_exit);



void * trace_malloc(size_t size, gfp_t flags) {
    void * addr = NULL;

    mod_allocs++;
    addr = palacios_alloc_extended(size, flags, -1);

    return addr;
}


void trace_free(const void * objp) {
    mod_frees++;
    palacios_free((void*)objp);
}
