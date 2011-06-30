/* 
 * Host device interface + user-space device interface
 * (c) 2011 Peter Dinda
 */

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <interfaces/vmm_host_dev.h>

#include "palacios.h"
#include "iface-host-dev.h"
#include "linux-exts.h"
#include "vm.h"

/*
  There are two things in this file:


  1. An implementation of the Palacios host device interface that will
     accept any URL from Palacios.  Currently, the only URL type it will
     handle is user:<name>, but it should be clear how to extend with
     other types.

     Palacios opens a host device by issuing something like this:

         palacios_host_dev_open( impl="user:foo" busclass=pci, opaque )

     This will attempt to rendezvous with the user space device The
     rendevzous retry and timeout periods can be set below.

  2. An implementation of user: urls - the host interface is mapped
     into an RPC-like interface to/from user space via a file
     interface.   

     The user space gets a file descriptor like this:

     int vmfd = open("/dev/v3-vmX",...);

     int devfd = ioctl(vmfd,V3_HOST_DEV_CONNECT,"user:foo"); 

     This will attempt to rendezvous with the host side.

     If it returns successfully, you can now issue synchronous,
     blocking RPCs to the guest to read/write its memory and inject
     irqs.  This means that a user->host request is handled
     immediately, and independently of any host->user request. 

     struct palacios_host_dev_user_op op;

     // fill out op
     op.type = PALACIOS_HOST_DEV_USER_REQUEST_WRITE_GUEST; 
     ...

     ioctl(devfd, V3_HOST_DEV_USER_REQUEST_PUSH_IOCTL, &op);

     // return value is # bytes read or written; or 0 if irq injected
     // negative value is error

     The interface from the host to the user side is asynchronous
     blocking RPC.  Any host device will have at most one outstanding
     request from palacios.  The implementation here stores the
     request until the user side is ready to accept it.  The user side
     can check if there is a request via a poll/select or an ioctl.
     The ioctl also returns the needed size for the request structure.
     After the user side has a request, it is expected to process it,
     perhaps making user->host requests as described above, and then
     return a response.  Only one host->user request should be in
     progress at any time in the user space.

     What this looks like is:
     
     poll(...devfd for read...) or select(...devfd for read...)

     if (devfd is marked readable) { 
         uint64_t size;

         ioctl(devfd,V3_HOST_DEV_HOST_REQUEST_SIZE_IOCTL,&size);
         // returns 1 if there is a request, 0 if not, negative on err

         struct palacios_host_dev_host_request_response *req;

         req = allocate req to be at least size bytes long

         ioctl(devfd,V3_HOST_DEV_HOST_REQUEST_PULL_IOCTL,req)
         // returns 1 if there is a request, 0 if not, negative on err
	 
         // process request, perhaps using above user->host request
         // build response structure
         // resp.data_len == size of structure including relevant data at end

         ioctl(devfd,V3_HOST_DEV_USER_RESPONSE_PUSH_IOCTL,resp);
	 // returns 0 if there is no outstanding request (user error)
	 // 1 on success, negative on error
     }
    
     Note that the current model has deferred rendezvous and allows
     for user-side device disconnection and reconnection.  It is important
     to note that this implementation does NOT deal with state discrepency
     between the palacios-side and the user-side.   For example, a user-side
     device can disconnect, a palacios-side request can then fail, and 
     when the user-side device reconnects, it is unaware of this failure.  

*/


struct palacios_host_dev {
    spinlock_t      lock;
    struct list_head devs;
};


#define MAX_URL 256

#define RENDEZVOUS_WAIT_SECS  60
#define RENDEZVOUS_RETRY_SECS 1

#define DEEP_DEBUG    0
#define SHALLOW_DEBUG 0

#if DEEP_DEBUG
#define DEEP_DEBUG_PRINT(fmt, args...) printk((fmt), ##args)
#else
#define DEEP_DEBUG_PRINT(fmt, args...) 
#endif

#if SHALLOW_DEBUG
#define SHALLOW_DEBUG_PRINT(fmt, args...) printk((fmt), ##args)
#else
#define SHALLOW_DEBUG_PRINT(fmt, args...) 
#endif


#define ERROR(fmt, args...) printk((fmt), ##args)
#define INFO(fmt, args...) printk((fmt), ##args)

struct palacios_host_device_user {
    spinlock_t lock;
    int      connected;    // is the user space connected to this?
    int      waiting;      // am I waiting for a user-space response?

    int      fd;           // what is the user space fd?

    char     url[MAX_URL]; // what is the url describing the device

    v3_guest_dev_t guestdev; // what is the palacios-side device

    wait_queue_head_t  user_wait_queue; // user space processes waiting on us (should be only one)
    wait_queue_head_t  host_wait_queue; // host threads (should only be one) waiting on user space

    struct v3_guest                                *guest; // my guest
    struct palacios_host_dev_host_request_response *req;   // curent request
    struct palacios_host_dev_host_request_response *resp;  // curent response

    struct list_head  node;   // for adding me to the list of hostdevs this VM has
};


/**************************************************************************************
  Utility functions
*************************************************************************************/

static void palacios_host_dev_user_free(struct palacios_host_device_user *dev)
{
    if (dev->req) {
	kfree(dev->req);
	dev->req=0;
    } 
    if (dev->resp) { 
	kfree(dev->resp);
	dev->resp=0;
    }
    kfree(dev);
}


//
// Is this structure big enough for the data_size we will use?
//
// THIS FUNCTION CAN BE CALLED WHILE INTERRUPTS ARE OFF
//
static int palacios_bigenough_reqresp(struct palacios_host_dev_host_request_response *r, uint64_t data_size)
{
    if (!r) { 
	return 0;
    } else {
	if (((r->len)-sizeof(struct palacios_host_dev_host_request_response)) < data_size) {
	    return 0;
	} else {
	    return 1;
	}
    }
}

