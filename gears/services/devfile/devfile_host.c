/* 
   Device File Virtualization Host Module 

   (c) Akhil Guliani and William Gross, 2015
     
   Adapted from MPI module (c) 2012 Peter Dinda

 */
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/version.h>
#include <linux/file.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sched.h>

#include <linux/syscalls.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/poll.h>

#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/device.h>
#include <linux/cdev.h>

#include <asm/uaccess.h>

#include <asm/page.h>

#include <palacios/vm_guest_mem.h>
#include <interfaces/vmm_host_hypercall.h>


#include "devfile_hc.h"


#define DEEP_DEBUG    1
#define SHALLOW_DEBUG 1

#if DEEP_DEBUG
#define DEEP_DEBUG_PRINT(fmt, args...) printk(("devfile: " fmt), ##args)
#else
#define DEEP_DEBUG_PRINT(fmt, args...) 
#endif

#if SHALLOW_DEBUG
#define SHALLOW_DEBUG_PRINT(fmt, args...) printk(("devfile: " fmt), ##args)
#else
#define SHALLOW_DEBUG_PRINT(fmt, args...) 
#endif


#define ERROR(fmt, args...) printk(("devfile: " fmt), ##args)
#define INFO(fmt, args...) printk(("devfile: " fmt), ##args)

#define PRINT_CONSOLE(fmt,args...) printf(("devfile: " fmt),##args)




// Added to make unique id's for IOCTL
#define MY_MACIG 'G'
#define INIT_IOCTL _IOR(MY_MACIG, 0, int)
#define SHADOW_SYSCALL_DONE _IOW(MY_MACIG, 2, int)

#define DEVFILE_NAME "v3-devfile"

static int devfile_major_num = 0;
static struct class *devfile_class = 0;
static struct cdev devfile_dev;



struct devfile_state {
    void     *shared_mem_va;
    uint64_t  shared_mem_uva;
    uint64_t  shared_mem_pa;
    uint64_t returned_fd;

    wait_queue_head_t user_wait_queue;
    wait_queue_head_t host_wait_queue;

    enum { WAIT_FOR_INIT, WAIT_ON_GUEST, WAIT_ON_SHADOW} state; 
} ;

// Currently this proof of concept supports a single userland/VM binding
// and is serially reusable
static struct devfile_state *state=0;

static inline struct devfile_state *find_matching_state(palacios_core_t core) { return state; } 


/* Hypercall helpers */ 

static void get_args_64(palacios_core_t core,
			struct guest_accessors *acc,
			uint64_t *a1,
			uint64_t *a2,
			uint64_t *a3,
			uint64_t *a4,
			uint64_t *a5,
			uint64_t *a6,
			uint64_t *a7,
			uint64_t *a8)
{
    *a1 = acc->get_rcx(core);
    *a2 = acc->get_rdx(core);
    *a3 = acc->get_rsi(core);
    *a4 = acc->get_rdi(core);
    *a5 = acc->get_r8(core);
    *a6 = acc->get_r9(core);
    *a7 = acc->get_r10(core);
    *a8 = acc->get_r11(core);
}

static void get_args_32(palacios_core_t core,
			struct guest_accessors *acc,
			uint64_t *a1,
			uint64_t *a2,
			uint64_t *a3,
			uint64_t *a4,
			uint64_t *a5,
			uint64_t *a6,
			uint64_t *a7,
			uint64_t *a8)
{
    uint64_t rsp;
    uint32_t temp;
    
    
    rsp = acc->get_rsp(core);
    
    acc->read_gva(core,rsp,4,&temp); *a1=temp;
    acc->read_gva(core,rsp+4,4,&temp); *a2=temp;
    acc->read_gva(core,rsp+8,4,&temp); *a3=temp;
    acc->read_gva(core,rsp+12,4,&temp); *a4=temp;
    acc->read_gva(core,rsp+16,4,&temp); *a5=temp;
    acc->read_gva(core,rsp+20,4,&temp); *a6=temp;
    acc->read_gva(core,rsp+24,4,&temp); *a7=temp;
    acc->read_gva(core,rsp+28,4,&temp); *a8=temp;
}

static void get_args(palacios_core_t core,
		     struct guest_accessors *acc,
		     uint64_t *a1,
		     uint64_t *a2,
		     uint64_t *a3,
		     uint64_t *a4,
		     uint64_t *a5,
		     uint64_t *a6,
		     uint64_t *a7,
		     uint64_t *a8)
{
    uint64_t rbx;
    uint32_t ebx;
    
    rbx=acc->get_rbx(core);
    ebx=rbx&0xffffffff;
    
    switch (ebx) {
	case 0x64646464:
	    DEEP_DEBUG_PRINT("64 bit hcall\n");
	    return get_args_64(core,acc,a1,a2,a3,a4,a5,a6,a7,a8);
	    break;
	case 0x32323232:
	    DEEP_DEBUG_PRINT("32 bit hcall\n");
	    return get_args_32(core,acc,a1,a2,a3,a4,a5,a6,a7,a8);
	    break;
	default:
	    ERROR("UNKNOWN hcall calling convention\n");
	    break;
    }
}

