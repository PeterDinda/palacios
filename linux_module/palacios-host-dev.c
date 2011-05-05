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
#include "palacios-host-dev.h"
#include "palacios-host-dev-user.h"



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
    

*/


#define MAX_URL 256

#define RENDEZVOUS_WAIT_SECS  60
#define RENDEZVOUS_RETRY_SECS 1


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

static int palacios_resize_reqresp(struct palacios_host_dev_host_request_response **r, uint64_t data_size, int copy)
{
    if (!*r) { 
	// allocate it
	*r = kmalloc(sizeof(struct palacios_host_dev_host_request_response)+data_size,GFP_KERNEL);
	if (!*r) { 
	    return -1;
	} else {
	    (*r)->len=sizeof(struct palacios_host_dev_host_request_response)+data_size;
	    return 0;
	}
    } else {
	//let it go if it's big enough
	uint64_t cur_len = (*r)->len-sizeof(struct palacios_host_dev_host_request_response);

	if (data_size<=cur_len) { 
	    // do nothing
	    return 0;
	} else {
	    struct palacios_host_dev_host_request_response *new;

	    if (!copy) { 
		kfree(*r);
		*r=0;
	    }
	    new = kmalloc(sizeof(struct palacios_host_dev_host_request_response)+data_size,GFP_KERNEL);
	    if (!new) { 
		return -1;
	    } else {
		new->len=sizeof(struct palacios_host_dev_host_request_response)+data_size;
		if (copy) { 
		    memcpy(new->data,(*r)->data,(*r)->data_len-sizeof(struct palacios_host_dev_host_request_response));
		    new->data_len=(*r)->data_len;
		    kfree(*r);
		}
		*r=new;
		return 0;
	    }
	}
    }
}

static void cycle_request_response(struct palacios_host_device_user *dev)
{
    // wake up user side so that polls fall through
    wake_up_interruptible(&(dev->user_wait_queue));
    // put us to sleep until the user side wakes us up
    wait_event_interruptible((dev->host_wait_queue), (dev->waiting==0));
}

static void cycle_response_request(struct palacios_host_device_user *dev)
{
    // wake up host side
    wake_up_interruptible(&(dev->host_wait_queue));
}

    
/*********************************************************************************************

    Interface to user space

 *********************************************************************************************/ 



static unsigned int host_dev_poll(struct file * filp, 
				  struct poll_table_struct * poll_tb) 
{

    struct palacios_host_device_user * dev = filp->private_data;
    unsigned long f;

    if (!dev->connected) { 
	return -EFAULT;
    }

    spin_lock_irqsave(&(dev->lock),f);

    if (dev->waiting) { 
	// Yes, we have a request if you want it!
	spin_unlock_irqrestore(&(dev->lock),f);
	return  POLLIN | POLLRDNORM;
    } 

    // No request yet, so we need to wait for one to show up.

    // register ourselves on the user wait queue
    poll_wait(filp, &(dev->user_wait_queue), poll_tb);

    spin_unlock_irqrestore(&(dev->lock),f);

    // We will get called again when that queue is woken up

    return 0;
}