//
// Resize a request/response structure so that it will fit data_size bytes
//
// At the end of this, *r->len >= sizeof(struct)+data_size
//
// THIS FUNCTION MAY SLEEP AS IT CALLS KMALLOC 
// DO NOT CALL IT WHILE HOLDING A SPIN LOCK WITH INTERRUPTS OFF
//
static int palacios_resize_reqresp(struct palacios_host_dev_host_request_response **r, uint64_t data_size, int copy)
{
    
    DEEP_DEBUG_PRINT("palacios: hostdev: resize 0x%p to %llu\n",*r,data_size);

    if ((*r)==0) { 
	// allocate it
	DEEP_DEBUG_PRINT("palacios: hostdev: attempt alloc\n");
	*r = kmalloc(sizeof(struct palacios_host_dev_host_request_response)+data_size,GFP_KERNEL);
	DEEP_DEBUG_PRINT("palacios: hostdev: kmalloc done\n");
	if ((*r)==0) { 
	    ERROR("palacios: hostdev: failed to allocate\n");
	    return -1;
	} else {
	    (*r)->len=sizeof(struct palacios_host_dev_host_request_response)+data_size;
	    DEEP_DEBUG_PRINT("palacios: hostdev: allocated\n");
	    return 0;
	}
    } else {
	//let it go if it's big enough
	uint64_t cur_len = (*r)->len-sizeof(struct palacios_host_dev_host_request_response);

	if (data_size<=cur_len) { 
	    // do nothing
	    DEEP_DEBUG_PRINT("palacios: hostdev: size ok\n");
	    return 0;
	} else {
	    struct palacios_host_dev_host_request_response *new;

	    if (!copy) { 
		kfree(*r);
		*r=0;
	    }
	    new = kmalloc(sizeof(struct palacios_host_dev_host_request_response)+data_size,GFP_KERNEL);
	    if (!new) { 
		ERROR("palacios: hostdev: failed to reallocate\n");
		return -1;
	    } else {
		new->len=sizeof(struct palacios_host_dev_host_request_response)+data_size;
		if (copy) { 
		    memcpy(new->data,(*r)->data,(*r)->data_len-sizeof(struct palacios_host_dev_host_request_response));
		    new->data_len=(*r)->data_len;
		    kfree(*r);
		}
		*r=new;
		DEEP_DEBUG_PRINT("palacios: hostdev: reallocated\n");
		return 0;
	    }
	}
    }
}

static void cycle_request_response(struct palacios_host_device_user *dev)
{
    DEEP_DEBUG_PRINT("palacios: hostdev: cycle request to response\n");
    // wake up user side so that polls fall through
    wake_up_interruptible(&(dev->user_wait_queue));
    // put us to sleep until the user side wakes us up
    while (wait_event_interruptible((dev->host_wait_queue), (dev->waiting==0)) != 0) {}

    DEEP_DEBUG_PRINT("palacios: hostdev: cycle request to response - done\n");
}

static void cycle_response_request(struct palacios_host_device_user *dev)
{
    DEEP_DEBUG_PRINT("palacios: hostdev: cycle response to request\n");
    // wake up host side
    wake_up_interruptible(&(dev->host_wait_queue));
    DEEP_DEBUG_PRINT("palacios: hostdev: cycle response to request - done\n");
}

    
/*********************************************************************************************

    Interface to user space

 *********************************************************************************************/ 



static unsigned int host_dev_poll(struct file * filp, 
				  struct poll_table_struct * poll_tb) 
{

    struct palacios_host_device_user * dev = filp->private_data;
    unsigned long f;

    SHALLOW_DEBUG_PRINT("palacios: hostdev: poll\n");

    if (!dev->connected) { 
	ERROR("palcios: hostdev: poll on unconnected device\n");
	return -EFAULT;
    }

    spin_lock_irqsave(&(dev->lock),f);

    if (dev->waiting) { 
	// Yes, we have a request if you want it!
	spin_unlock_irqrestore(&(dev->lock),f);
	DEEP_DEBUG_PRINT("palacios: hostdev: poll done immediate\n");
	return  POLLIN | POLLRDNORM;
    } 

    // No request yet, so we need to wait for one to show up.

    // register ourselves on the user wait queue
    poll_wait(filp, &(dev->user_wait_queue), poll_tb);

    spin_unlock_irqrestore(&(dev->lock),f);

    DEEP_DEBUG_PRINT("palacios: hostdev: poll delayed\n");
    // We will get called again when that queue is woken up

    return 0;
}


static int host_dev_release(struct inode * i, struct file * filp) 
{
    struct palacios_host_device_user *dev = filp->private_data;
    unsigned long f;

    INFO("palacios: user side is closing host device \"%s\"\n",dev->url);
    
    spin_lock_irqsave(&(dev->lock), f);
    dev->connected = 0;
    spin_unlock_irqrestore(&(dev->lock), f);

    // it is the palacios->host interface's responsibility to ignore
    // reads/writes until connected is true

    return 0;
}


