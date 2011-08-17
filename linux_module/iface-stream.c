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


// This is probably overkill
#define STREAM_RING_LEN 4096




static struct list_head global_streams;



struct stream_state {
    char name[STREAM_NAME_LEN];

    struct ringbuf * out_ring;

    int connected;

    wait_queue_head_t intr_queue;
    spinlock_t lock;

    struct v3_guest * guest;
    struct list_head stream_node;

    struct v3_stream * v3_stream;
};


// Currently just the list of open streams
struct vm_global_streams {
    struct list_head open_streams;
};





static struct stream_state * find_stream_by_name(struct v3_guest * guest, const char * name) {
    struct stream_state * stream = NULL;
    struct list_head * stream_list = NULL;
    struct vm_global_streams * vm_state = NULL;

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



#define TMP_BUF_LEN 128

static ssize_t stream_read(struct file * filp, char __user * buf, size_t size, loff_t * offset) {
    struct stream_state * stream = filp->private_data;
    ssize_t bytes_read = 0;
    ssize_t bytes_left = size;
    unsigned long flags;
    char tmp_buf[TMP_BUF_LEN];
    ssize_t total_bytes_left = 0;

    // memset(tmp_buf, 0, TMP_BUF_LEN);

    while (bytes_left > 0) {
	int tmp_len = (TMP_BUF_LEN > bytes_left) ? bytes_left : TMP_BUF_LEN;
	int tmp_read = 0;

	spin_lock_irqsave(&(stream->lock), flags);
	tmp_read = ringbuf_read(stream->out_ring, tmp_buf, tmp_len);
	spin_unlock_irqrestore(&(stream->lock), flags);

	if (tmp_read == 0) {
	    // If userspace reads more than we have
	    break;
	}

	if (copy_to_user(buf + bytes_read, tmp_buf, tmp_read)) {
	    printk("Read Fault\n");
	    return -EFAULT;
	}
	
	bytes_left -= tmp_read;
	bytes_read += tmp_read;
    }
    

    spin_lock_irqsave(&(stream->lock), flags); 
    total_bytes_left = ringbuf_data_len(stream->out_ring);
    spin_unlock_irqrestore(&(stream->lock), flags);

    if (total_bytes_left > 0) {
	wake_up_interruptible(&(stream->intr_queue));
    }

    return bytes_read;
}

static unsigned int 
stream_poll(struct file * filp, struct poll_table_struct * poll_tb) {
    struct stream_state * stream = filp->private_data;
    unsigned int mask = POLLIN | POLLRDNORM;
    unsigned long flags;
    int data_avail = 0;

    poll_wait(filp, &(stream->intr_queue), poll_tb);

    spin_lock_irqsave(&(stream->lock), flags);
    data_avail = ringbuf_data_len(stream->out_ring);
    spin_unlock_irqrestore(&(stream->lock), flags);

    if (data_avail > 0) {
	return mask;
    }

    return 0;

}

static ssize_t stream_write(struct file * filp, const char __user * buf, size_t size, loff_t * offset) {
    struct stream_state * stream = filp->private_data;
    char * kern_buf = NULL;
    ssize_t bytes_written = 0;
    
    kern_buf = kmalloc(size, GFP_KERNEL);

    if (copy_from_user(kern_buf, buf, size)) {
	printk("Stream Write Failed\n");
	return -EFAULT;
    };
    
    bytes_written = stream->v3_stream->input(stream->v3_stream, kern_buf, size);

    kfree(kern_buf);

    return bytes_written;
}


static int stream_release(struct inode * i, struct file * filp) {
    struct stream_state * stream = filp->private_data;
    unsigned long flags;
    
    spin_lock_irqsave(&(stream->lock), flags);
    stream->connected = 0;
    spin_unlock_irqrestore(&(stream->lock), flags);

    
    return 0;

}

static struct file_operations stream_fops = {
    .read = stream_read,
    .write = stream_write,
    .release = stream_release,
    .poll = stream_poll,
};



static void * palacios_stream_open(struct v3_stream * v3_stream, const char * name, void * private_data) {
    struct v3_guest * guest = (struct v3_guest *)private_data;
    struct stream_state * stream = NULL;
    struct vm_global_streams * vm_state = NULL;

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

    stream = kmalloc(sizeof(struct stream_state), GFP_KERNEL);
    memset(stream, 0, sizeof(struct stream_state));

    stream->out_ring = create_ringbuf(STREAM_RING_LEN);
    stream->v3_stream = v3_stream;
    stream->guest = guest;
    stream->connected = 0;

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


static uint64_t palacios_stream_output(struct v3_stream * v3_stream, char * buf, int len) {
    struct stream_state * stream = (struct stream_state *)v3_stream->host_stream_data;
    int bytes_written = 0;
    unsigned long flags;
    

    if (stream->connected == 0) {
	return 0;
    }

    while (bytes_written < len) {
	spin_lock_irqsave(&(stream->lock), flags);
	bytes_written += ringbuf_write(stream->out_ring, buf + bytes_written, len - bytes_written);
	spin_unlock_irqrestore(&(stream->lock), flags);

	wake_up_interruptible(&(stream->intr_queue));

	if (bytes_written < len) {
	    // not enough space in ringbuffer, activate user space to drain it
	    schedule();
	}
    }

    
    return bytes_written;
}


static void palacios_stream_close(struct v3_stream * v3_stream) {
    struct stream_state * stream = (struct stream_state *)v3_stream->host_stream_data;

    free_ringbuf(stream->out_ring);
    list_del(&(stream->stream_node));
    kfree(stream);

}

static struct v3_stream_hooks palacios_stream_hooks = {
    .open = palacios_stream_open,
    .output = palacios_stream_output,
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
    struct stream_state * stream = NULL;
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

    
    stream_fd = anon_inode_getfd("v3-stream", &stream_fops, stream, O_RDWR);

    if (stream_fd < 0) {
	printk("Error creating stream inode for (%s)\n", name);
	return stream_fd;
    }

    printk("Stream (%s) connected\n", name);

    return stream_fd;
}


static int guest_stream_init(struct v3_guest * guest, void ** vm_data) {
    struct vm_global_streams * state = kmalloc(sizeof(struct vm_global_streams), GFP_KERNEL);

    INIT_LIST_HEAD(&(state->open_streams));
    *vm_data = state;

    add_guest_ctrl(guest, V3_VM_STREAM_CONNECT, stream_connect, state);

    return 0;
}


static int guest_stream_deinit(struct v3_guest * guest, void * vm_data) {
    struct vm_global_streams * state = vm_data;
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