static int host_dev_release(struct inode * i, struct file * filp) 
{
    struct palacios_host_device_user *dev = filp->private_data;
    unsigned long f;

    printk("palacios: user side is closing host device \"%s\"\n",dev->url);
    
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

    if (!dev->connected) { 
	return -EFAULT;
    }
    
    switch (val) { 
	case V3_HOST_DEV_USER_REQUEST_PUSH_IOCTL: {
	    
	    struct palacios_host_dev_user_op op;
	    
	    if (copy_from_user(&op,argp,sizeof(struct palacios_host_dev_user_op))) { 
		printk("palacios: unable to copy from user for host device \"%s\"\n",dev->url);
		return -EFAULT;
	    }
	    
	    switch (op.type) { 
		case PALACIOS_HOST_DEV_USER_REQUEST_READ_GUEST: {
		    void *temp = kmalloc(op.len,GFP_KERNEL);

		    if (!temp) { 
			printk("palacios: unable to allocate enough for read guest request for host device \"%s\"\n",dev->url);
			return -EFAULT;
		    }
		    
		    if (v3_host_dev_read_guest_mem(dev->guestdev,
						   dev,
						   op.gpa,
						   temp,
						   op.len) != op.len) {
			printk("palacios: unable to read enough from guest for host device \"%s\"\n",dev->url);
			kfree(temp);
			return -EFAULT;
		    }
		    
		    if (copy_to_user(op.data,temp,op.len)) { 
			printk("palacios: unable to copy to user for host device \"%s\"\n",dev->url);
			kfree(temp);
			return -EFAULT;
		    }

		    kfree(temp);

		    return op.len;
		}
		    break;
		    

		case PALACIOS_HOST_DEV_USER_REQUEST_WRITE_GUEST: {

		    void *temp = kmalloc(op.len,GFP_KERNEL);

		    if (!temp) { 
			printk("palacios: unable to allocate enough for write guest request for host device \"%s\"\n",dev->url);
			return -EFAULT;
		    }
		    
		    if (copy_from_user(temp,op.data,op.len)) { 
			printk("palacios: unable to copy from user for host device \"%s\"\n",dev->url);
			kfree(temp);
			return -EFAULT;
		    }
		    
		    if (v3_host_dev_write_guest_mem(dev->guestdev,
						    dev,
						    op.gpa,
						    temp,
						    op.len) != op.len) {
			printk("palacios: unable to write enough to guest for host device \"%s\"\n",dev->url);
			kfree(temp);
			return -EFAULT;
		    }

		    kfree(temp);
		    
		    return op.len;
		}
		    break;

		case PALACIOS_HOST_DEV_USER_REQUEST_IRQ_GUEST: {

		    return  v3_host_dev_raise_irq(dev->guestdev, dev, op.irq);
		}
		    break;

		default:
		    printk("palacios: unknown user request to host device \"%s\"\n",dev->url);
		    return -EFAULT;
		    break;
	    }
	}
	    break;

	case V3_HOST_DEV_HOST_REQUEST_SIZE_IOCTL: {
	    
	    unsigned long f;
	    
	    spin_lock_irqsave(&(dev->lock),f);
	    
	    if (!(dev->waiting)) { 
		spin_unlock_irqrestore(&(dev->lock),f);
		return 0; // no request available now
	    } 
	    
	    if (copy_to_user(argp,&(dev->req->data_len),sizeof(uint64_t))) { 
		printk("palacios: unable to copy to user for host device \"%s\"\n",dev->url);
		spin_unlock_irqrestore(&(dev->lock),f);
		return -EFAULT; // failed to copy!
	    }
	    
	    spin_unlock_irqrestore(&(dev->lock),f);
	    
	    return 1; // have request for you
	    
	}
	    
	    break;
	    
	case V3_HOST_DEV_HOST_REQUEST_PULL_IOCTL: {
	    
	    unsigned long f;
	    
	    spin_lock_irqsave(&(dev->lock),f);
	    
	    if (!(dev->waiting)) { 
		spin_unlock_irqrestore(&(dev->lock),f);
		return 0; // no request available now
	    } 
	    
	    if (copy_to_user(argp,dev->req,dev->req->data_len)) { 
		printk("palacios: unable to copy to user for host device \"%s\"\n",dev->url);
		spin_unlock_irqrestore(&(dev->lock),f);
		return -EFAULT; // failed to copy!
	    }
	    
	    spin_unlock_irqrestore(&(dev->lock),f);
	    
	    return 1; // copied request for you
	}
	    break;
	    
	case V3_HOST_DEV_USER_RESPONSE_PUSH_IOCTL: {

	    unsigned long f;
	    uint64_t user_datalen;
	    
	    spin_lock_irqsave(&(dev->lock),f);
	    
	    if (!(dev->waiting)) { 
		spin_unlock_irqrestore(&(dev->lock),f);
		return 0; // no request outstanding, so we do not need a response!
	    }
	    
	    if (copy_from_user(&user_datalen,argp,sizeof(uint64_t))) { 
		printk("palacios: unable to copy from user for host device \"%s\"\n",dev->url);
		spin_unlock_irqrestore(&(dev->lock),f);
		return -EFAULT; // failed to copy!
	    } 
	    
	    if (palacios_resize_reqresp(&(dev->resp),user_datalen,0)) {
		printk("palacios: unable to resize to accept request of size %llu from user for host device \"%s\"\n",user_datalen,dev->url);
		spin_unlock_irqrestore(&(dev->lock),f);
		return -EFAULT;
	    } 
	    
	    if (copy_from_user(dev->resp, argp, user_datalen)) { 
		printk("palacios: unable to copy from user for host device \"%s\"\n",dev->url);
		spin_unlock_irqrestore(&(dev->lock),f);
		return -EFAULT; // failed to copy!
	    } 
	    
	    // now have valid response!
	    dev->waiting=0;

	    // wake the palacios side up so that it sees it
	    cycle_response_request(dev);

	    spin_unlock_irqrestore(&(dev->lock),f);

	    return 1; // done
	}
	    break;
	    
	default:
	    printk("palacios: unknown ioctl for host device \"%s\"\n",dev->url);
	    return -EFAULT;
	    break;
    }
    
}