static int host_dev_ioctl(struct inode *ip, struct file *fp, unsigned int val, unsigned long arg)
{
    void __user *argp = (void __user *)arg;

    struct palacios_host_device_user *dev = fp->private_data;

    DEEP_DEBUG_PRINT("palacios: hostdev: ioctl %u\n",val);
    

    if (!dev->connected) { 
	ERROR("palacios: hostdev: ioctl on unconnected device\n");
	return -EFAULT;
    }
    
    switch (val) { 
	case V3_HOST_DEV_USER_REQUEST_PUSH_IOCTL: {
	    
	    struct palacios_host_dev_user_op op;
	    
	    if (copy_from_user(&op,argp,sizeof(struct palacios_host_dev_user_op))) { 
		ERROR("palacios: unable to copy from user for host device \"%s\"\n",dev->url);
		return -EFAULT;
	    }

	    DEEP_DEBUG_PRINT("palacios: hostdev: user request push, type %d\n",op.type);
	    
	    switch (op.type) { 
		case PALACIOS_HOST_DEV_USER_REQUEST_READ_GUEST: {
		    void *temp = kmalloc(op.len,GFP_KERNEL);

		    DEEP_DEBUG_PRINT("palacios: hostdev: read guest\n");

		    if (!temp) { 
			ERROR("palacios: unable to allocate enough for read guest request for host device \"%s\"\n",dev->url);
			return -EFAULT;
		    }
		    
		    if (v3_host_dev_read_guest_mem(dev->guestdev,
						   dev,
						   op.gpa,
						   temp,
						   op.len) != op.len) {
			ERROR("palacios: unable to read enough from guest for host device \"%s\"\n",dev->url);
			kfree(temp);
			return -EFAULT;
		    }
		    
		    if (copy_to_user(op.data,temp,op.len)) { 
			ERROR("palacios: unable to copy to user for host device \"%s\"\n",dev->url);
			kfree(temp);
			return -EFAULT;
		    }

		    kfree(temp);

		    return op.len;
		}
		    break;
		    

		case PALACIOS_HOST_DEV_USER_REQUEST_WRITE_GUEST: {
		    void *temp;
		    
		    DEEP_DEBUG_PRINT("palacios: hostdev: write guest\n");

		    temp = kmalloc(op.len,GFP_KERNEL);

		    if (!temp) { 
			ERROR("palacios: unable to allocate enough for write guest request for host device \"%s\"\n",dev->url);
			return -EFAULT;
		    }
		    
		    if (copy_from_user(temp,op.data,op.len)) { 
			ERROR("palacios: unable to copy from user for host device \"%s\"\n",dev->url);
			kfree(temp);
			return -EFAULT;
		    }
		    
		    if (v3_host_dev_write_guest_mem(dev->guestdev,
						    dev,
						    op.gpa,
						    temp,
						    op.len) != op.len) {
			ERROR("palacios: unable to write enough to guest for host device \"%s\"\n",dev->url);
			kfree(temp);
			return -EFAULT;
		    }

		    kfree(temp);
		    
		    return op.len;
		}
		    break;

		case PALACIOS_HOST_DEV_USER_REQUEST_IRQ_GUEST: {

		    DEEP_DEBUG_PRINT("palacios: hostdev: irq guest\n");

		    return  v3_host_dev_raise_irq(dev->guestdev, dev, op.irq);
		}
		    break;

		default:
		    ERROR("palacios: unknown user request to host device \"%s\"\n",dev->url);
		    return -EFAULT;
		    break;
	    }
	}
	    break;

	case V3_HOST_DEV_HOST_REQUEST_SIZE_IOCTL: {
	    
	    unsigned long f;


	    DEEP_DEBUG_PRINT("palacios: hostdev: request size of request\n");
	    
	    spin_lock_irqsave(&(dev->lock),f);
	    
	    if (!(dev->waiting)) { 
		spin_unlock_irqrestore(&(dev->lock),f);
		DEEP_DEBUG_PRINT("palacios: hostdev: no request available\n");
		schedule();  // avoid livelock for polling user space process  SUSPICOUS
		return 0; // no request available now
	    } 
	    
	    if (copy_to_user(argp,&(dev->req->data_len),sizeof(uint64_t))) { 
		spin_unlock_irqrestore(&(dev->lock),f);
		ERROR("palacios: unable to copy to user for host device \"%s\"\n",dev->url);
		return -EFAULT; // failed to copy!

	    }
	    
	    spin_unlock_irqrestore(&(dev->lock),f);

	    DEEP_DEBUG_PRINT("palacios: hostdev: have request\n");

	    return 1; // have request for you
	    
	}
	    
	    break;
	    
	case V3_HOST_DEV_HOST_REQUEST_PULL_IOCTL: {
	    
	    unsigned long f;
	    
	    spin_lock_irqsave(&(dev->lock),f);
	    
	    DEEP_DEBUG_PRINT("palacios: hostdev: pull request\n");

	    if (!(dev->waiting) || !(dev->req)) { 
		spin_unlock_irqrestore(&(dev->lock),f);
		DEEP_DEBUG_PRINT("palacios: hostdev: no request to pull\n");
		return 0; // no request available now
	    } 

	    
	    if (copy_to_user(argp,dev->req,dev->req->data_len)) { 
		spin_unlock_irqrestore(&(dev->lock),f);
		ERROR("palacios: unable to copy to user for host device \"%s\"\n",dev->url);
		return -EFAULT; // failed to copy!
	    }
    
	    spin_unlock_irqrestore(&(dev->lock),f);

	    DEEP_DEBUG_PRINT("palacios: hostdev: request pulled\n");
	    
	    return 1; // copied request for you
	}
	    break;
	    
	case V3_HOST_DEV_USER_RESPONSE_PUSH_IOCTL: {

	    unsigned long f;
	    uint64_t user_datalen;
	    uint64_t old_len;
	    
	    spin_lock_irqsave(&(dev->lock),f);
	    
	    DEEP_DEBUG_PRINT("palacios: hostdev: push response\n");

	    if (!(dev->waiting)) { 
		spin_unlock_irqrestore(&(dev->lock),f);
		ERROR("palacios: hostdev: no matching request for pushed response\n");
		return 0; // no request outstanding, so we do not need a response!
	    }
	    
	    if (copy_from_user(&user_datalen,argp,sizeof(uint64_t))) { 
		spin_unlock_irqrestore(&(dev->lock),f);
		ERROR("palacios: unable to copy from user for host device \"%s\"\n",dev->url);
		return -EFAULT; // failed to copy!
	    } 

	    if (user_datalen<sizeof(struct palacios_host_dev_host_request_response)) { 
		// bad user
		spin_unlock_irqrestore(&(dev->lock),f);
		ERROR("palacios: user has response that is too small on host device \"%s\"\n",dev->url);
		return -EFAULT;
	    }

	    if (!palacios_bigenough_reqresp(dev->resp,user_datalen-sizeof(struct palacios_host_dev_host_request_response))) {
		// not enough room.
		// we drop the lock, turn on interrupts, resize, and then retry
		DEEP_DEBUG_PRINT("palacios: response not big enough, dropping lock to resize on device \"%s\"\n",dev->url);

		spin_unlock_irqrestore(&(dev->lock),f);
		
		if (palacios_resize_reqresp(&(dev->resp),user_datalen-sizeof(struct palacios_host_dev_host_request_response),0)) {
		    ERROR("palacios: unable to resize to accept response of size %llu from user for host device \"%s\"\n",user_datalen,dev->url);
		    return -EFAULT;
		} else {
		    // reacquire the lock
		    // There shouldn't be a race here, since there should
		    // be exactly one user space thread giving us a response for this device
		    // and it is blocked waiting for us to finish
		    spin_lock_irqsave(&(dev->lock),f);
		    DEEP_DEBUG_PRINT("palacios: reacuired lock on device \"%s\"\n",dev->url);
		}
	    }

	    //We only copy data_len bytes from user, but we will
	    //overwrite the len field, so we preserve and then restore
	    old_len = dev->resp->len;
	    if (copy_from_user(dev->resp, argp, user_datalen)) { 
		dev->resp->len=old_len;
		spin_unlock_irqrestore(&(dev->lock),f);
		ERROR("palacios: unable to copy from user for host device \"%s\"\n",dev->url);
		return -EFAULT; // failed to copy!
	    } 
	    dev->resp->len=old_len;
	    
	    DEEP_DEBUG_PRINT("palacios: hostdev: valid response pushed\n");
	    // now have valid response!
	    dev->waiting=0;

	    spin_unlock_irqrestore(&(dev->lock),f);

	    // wake the palacios side up so that it sees it
	    cycle_response_request(dev);

	    return 1; // done
	}
	    break;
	    
	default:
	    ERROR("palacios: unknown ioctl for host device \"%s\"\n",dev->url);
	    return -EFAULT;
	    break;
    }
    
}




