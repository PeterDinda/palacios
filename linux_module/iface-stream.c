/*
 * Stream Implementation
 * (c) Lei Xia  2010
 */
 
 
#include <linux/errno.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>


#include <interfaces/vmm_stream.h>
#include "linux-exts.h"
#include "util-ringbuffer.h"
#include "vm.h"
#include "iface-stream.h"


// This is going to need to be a lot bigger...
#define STREAM_BUF_SIZE 1024




static struct list_head global_streams;

struct stream_buffer {
    char name[STREAM_NAME_LEN];
    struct ringbuf * buf;

    int connected;

    wait_queue_head_t intr_queue;
    spinlock_t lock;

    struct v3_guest * guest;
    struct list_head stream_node;
};


// Currently just the list of open streams
struct vm_stream_state {
    struct list_head open_streams;
};


static struct stream_buffer * find_stream_by_name(struct v3_guest * guest, const char * name) {
    struct stream_buffer * stream = NULL;
    struct list_head * stream_list = NULL;
    struct vm_stream_state * vm_state = NULL;

    if (guest == NULL) {
	stream_list = &global_streams;
    } else {
	vm_state = get_vm_ext_data(guest, "STREAM_INTERFACE");

	if (vm_state == NULL) {
	    printk("ERROR: Could not locate vm stream state for extension STREAM_INTERFACE\n");
	    return NULL;
	}

	stream_list = &(vm_state->open_streams);
    }

    list_for_each_entry(stream,  stream_list, stream_node) {
	if (strncmp(stream->name, name, STREAM_NAME_LEN) == 0) {
	    return stream;
	}
    }

    return NULL;
}



static ssize_t stream_read(struct file * filp, char __user * buf, size_t size, loff_t * offset) {
    struct stream_buffer * stream = filp->private_data;
    
    wait_event_interruptible(stream->intr_queue, (ringbuf_data_len(stream->buf) != 0));

    return ringbuf_read(stream->buf, buf, size);
}



static struct file_operations stream_fops = {
    .read = stream_read,
    //    .release = stream_close,
    //  .poll = stream_poll,
};



static void * palacios_stream_open(const char * name, void * private_data) {
    struct v3_guest * guest = (struct v3_guest *)private_data;
    struct stream_buffer * stream = NULL;
    struct vm_stream_state * vm_state = NULL;

    if (guest != NULL) {
	vm_state = get_vm_ext_data(guest, "STREAM_INTERFACE");

	if (vm_state == NULL) {
	    printk("ERROR: Could not locate vm stream state for extension STREAM_INTERFACE\n");
	    return NULL;
	}
    }

    if (find_stream_by_name(guest, name) != NULL) {
	printk("Stream already exists\n");
	return NULL;
    }

    stream = kmalloc(sizeof(struct stream_buffer), GFP_KERNEL);
	
    stream->buf = create_ringbuf(STREAM_BUF_SIZE);
    stream->guest = guest;

    strncpy(stream->name, name, STREAM_NAME_LEN - 1);

    init_waitqueue_head(&(stream->intr_queue));
    spin_lock_init(&(stream->lock));

    if (guest == NULL) {
	list_add(&(stream->stream_node), &(global_streams));
    } else {
	list_add(&(stream->stream_node), &(vm_state->open_streams));
    } 

    return stream;
}


static int palacios_stream_write(void * stream_ptr, char * buf, int len) {
    struct stream_buffer * stream = (struct stream_buffer *)stream_ptr;
    int ret = 0;

    ret = ringbuf_write(stream->buf, buf, len);

    if (ret > 0) {
	wake_up_interruptible(&(stream->intr_queue));
    }

    return ret;
}


static void palacios_stream_close(void * stream_ptr) {
    struct stream_buffer * stream = (struct stream_buffer *)stream_ptr;

    free_ringbuf(stream->buf);
    list_del(&(stream->stream_node));
    kfree(stream);

}

static struct v3_stream_hooks palacios_stream_hooks = {
    .open = palacios_stream_open,
    .write = palacios_stream_write,
    .close = palacios_stream_close,
};


static int stream_init( void ) {
    INIT_LIST_HEAD(&(global_streams));
    V3_Init_Stream(&palacios_stream_hooks);
    
    return 0;
}


static int stream_deinit( void ) {
    if (!list_empty(&(global_streams))) {
	printk("Error removing module with open streams\n");
	printk("TODO: free old streams... \n");
    }

    return 0;
}





static int stream_connect(struct v3_guest * guest, unsigned int cmd, unsigned long arg, void * priv_data) {
    void __user * argp = (void __user *)arg;
    struct stream_buffer * stream = NULL;
    int stream_fd = 0;
    char name[STREAM_NAME_LEN];
    unsigned long flags = 0;
    int ret = -1;
    
    
    if (copy_from_user(name, argp, STREAM_NAME_LEN)) {
	printk("%s(%d): copy from user error...\n", __FILE__, __LINE__);
	return -EFAULT;
    }

    stream = find_stream_by_name(guest, name);

    if (stream == NULL) {
	printk("Could not find stream (%s)\n", name);
	return -EFAULT;
    }

    spin_lock_irqsave(&(stream->lock), flags);
    if (stream->connected == 0) {
	stream->connected = 1;
	ret = 1;
    }
    spin_unlock_irqrestore(&(stream->lock), flags);


    if (ret == -1) {
	printk("Stream (%s) already connected\n", name);
	return -EFAULT;
    }

    
    stream_fd = anon_inode_getfd("v3-stream", &stream_fops, stream, 0);

    if (stream_fd < 0) {
	printk("Error creating stream inode for (%s)\n", name);
	return stream_fd;
    }

    printk("Stream (%s) connected\n", name);

    return stream_fd;
}


static int guest_stream_init(struct v3_guest * guest, void ** vm_data) {
    struct vm_stream_state * state = kmalloc(sizeof(struct vm_stream_state), GFP_KERNEL);

    INIT_LIST_HEAD(&(state->open_streams));
    *vm_data = state;


    add_guest_ctrl(guest, V3_VM_STREAM_CONNECT, stream_connect, state);

    return 0;
}


static int guest_stream_deinit(struct v3_guest * guest, void * vm_data) {
    struct vm_stream_state * state = vm_data;
    if (!list_empty(&(state->open_streams))) {
	printk("Error shutting down VM with open streams\n");
    }

    return 0;
}



static struct linux_ext stream_ext = {
    .name = "STREAM_INTERFACE",
    .init = stream_init,
    .deinit = stream_deinit,
    .guest_init = guest_stream_init,
    .guest_deinit = guest_stream_deinit
};


register_extension(&stream_ext);