static void put_return(palacios_core_t core, 
		       struct guest_accessors *acc,
		       uint64_t rc,
		       uint64_t errno)
{
    acc->set_rax(core,rc);
    acc->set_rbx(core,errno);
}
	
/*
  Convert all hypercall pointer arguments from GVAs to GPAs
  The host userland is responsible for converting from 
  GVAs to HVAs. 
  
  The assumption here is that any pointer argument
  points to a structure that does NOT span a page
  boundary.  The guest userland is responsible for
  assuring that this is the case.
*/	     
static int deref_args(palacios_core_t core, 
		      struct guest_accessors *acc,
		      uint64_t* a1, uint64_t* a2, uint64_t* a3, uint64_t* a4, uint64_t* a5,
		      uint64_t* a6, uint64_t bvec)
{
    if (bvec & 1){
        uint64_t a1tmp = *a1;
        acc->gva_to_gpa(core,a1tmp,a1);
    }
    if (bvec & 2){
        uint64_t a2tmp = *a2;
        acc->gva_to_gpa(core,a2tmp,a2);
    }
    if (bvec & 4){
        uint64_t a3tmp = *a3;
        acc->gva_to_gpa(core,a3tmp,a3);
    }
    if (bvec & 8){
        uint64_t a4tmp = *a4;
        acc->gva_to_gpa(core,a4tmp,a4);
    }
    if (bvec & 16){
        uint64_t a5tmp = *a5;
        acc->gva_to_gpa(core,a5tmp,a5);
    }
    if (bvec & 32){
        uint64_t a6tmp = *a6;
        acc->gva_to_gpa(core,a6tmp,a6);
    }
    return 0; 
}



static uint64_t devfile_syscall_return(struct devfile_state *s, uint64_t *errno)
{
    uint64_t rc;
    uint64_t *shared_page = (uint64_t*)(s->shared_mem_va);

    s->state=WAIT_ON_SHADOW;

    // kick the the user if needed
    //!! IDEA: We can add Usermode Helper to start shadow process instead
    // and wait for it to send us an ioctl to wake up the module. 
    wake_up_interruptible(&(s->user_wait_queue));
    // goto sleep until we see a message received
    // part of a separate ioctl
    SHALLOW_DEBUG_PRINT("waiting For Shadow Process\n");
    while (wait_event_interruptible(s->host_wait_queue, (s->state==WAIT_ON_GUEST)) != 0) {}
    SHALLOW_DEBUG_PRINT("waiting done\n");
    // Get the returned value and errno
    rc     = *(shared_page +8);
    *errno = *(shared_page +9);

    SHALLOW_DEBUG_PRINT("waiting done %016llu (errno %016llu)\n",rc,*errno);
    return rc;
}


static int devfile_syscall_hcall(struct devfile_state *s, 
				 palacios_core_t core,
				 uint64_t sys_code,
				 uint64_t a1, uint64_t a2,uint64_t a3, 
				 uint64_t a4, uint64_t a5, uint64_t a6, 
				 uint64_t bit_vec, 
				 uint64_t *errno)
{
    //Using shared memory page
    uint64_t ret;
    uint64_t *shared_page = (uint64_t*)(s->shared_mem_va);

    *(shared_page +0) = sys_code;
    *(shared_page +1) = a1;
    *(shared_page +2) = a2;
    *(shared_page +3) = a3;
    *(shared_page +4) = a4;
    *(shared_page +5) = a5;
    *(shared_page +6) = a6;
    *(shared_page +7) = bit_vec;

    SHALLOW_DEBUG_PRINT("Host Module to wait on shadow\n");

    //Now wait for rc and errno to be written to the shared page
    ret = devfile_syscall_return(s, errno);

    SHALLOW_DEBUG_PRINT("SYSCALL HCALL %016llu (errno %016llu)\n",ret,*errno);

    return ret;
}



// The main Interface for Hypercalls
int devfile_hypercall(palacios_core_t *core,
		      unsigned int hid,
		      struct guest_accessors *acc,
		      void *p)
{
    uint64_t a1,a2,a3,a4,a5,a6,bit_vec,sys_code;
    uint64_t rc;
    uint64_t errno;
    
    struct devfile_state *s = find_matching_state(core);

    if (s->state == WAIT_FOR_INIT){
        SHALLOW_DEBUG_PRINT("Shared Memory Not Yet Initialized, returning syscall hypercall\n");
        return -1;
    }
    
    sys_code = 0;
    bit_vec = 0;
    
    DEEP_DEBUG_PRINT("devfile_hypercall(%p,0x%x,%p,%p)\n",
		     core,hid,acc,p);
    
    get_args(core,acc,&sys_code,&a1,&a2,&a3,&a4,&a5,&a6,&bit_vec);

    DEEP_DEBUG_PRINT("original arguments: %016llu, %016llu, %016llu, %016llu, %016llu, %016llu, %016llu, %016llu\n",
		     sys_code,a1,a2,a3,a4,a5,a6,bit_vec);
    
    // Convert any pointer arguments from GVAs to GPAs
    deref_args(core,acc,&a1,&a2,&a3,&a4,&a5,&a6,bit_vec);

    DEEP_DEBUG_PRINT("derefed arguments: %016llu, %016llu, %016llu, %016llu, %016llu, %016llu, %016llu, %016llu\n",
		     sys_code,a1,a2,a3,a4,a5,a6,bit_vec);

    rc = devfile_syscall_hcall(s,core,sys_code,a1,a2,a3,a4,a5,a6,bit_vec,&errno);

    SHALLOW_DEBUG_PRINT("Syscall rc: %016llu errno=%016llu\n",rc,errno);

    put_return(core,acc,rc,errno);

    return 0;

} 