static struct file_operations host_dev_fops = {
    .poll     = host_dev_poll,
    .release  = host_dev_release,
    .ioctl    = host_dev_ioctl,
};



static int host_dev_connect(struct v3_guest * guest, unsigned int cmd, unsigned long arg, void * priv_data) 
{
    void __user * argp = (void __user *)arg;
    char url[MAX_URL];
    struct palacios_host_dev * host_dev = priv_data;
    struct palacios_host_device_user *dev;
    unsigned long f1, f2;
    int i;



    if (copy_from_user(url, argp, MAX_URL)) {
	printk("copy from user error getting url for host device connect...\n");
	return -EFAULT;
    }

    // currently only support user: types:
    if (strncasecmp(url,"user:",5)) { 
	ERROR("palacios: do not currently support host devices of type in \"%s\"\n",url);
	return -1;
    }
    
    INFO("palacios: attempting to rendezvous with palacios side of host device \"%s\"\n",url);
    
    // We will scan the list looking for the relevant
    // URL.  If we don't find it after a while, we give up
    
    for (i=0;i<RENDEZVOUS_WAIT_SECS/RENDEZVOUS_RETRY_SECS;i++) { 
	spin_lock_irqsave(&(host_dev->lock),f1);
	list_for_each_entry(dev,&(host_dev->devs), node) {
	    if (!strncasecmp(url,dev->url,MAX_URL)) { 
		// found it
		spin_lock_irqsave(&(dev->lock),f2);
		if (dev->connected) { 
		    ERROR("palacios: device for \"%s\" is already connected!\n",url);
		    spin_unlock_irqrestore(&(dev->lock),f2);
		    spin_unlock_irqrestore(&(host_dev->lock),f1);
		    return -1;
		} else {
		    dev->fd = anon_inode_getfd("v3-hostdev", &host_dev_fops, dev, 0);
		    if (dev->fd<0) { 
			ERROR("palacios: cannot create fd for device \"%s\"\n",url);
			spin_unlock_irqrestore(&(dev->lock),f2);
			spin_unlock_irqrestore(&(host_dev->lock),f1);
			return -1;
		    }
		    dev->connected=1;
		    dev->waiting=0;
		    if (dev->req) { 
			kfree(dev->req);
			dev->req=0;
		    } 
		    if (dev->resp) { 
			kfree(dev->resp);
			dev->resp=0;
		    }
		    INFO("palacios: connected fd for device \"%s\"\n",url);
		    spin_unlock_irqrestore(&(dev->lock),f2);
		    spin_unlock_irqrestore(&(host_dev->lock),f1);
		    return dev->fd;
		}
		spin_unlock_irqrestore(&(dev->lock),f2);
	    }
	}
	spin_unlock_irqrestore(&(host_dev->lock),f1);
	
	ssleep(RENDEZVOUS_RETRY_SECS);
    }
    
    ERROR("palacios: timeout waiting for connection for device \"%s\"",url);
    
    return -1;
    
}








/***************************************************************************************

   Following this is the implementation of the palacios->host interface

**************************************************************************************/