static struct file_operations host_dev_fops = {
    .poll     = host_dev_poll,
    .release  = host_dev_release,
    .ioctl    = host_dev_ioctl,
};



    
int connect_host_dev(struct v3_guest * guest, char *url) 
{
    struct palacios_host_device_user *dev;
    unsigned long f1, f2;
    int i;

    // currently only support user: types:
    if (strncasecmp(url,"user:",5)) { 
	printk("palacios: do not currently support host devices of type in \"%s\"\n",url);
	return -1;
    }
    
    printk("palacios: attempting to rendezvous with palacios side of host device \"%s\"\n",url);
    
    // We will scan the list looking for the relevant
    // URL.  If we don't find it after a while, we give up
    
    for (i=0;i<RENDEZVOUS_WAIT_SECS/RENDEZVOUS_RETRY_SECS;i++) { 
	spin_lock_irqsave(&(guest->hostdev.lock),f1);
	list_for_each_entry(dev,&(guest->hostdev.devs), node) {
	    if (!strncasecmp(url,dev->url,MAX_URL)) { 
		// found it
		spin_lock_irqsave(&(dev->lock),f2);
		if (dev->connected) { 
		    printk("palacios: device for \"%s\" is already connected!\n",url);
		    spin_unlock_irqrestore(&(dev->lock),f2);
		    spin_unlock_irqrestore(&(guest->hostdev.lock),f1);
		    return -1;
		} else {
		    dev->fd = anon_inode_getfd("v3-hostdev", &host_dev_fops, dev, 0);
		    if (dev->fd<0) { 
			printk("palacios: cannot create fd for device \"%s\"\n",url);
			spin_unlock_irqrestore(&(dev->lock),f2);
			spin_unlock_irqrestore(&(guest->hostdev.lock),f1);
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
		    printk("palacios: connected fd for device \"%s\"\n",url);
		    spin_unlock_irqrestore(&(dev->lock),f2);
		    spin_unlock_irqrestore(&(guest->hostdev.lock),f1);
		    return dev->fd;
		}
		spin_unlock_irqrestore(&(dev->lock),f2);
	    }
	}
	spin_unlock_irqrestore(&(guest->hostdev.lock),f1);
	
	ssleep(RENDEZVOUS_RETRY_SECS);
    }
    
    printk("palacios: timeout waiting for connection for device \"%s\"",url);
    
    return -1;
    
}








/***************************************************************************************

   Following this is the implementation of the palacios->host interface

**************************************************************************************/

static v3_host_dev_t palacios_host_dev_open(char *url,
					    v3_bus_class_t bus,
					    v3_guest_dev_t gdev,
					    void *host_priv_data)
{
    struct v3_guest *guest= (struct v3_guest*)host_priv_data;
    struct palacios_host_device_user *dev;
    unsigned long f1,f2;
    int i;

    /*
      I will create the device in the list and then wait
      for the user side to attach
    */


    if (strncasecmp(url,"user:",5)) { 
	printk("palacios: do not currently support devices of type in \"%s\"\n",url);
	return NULL;
    }

    // Check to see if a device of this url already exists, which would be ugly
    spin_lock_irqsave(&(guest->hostdev.lock),f1);
    list_for_each_entry(dev,&(guest->hostdev.devs), node) {
	if (!strncasecmp(url,dev->url,MAX_URL)) { 
	    // found it
	    spin_unlock_irqrestore(&(guest->hostdev.lock),f1);
	    printk("palacios: a host device with url \"%s\" already exists in the guest!\n",url);
	    return NULL;
	}
    }
    spin_unlock_irqrestore(&(guest->hostdev.lock),f1);


    printk("palacios: creating host device \"%s\"\n",url);

    dev = kmalloc(sizeof(struct palacios_host_device_user),GFP_KERNEL);
    
    if (!dev) { 
	printk("palacios: cannot allocate for host device \"%s\"\n",url);
	return NULL;
    }

    memset(dev,0,sizeof(struct palacios_host_device_user));
    
    strncpy(dev->url,url,MAX_URL);
    
    dev->guestdev=gdev;
    
    dev->guest=guest;

    spin_lock_init(&(dev->lock));

    init_waitqueue_head(&(dev->user_wait_queue));
    init_waitqueue_head(&(dev->host_wait_queue));

    printk("palacios: attempting to rendezvous with user side of host device \"%s\"\n",url);
    
    // Insert ourselves into the list
    spin_lock_irqsave(&(guest->hostdev.lock),f1);
    list_add(&(dev->node),&(guest->hostdev.devs));
    spin_unlock_irqrestore(&(guest->hostdev.lock),f1);

    
    // Now wait until we are noticed!
    for (i=0;i<RENDEZVOUS_WAIT_SECS/RENDEZVOUS_RETRY_SECS;i++) { 
	spin_lock_irqsave(&(dev->lock),f2);
	if (dev->connected){ 
	    printk("palacios: connection with user side established for host device \"%s\" fd=%d\n",dev->url,dev->fd);
	    spin_unlock_irqrestore(&(dev->lock),f2);
	    return dev;
	}
	spin_unlock_irqrestore(&(dev->lock),f2);
	ssleep(RENDEZVOUS_RETRY_SECS);
    }
    
    printk("palacios: timeout waiting for user side to connect to host device \"%s\"",url);
    
    // get us out of the list
    spin_lock_irqsave(&(guest->hostdev.lock),f1);
    list_del(&(dev->node));
    spin_unlock_irqrestore(&(guest->hostdev.lock),f1);
    
    palacios_host_dev_user_free(dev);
    
    return NULL;
}

static int palacios_host_dev_close(v3_host_dev_t hostdev)
{
    unsigned long f1, f2;

    struct palacios_host_device_user *dev = (struct palacios_host_device_user *) hostdev;
    
    printk("palacios: closing host device \"%s\"\n",dev->url);

    spin_lock_irqsave(&(dev->guest->hostdev.lock),f1);

    spin_lock_irqsave(&(dev->lock),f2);

    if (dev->connected) { 
	dev->connected=0;
	// After this, any user side request will return -EFAULT
    }

    list_del(&(dev->node));
    
    spin_unlock_irqrestore(&(dev->lock),f2);
    spin_unlock_irqrestore(&(dev->guest->hostdev.lock),f1);
    
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

    spin_lock_irqsave(&(dev->lock),f);
    
    if (dev->waiting) { 
	printk("palacios: guest issued i/o read request with host device \"%s\" in wrong state (waiting=%d, connected=%d)\n",dev->url,dev->waiting,dev->connected);
	spin_unlock_irqrestore(&(dev->lock),f);
	return 0;
    }

    if (!dev->connected) {
	printk("palacios: ignoring request as user side is not connected for host device \"%s\"\n",dev->url);
	spin_unlock_irqrestore(&(dev->lock),f);
	return 0;
    }
    
    // resize request and response in case they will need it
    palacios_resize_reqresp(&(dev->req),0,0); // request doesn't carry data

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

    spin_lock_irqsave(&(dev->lock),f);
    
    if (dev->waiting) { 
	printk("palacios: guest issued memory read request with host device \"%s\" in wrong state (waiting=%d, connected=%d)\n",dev->url,dev->waiting,dev->connected);
	spin_unlock_irqrestore(&(dev->lock),f);
	return 0;
    }
    if (!dev->connected) {
	printk("palacios: ignoring request as user side is not connected for host device \"%s\"\n",dev->url);
	spin_unlock_irqrestore(&(dev->lock),f);
	return 0;
    }
    
    // resize request and response in case they will need it
    palacios_resize_reqresp(&(dev->req),0,0); // request doesn't carry data

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

    spin_lock_irqsave(&(dev->lock),f);
    
    if (dev->waiting) { 
	printk("palacios: guest issued config read request with host device \"%s\" in wrong state (waiting=%d, connected=%d)\n",dev->url,dev->waiting,dev->connected);
	spin_unlock_irqrestore(&(dev->lock),f);
	return 0;
    }
    if (!dev->connected) {
	printk("palacios: ignoring request as user side is not connected for host device \"%s\"\n",dev->url);
	spin_unlock_irqrestore(&(dev->lock),f);
	return 0;
    }
    
    // resize request and response in case they will need it
    palacios_resize_reqresp(&(dev->req),0,0); // request doesn't carry data

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

    spin_lock_irqsave(&(dev->lock),f);
    
    if (dev->waiting) { 
	printk("palacios: guest issued i/o write request with host device \"%s\" in wrong state (waiting=%d, connected=%d)\n",dev->url,dev->waiting,dev->connected);
	spin_unlock_irqrestore(&(dev->lock),f);
	return 0;
    }
    if (!dev->connected) {
	printk("palacios: ignoring request as user side is not connected for host device \"%s\"\n",dev->url);
	spin_unlock_irqrestore(&(dev->lock),f);
	return 0;
    }
    
    // resize request and response in case they will need it
    palacios_resize_reqresp(&(dev->req),len,0); // make room for data

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

    spin_lock_irqsave(&(dev->lock),f);
    
    if (dev->waiting) { 
	printk("palacios: guest issued memory write request with host device \"%s\" in wrong state (waiting=%d, connected=%d)\n",dev->url,dev->waiting,dev->connected);
	spin_unlock_irqrestore(&(dev->lock),f);
	return 0;
    }
    if (!dev->connected) {
	printk("palacios: ignoring request as user side is not connected for host device \"%s\"\n",dev->url);
	spin_unlock_irqrestore(&(dev->lock),f);
	return 0;
    }
    
    // resize request and response in case they will need it
    palacios_resize_reqresp(&(dev->req),len,0); // make room for data

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


static int palacios_host_dev_ack_irq(v3_host_dev_t hostdev, uint8_t irq)
{
    // we don't care
    return 0;
}
 


static uint64_t palacios_host_dev_write_conf(v3_host_dev_t hostdev,
					     uint64_t      offset,
					     void          *src,
					     uint64_t      len)
{
    struct palacios_host_device_user *dev = (struct palacios_host_device_user *)hostdev;
    unsigned long f;
    uint64_t op_len;

    spin_lock_irqsave(&(dev->lock),f);
    
    if (dev->waiting) { 
	printk("palacios: guest issued config write request with host device \"%s\" in wrong state (waiting=%d, connected=%d)\n",dev->url,dev->waiting,dev->connected);
	spin_unlock_irqrestore(&(dev->lock),f);
	return 0;
    }
    if (!dev->connected) {
	printk("palacios: ignoring request as user side is not connected for host device \"%s\"\n",dev->url);
	spin_unlock_irqrestore(&(dev->lock),f);
	return 0;
    }
    
    // resize request and response in case they will need it
    palacios_resize_reqresp(&(dev->req),len,0); // make room for data

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
 
 




static struct v3_host_dev_hooks palacios_host_dev_hooks = {
    .open			= palacios_host_dev_open,
    .close                      = palacios_host_dev_close,
    .read_io                    = palacios_host_dev_read_io,
    .write_io                   = palacios_host_dev_write_io,
    .read_mem                   = palacios_host_dev_read_mem,
    .write_mem                  = palacios_host_dev_write_mem,
    .read_config                = palacios_host_dev_read_conf,
    .write_config               = palacios_host_dev_write_conf,
    .ack_irq                    = palacios_host_dev_ack_irq,
};



int palacios_init_host_dev( void ) {
    V3_Init_Host_Device_Support(&palacios_host_dev_hooks);
    
    return 0;
}