static int devfile_open(struct inode * inode, struct file * filp) 
{
    struct devfile_state *s = state;
    
    if (s) { 
	ERROR("attempting to open devfile that is already open\n");
	return -EINVAL;
    }

    s=(struct devfile_state*)kmalloc(sizeof(struct devfile_state),GFP_KERNEL);

    if (!s) { 
	ERROR("Failed to allocate space for open\n");
	return -EINVAL;
    }

    // This hideousness is here because in this POC we
    // are simply allowing a single userland to be tied to 
    // a single VM.   At the same time, we are making 
    // the rest of the code more flexible for the future
    state = s;

    memset(s,0,sizeof(*s));

    init_waitqueue_head(&s->user_wait_queue);
    init_waitqueue_head(&s->host_wait_queue);

    s->state = WAIT_FOR_INIT;
	
    filp->private_data = (void*) s;

    return 0;
}

static int devfile_close(struct inode * inode, struct file * filp) 
{
    struct devfile_state *s = filp->private_data;
    
    if (s) { 
	if (s->state==WAIT_ON_SHADOW) { 
	    ERROR("Odd, userland is closing devfile while we are waiting for it\n");
	}
	kfree(s);
	state=0;
    }
    
    return 0;

}


static long devfile_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) 
{
    struct devfile_state *s = filp->private_data;

    switch(cmd) {
        case INIT_IOCTL:    
            s->shared_mem_pa = (uint64_t)arg;
            s->shared_mem_va = __va(s->shared_mem_pa);
            SHALLOW_DEBUG_PRINT("Shared Memory Physical Address: %016llu\n",s->shared_mem_pa);
            SHALLOW_DEBUG_PRINT("Shared Memory Kernel VA: %p\n",s->shared_mem_va);
            //Change State to wait on guest
            s->state = WAIT_ON_GUEST;
            break;
	    
        case SHADOW_SYSCALL_DONE:
            s->state = WAIT_ON_GUEST;
            wake_up_interruptible(&(s->host_wait_queue));
            break;

        default:
            return -EINVAL;
    }

    return 0;

}


static unsigned int devfile_poll(struct file * filp, 
				 struct poll_table_struct * poll_tb) 
{
    struct devfile_state *s = filp->private_data;

    SHALLOW_DEBUG_PRINT("poll\n");

    // register ourselves on the user wait queue
    poll_wait(filp, &(s->user_wait_queue), poll_tb);

    if (s->state==WAIT_ON_SHADOW) { 
	// Yes, we have a request if you want it!
	DEEP_DEBUG_PRINT("poll done immediate\n");
	return  POLLIN | POLLRDNORM;
    } 
    // No request yet, so we need to wait for one to show up.
    DEEP_DEBUG_PRINT("poll delayed\n");
    // We will get called again when that queue is woken up

    return 0;
}

static struct file_operations devfile_fops = {
    .open     = devfile_open,
    .release  = devfile_close,
    .poll     = devfile_poll,
    .unlocked_ioctl = devfile_ioctl,
    .compat_ioctl = devfile_ioctl
};

EXPORT_SYMBOL(devfile_hypercall);

int init_module(void) 
{
    dev_t dev;

    SHALLOW_DEBUG_PRINT("INIT\n");
  
    devfile_class = class_create(THIS_MODULE,"devfile");
    if (!devfile_class || IS_ERR(devfile_class)) { 
	ERROR("Cannot register devfile device class\n");
	return PTR_ERR(devfile_class);
    }

    dev = MKDEV(0,0);

    if (alloc_chrdev_region(&dev,0,1,"devfile")<0) {
	ERROR("Failed to alloc chrdev region\n");
	return -1;
    }

    devfile_major_num = MAJOR(dev);

    dev = MKDEV(devfile_major_num,1);

    cdev_init(&devfile_dev, &devfile_fops);
    devfile_dev.owner = THIS_MODULE;
    devfile_dev.ops = &devfile_fops;
    cdev_add(&devfile_dev, dev, 1);
    
    device_create(devfile_class, NULL, dev, NULL, "v3-devfile");


    INFO("inited\n");
    
    return 0;
}

void cleanup_module(void) 
{
    dev_t dev = MKDEV(devfile_major_num,1);

    unregister_chrdev_region(MKDEV(devfile_major_num,0),1);
    cdev_del(&devfile_dev);
    device_destroy(devfile_class,dev);
    class_destroy(devfile_class);

    if (state) {
	kfree(state);
    }

    INFO("deinited\n");
}


MODULE_LICENSE("GPL");