/* Attempt to rendezvous with the user device if no device is currently connected */
static int palacios_host_dev_rendezvous(struct palacios_host_device_user *dev)
{
    unsigned long f;
    int i;

    if (dev->connected) { 
	return 0;
    }

   
    INFO("palacios: attempting new rendezvous for host device \"%s\"\n",dev->url);

    // Now wait until we are noticed!
    for (i=0;i<RENDEZVOUS_WAIT_SECS/RENDEZVOUS_RETRY_SECS;i++) { 
	spin_lock_irqsave(&(dev->lock),f);
	if (dev->connected) { 
	    INFO("palacios: connection with user side established for host device \"%s\" fd=%d\n",dev->url,dev->fd);
	    spin_unlock_irqrestore(&(dev->lock),f);
	    return 0;
	}
	spin_unlock_irqrestore(&(dev->lock),f);
	ssleep(RENDEZVOUS_RETRY_SECS);
    }
    
    ERROR("palacios: timeout waiting for user side to connect to host device \"%s\"",dev->url);

    // We stay in the list because a future rendezvous might happen
    
    return -1;
}


/* Creates the device without rendezvous */
static v3_host_dev_t palacios_host_dev_open_deferred(char *url,
						     v3_bus_class_t bus,
						     v3_guest_dev_t gdev,
						     void *host_priv_data)
{
    struct v3_guest *guest= (struct v3_guest*)host_priv_data;
    struct palacios_host_device_user *dev;
    struct palacios_host_dev * host_dev = NULL;
    unsigned long f;

    /*
      I will create the device in the list and then wait
      for the user side to attach
    */

    if (guest == NULL) {
	return 0;
    }
    

    host_dev = get_vm_ext_data(guest, "HOST_DEVICE_INTERFACE");

    if (host_dev == NULL) {
	printk("Error locating vm host data for HOST_DEVICE_INTERFACE\n");
	return 0;
    }


    if (strncasecmp(url,"user:",5)) { 
	ERROR("palacios: do not currently support devices of type in \"%s\"\n",url);
	return NULL;
    }

    // Check to see if a device of this url already exists, which would be ugly
    spin_lock_irqsave(&(host_dev->lock),f);
    list_for_each_entry(dev,&(host_dev->devs), node) {
	if (!strncasecmp(url,dev->url,MAX_URL)) { 
	    // found it
	    spin_unlock_irqrestore(&(host_dev->lock),f);
	    ERROR("palacios: a host device with url \"%s\" already exists in the guest!\n",url);
	    return NULL;
	}
    }
    spin_unlock_irqrestore(&(host_dev->lock),f);


    INFO("palacios: creating host device \"%s\"\n",url);

    dev = kmalloc(sizeof(struct palacios_host_device_user),GFP_KERNEL);
    
    if (!dev) { 
	ERROR("palacios: cannot allocate for host device \"%s\"\n",url);
	return NULL;
    }

    memset(dev,0,sizeof(struct palacios_host_device_user));
    
    strncpy(dev->url,url,MAX_URL);
    
    dev->guestdev = gdev;
    
    dev->guest = guest;

    spin_lock_init(&(dev->lock));

    init_waitqueue_head(&(dev->user_wait_queue));
    init_waitqueue_head(&(dev->host_wait_queue));

    // Insert ourselves into the list
    spin_lock_irqsave(&(host_dev->lock),f);
    list_add(&(dev->node),&(host_dev->devs));
    spin_unlock_irqrestore(&(host_dev->lock),f);

    INFO("palacios: host device \"%s\" created with deferred rendezvous\n",url);

    return dev;

}



static int palacios_host_dev_close(v3_host_dev_t hostdev)
{
    unsigned long f1, f2;

    struct palacios_host_device_user *dev = (struct palacios_host_device_user *) hostdev;
    struct palacios_host_dev * host_dev = NULL;

    INFO("palacios: closing host device \"%s\"\n",dev->url);

    if ((dev == NULL) || (dev->guest == NULL)) {
	return -1;
    }

    host_dev = get_vm_ext_data(dev->guest, "HOST_DEVICE_INTERFACE");

    
    if (host_dev == NULL) {
	return -1;
    }

    spin_lock_irqsave(&(host_dev->lock),f1);

    spin_lock_irqsave(&(dev->lock),f2);

    if (dev->connected) { 
	dev->connected=0;
	// After this, any user side request will return -EFAULT
    }

    list_del(&(dev->node));
    
    spin_unlock_irqrestore(&(dev->lock),f2);
    spin_unlock_irqrestore(&(host_dev->lock),f1);
    
    palacios_host_dev_user_free(dev);

    return 0;
}



	
static uint64_t palacios_host_dev_read_io(v3_host_dev_t hostdev,
					  uint16_t      port,
					  void          *dest,
					  uint64_t      len)
{
    struct palacios_host_device_user *dev = (struct palacios_host_device_user *)hostdev;
    unsigned long f;
    uint64_t op_len;

    DEEP_DEBUG_PRINT("palacios: hostdev: read io port 0x%x\n",port);
	    

    spin_lock_irqsave(&(dev->lock),f);
    
    if (palacios_host_dev_rendezvous(dev)) {
	spin_unlock_irqrestore(&(dev->lock),f);
	ERROR("palacios: ignoring request as user side is not connected (and did not rendezvous) for host device \"%s\"\n",dev->url);
	return 0;
    }

    if (dev->waiting) { 
	spin_unlock_irqrestore(&(dev->lock),f);
	ERROR("palacios: guest issued i/o read request with host device \"%s\" in wrong state (waiting=%d, connected=%d)\n",dev->url,dev->waiting,dev->connected);
	return 0;
    }

    
    
    // resize request (no data)
    if (!palacios_bigenough_reqresp(dev->req,0)) {
	// not enough room.
	// we drop the lock, turn on interrupts, resize, and then retry
	DEEP_DEBUG_PRINT("palacios: request not big enough, dropping lock to resize on device \"%s\"\n",dev->url);
	
	spin_unlock_irqrestore(&(dev->lock),f);
	
	if (palacios_resize_reqresp(&(dev->req),0,0)) {
	    ERROR("palacios: cannot resize for request on device \"%s\"\n",dev->url);
	    return 0;
	} else {
	    // reacquire the lock
	    // There shouldn't be a race here since there should not be another
	    // request from palacios until this one finishes
	    spin_lock_irqsave(&(dev->lock),f);
	    DEEP_DEBUG_PRINT("palacios: reacquired lock on device \"%s\"\n",dev->url);
	}
    }
    

    dev->req->type=PALACIOS_HOST_DEV_HOST_REQUEST_READ_IO;
    dev->req->port=port;
    dev->req->op_len=len;
    dev->req->gpa=0;
    dev->req->conf_addr=0;
    dev->req->data_len=sizeof(struct palacios_host_dev_host_request_response);

    dev->waiting=1;
    
    spin_unlock_irqrestore(&(dev->lock),f);

    // hand over to the user space and wait for it to respond
    cycle_request_response(dev);

    // We're back!   So now we'll hand the response back to Palacios

    spin_lock_irqsave(&(dev->lock),f);

    op_len = dev->resp->op_len < len ? dev->resp->op_len : len ;

    memcpy(dest,dev->resp->data, op_len);
    
    spin_unlock_irqrestore(&(dev->lock),f);

    return op_len;
}

static uint64_t palacios_host_dev_read_mem(v3_host_dev_t hostdev,
					   void *        gpa,
					   void          *dest,
					   uint64_t      len)
{
    struct palacios_host_device_user *dev = (struct palacios_host_device_user *)hostdev;
    unsigned long f;
    uint64_t op_len;

    DEEP_DEBUG_PRINT("palacios: hostdev: read mem  0x%p\n",gpa);

    spin_lock_irqsave(&(dev->lock),f);
    
    if (palacios_host_dev_rendezvous(dev)) {
	spin_unlock_irqrestore(&(dev->lock),f);
	ERROR("palacios: ignoring request as user side is not connected (and did not rendezvous) for host device \"%s\"\n",dev->url);
	return 0;
    }

    if (dev->waiting) { 
	spin_unlock_irqrestore(&(dev->lock),f);
	ERROR("palacios: guest issued memory read request with host device \"%s\" in wrong state (waiting=%d, connected=%d)\n",dev->url,dev->waiting,dev->connected);
	return 0;
    }
    
    // resize request (no data)
    if (!palacios_bigenough_reqresp(dev->req,0)) {
	// not enough room.
	// we drop the lock, turn on interrupts, resize, and then retry
	DEEP_DEBUG_PRINT("palacios: request not big enough, dropping lock to resize on device \"%s\"\n",dev->url);
	
	spin_unlock_irqrestore(&(dev->lock),f);
	
	if (palacios_resize_reqresp(&(dev->req),0,0)) {
	    ERROR("palacios: cannot resize for request on device \"%s\"\n",dev->url);
	    return 0;
	} else {
	    // reacquire the lock
	    // There shouldn't be a race here since there should not be another
	    // request from palacios until this one finishes
	    spin_lock_irqsave(&(dev->lock),f);
	    DEEP_DEBUG_PRINT("palacios: reacquired lock on device \"%s\"\n",dev->url);
	}
    }

    dev->req->type=PALACIOS_HOST_DEV_HOST_REQUEST_READ_MEM;
    dev->req->port=0;
    dev->req->op_len=len;
    dev->req->gpa=gpa;
    dev->req->conf_addr=0;
    dev->req->data_len=sizeof(struct palacios_host_dev_host_request_response);

    dev->waiting=1;
    
    spin_unlock_irqrestore(&(dev->lock),f);

    // hand over to the user space and wait for it to respond
    cycle_request_response(dev);

    // We're back!   So now we'll hand the response back to Palacios

    spin_lock_irqsave(&(dev->lock),f);

    op_len = dev->resp->op_len < len ? dev->resp->op_len : len ;

    memcpy(dest,dev->resp->data, op_len);
    
    spin_unlock_irqrestore(&(dev->lock),f);

    return op_len;
}

static uint64_t palacios_host_dev_read_conf(v3_host_dev_t hostdev,
					    uint64_t      offset,
					    void          *dest,
					    uint64_t      len)
{
    struct palacios_host_device_user *dev = (struct palacios_host_device_user *)hostdev;
    unsigned long f;
    uint64_t op_len;

    DEEP_DEBUG_PRINT("palacios: hostdev: read conf 0x%p\n",(void*)offset);

    spin_lock_irqsave(&(dev->lock),f);
    
    if (palacios_host_dev_rendezvous(dev)) {
	spin_unlock_irqrestore(&(dev->lock),f);
	ERROR("palacios: ignoring request as user side is not connected (and did not rendezvous) for host device \"%s\"\n",dev->url);
	return 0;
    }

    if (dev->waiting) { 
	spin_unlock_irqrestore(&(dev->lock),f);
	ERROR("palacios: guest issued config read request with host device \"%s\" in wrong state (waiting=%d, connected=%d)\n",dev->url,dev->waiting,dev->connected);
	return 0;
    }
    
    // resize request (no data)
    if (!palacios_bigenough_reqresp(dev->req,0)) {
	// not enough room.
	// we drop the lock, turn on interrupts, resize, and then retry
	DEEP_DEBUG_PRINT("palacios: request not big enough, dropping lock to resize on device \"%s\"\n",dev->url);
	
	spin_unlock_irqrestore(&(dev->lock),f);
	
	if (palacios_resize_reqresp(&(dev->req),0,0)) {
	    ERROR("palacios: cannot resize for request on device \"%s\"\n",dev->url);
	    return 0;
	} else {
	    // reacquire the lock
	    // There shouldn't be a race here since there should not be another
	    // request from palacios until this one finishes
	    spin_lock_irqsave(&(dev->lock),f);
	    DEEP_DEBUG_PRINT("palacios: reacquired lock on device \"%s\"\n",dev->url);
	}
    }

    dev->req->type=PALACIOS_HOST_DEV_HOST_REQUEST_READ_CONF;
    dev->req->port=0;
    dev->req->op_len=len;
    dev->req->gpa=0;
    dev->req->conf_addr=offset;
    dev->req->data_len=sizeof(struct palacios_host_dev_host_request_response);

    dev->waiting=1;
    
    spin_unlock_irqrestore(&(dev->lock),f);

    // hand over to the user space and wait for it to respond
    cycle_request_response(dev);

    // We're back!   So now we'll hand the response back to Palacios

    spin_lock_irqsave(&(dev->lock),f);

    op_len = dev->resp->op_len < len ? dev->resp->op_len : len ;

    memcpy(dest,dev->resp->data, op_len);
    
    spin_unlock_irqrestore(&(dev->lock),f);

    return op_len;
}


static uint64_t palacios_host_dev_write_io(v3_host_dev_t hostdev,
					   uint16_t      port,
					   void          *src,
					   uint64_t      len)
{
    struct palacios_host_device_user *dev = (struct palacios_host_device_user *)hostdev;
    unsigned long f;
    uint64_t op_len;

    DEEP_DEBUG_PRINT("palacios: hostdev: write io port 0x%x \n",port);

    spin_lock_irqsave(&(dev->lock),f);
    
    if (palacios_host_dev_rendezvous(dev)) {
	spin_unlock_irqrestore(&(dev->lock),f);
	ERROR("palacios: ignoring request as user side is not connected (and did not rendezvous) for host device \"%s\"\n",dev->url);
	return 0;
    }

    if (dev->waiting) { 
	spin_unlock_irqrestore(&(dev->lock),f);
	ERROR("palacios: guest issued i/o write request with host device \"%s\" in wrong state (waiting=%d, connected=%d)\n",dev->url,dev->waiting,dev->connected);
	return 0;
    }

    // resize request 
    if (!palacios_bigenough_reqresp(dev->req,len)) {
	// not enough room.
	// we drop the lock, turn on interrupts, resize, and then retry
	DEEP_DEBUG_PRINT("palacios: request not big enough, dropping lock to resize on device \"%s\"\n",dev->url);
	
	spin_unlock_irqrestore(&(dev->lock),f);
	
	if (palacios_resize_reqresp(&(dev->req),len,0)) {
	    ERROR("palacios: cannot resize for request on device \"%s\"\n",dev->url);
	    return 0;
	} else {
	    // reacquire the lock
	    // There shouldn't be a race here since there should not be another
	    // request from palacios until this one finishes
	    spin_lock_irqsave(&(dev->lock),f);
	    DEEP_DEBUG_PRINT("palacios: reacquired lock on device \"%s\"\n",dev->url);
	}
    }

    dev->req->type=PALACIOS_HOST_DEV_HOST_REQUEST_WRITE_IO;
    dev->req->port=port;
    dev->req->op_len=len;
    dev->req->gpa=0;
    dev->req->conf_addr=0;
    dev->req->data_len=sizeof(struct palacios_host_dev_host_request_response)+len;

    memcpy(dev->req->data,src,len);

    dev->waiting=1;

    spin_unlock_irqrestore(&(dev->lock),f);
   
    // hand over to the user space and wait for it to respond
    cycle_request_response(dev);

    // We're back!   So now we'll hand the response back to Palacios

    spin_lock_irqsave(&(dev->lock),f);

    op_len = dev->resp->op_len < len ? dev->resp->op_len : len ;

    spin_unlock_irqrestore(&(dev->lock),f);

    return op_len;
}


static uint64_t palacios_host_dev_write_mem(v3_host_dev_t hostdev,
					    void *        gpa,
					    void          *src,
					    uint64_t      len)
{
    struct palacios_host_device_user *dev = (struct palacios_host_device_user *)hostdev;
    unsigned long f;
    uint64_t op_len;

    DEEP_DEBUG_PRINT("palacios: hostdev: write mem 0x%p\n",gpa);

    spin_lock_irqsave(&(dev->lock),f);
    
    if (palacios_host_dev_rendezvous(dev)) {
	spin_unlock_irqrestore(&(dev->lock),f);
	ERROR("palacios: ignoring request as user side is not connected (and did not rendezvous) for host device \"%s\"\n",dev->url);
	return 0;
    }

    if (dev->waiting) { 
	spin_unlock_irqrestore(&(dev->lock),f);
	ERROR("palacios: guest issued memory write request with host device \"%s\" in wrong state (waiting=%d, connected=%d)\n",dev->url,dev->waiting,dev->connected);
	return 0;
    }
    
    // resize request 
    if (!palacios_bigenough_reqresp(dev->req,len)) {
	// not enough room.
	// we drop the lock, turn on interrupts, resize, and then retry
	DEEP_DEBUG_PRINT("palacios: request not big enough, dropping lock to resize on device \"%s\"\n",dev->url);
	
	spin_unlock_irqrestore(&(dev->lock),f);
	
	if (palacios_resize_reqresp(&(dev->req),len,0)) {
	    ERROR("palacios: cannot resize for request on device \"%s\"\n",dev->url);
	    return 0;
	} else {
	    // reacquire the lock
	    // There shouldn't be a race here since there should not be another
	    // request from palacios until this one finishes
	    spin_lock_irqsave(&(dev->lock),f);
	    DEEP_DEBUG_PRINT("palacios: reacquired lock on device \"%s\"\n",dev->url);
	}
    }

    dev->req->type=PALACIOS_HOST_DEV_HOST_REQUEST_WRITE_MEM;
    dev->req->port=0;
    dev->req->op_len=len;
    dev->req->gpa=gpa;
    dev->req->conf_addr=0;
    dev->req->data_len=sizeof(struct palacios_host_dev_host_request_response)+len;

    memcpy(dev->req->data,src,len);

    dev->waiting=1;
    
    spin_unlock_irqrestore(&(dev->lock),f);

    // hand over to the user space and wait for it to respond
    cycle_request_response(dev);

    // We're back!   So now we'll hand the response back to Palacios

    spin_lock_irqsave(&(dev->lock),f);

    op_len= dev->resp->op_len < len ? dev->resp->op_len : len ;

    spin_unlock_irqrestore(&(dev->lock),f);

    return op_len;
}




static uint64_t palacios_host_dev_write_conf(v3_host_dev_t hostdev,
					     uint64_t      offset,
					     void          *src,
					     uint64_t      len)
{
    struct palacios_host_device_user *dev = (struct palacios_host_device_user *)hostdev;
    unsigned long f;
    uint64_t op_len;

    DEEP_DEBUG_PRINT("palacios: hostdev: write conf 0x%p\n",(void*)offset);

    spin_lock_irqsave(&(dev->lock),f);
    
    if (palacios_host_dev_rendezvous(dev)) {
	spin_unlock_irqrestore(&(dev->lock),f);
	ERROR("palacios: ignoring request as user side is not connected (and did not rendezvous) for host device \"%s\"\n",dev->url);
	return 0;
    }

    if (dev->waiting) { 
	spin_unlock_irqrestore(&(dev->lock),f);
	ERROR("palacios: guest issued config write request with host device \"%s\" in wrong state (waiting=%d, connected=%d)\n",dev->url,dev->waiting,dev->connected);
	return 0;
    }
    
    // resize request 
    if (!palacios_bigenough_reqresp(dev->req,len)) {
	// not enough room.
	// we drop the lock, turn on interrupts, resize, and then retry
	DEEP_DEBUG_PRINT("palacios: request not big enough, dropping lock to resize on device \"%s\"\n",dev->url);
	
	spin_unlock_irqrestore(&(dev->lock),f);
	
	if (palacios_resize_reqresp(&(dev->req),len,0)) {
	    ERROR("palacios: cannot resize for request on device \"%s\"\n",dev->url);
	    return 0;
	} else {
	    // reacquire the lock
	    // There shouldn't be a race here since there should not be another
	    // request from palacios until this one finishes
	    spin_lock_irqsave(&(dev->lock),f);
	    DEEP_DEBUG_PRINT("palacios: reacquired lock on device \"%s\"\n",dev->url);
	}
    }

    dev->req->type=PALACIOS_HOST_DEV_HOST_REQUEST_WRITE_CONF;
    dev->req->port=0;
    dev->req->op_len=len;
    dev->req->gpa=0;
    dev->req->conf_addr=offset;
    dev->req->data_len=sizeof(struct palacios_host_dev_host_request_response)+len;

    memcpy(dev->req->data,src,len);

    dev->waiting=1;
    
    spin_unlock_irqrestore(&(dev->lock),f);

    // hand over to the user space and wait for it to respond
    cycle_request_response(dev);

    // We're back!   So now we'll hand the response back to Palacios

    spin_lock_irqsave(&(dev->lock),f);

    op_len = dev->resp->op_len < len ? dev->resp->op_len : len ;

    spin_unlock_irqrestore(&(dev->lock),f);

    return op_len;
}
 
 
static int palacios_host_dev_ack_irq(v3_host_dev_t hostdev, uint8_t irq)
{
    // we don't care
    return 0;
}
 




static struct v3_host_dev_hooks palacios_host_dev_hooks = {
    .open			= palacios_host_dev_open_deferred,
    .close                      = palacios_host_dev_close,
    .read_io                    = palacios_host_dev_read_io,
    .write_io                   = palacios_host_dev_write_io,
    .read_mem                   = palacios_host_dev_read_mem,
    .write_mem                  = palacios_host_dev_write_mem,
    .read_config                = palacios_host_dev_read_conf,
    .write_config               = palacios_host_dev_write_conf,
    .ack_irq                    = palacios_host_dev_ack_irq,
};



static int host_dev_init( void ) {
    V3_Init_Host_Device_Support(&palacios_host_dev_hooks);
    
    return 0;
}


static int host_dev_guest_init(struct v3_guest * guest, void ** vm_data ) {
    struct palacios_host_dev * host_dev = kmalloc(sizeof(struct palacios_host_dev), GFP_KERNEL);

    if (!host_dev) { 
	ERROR("palacios: failed to do guest_init for host device\n");
	return -1;
    }
    
    
    INIT_LIST_HEAD(&(host_dev->devs));
    spin_lock_init(&(host_dev->lock));

    *vm_data = host_dev;


    add_guest_ctrl(guest, V3_VM_HOST_DEV_CONNECT, host_dev_connect, host_dev);

    return 0;
}





static struct linux_ext host_dev_ext = {
    .name = "HOST_DEVICE_INTERFACE",
    .init = host_dev_init,
    .deinit = NULL,
    .guest_init = host_dev_guest_init,
    .guest_deinit = NULL
};


register_extension(&host_dev_ext);
