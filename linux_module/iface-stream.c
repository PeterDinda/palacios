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


#define sint64_t int64_t
#include <interfaces/vmm_stream.h>
#include "linux-exts.h"
#include "util-ringbuffer.h"
#include "vm.h"



// This is probably overkill
#define STREAM_RING_LEN 4096
#define STREAM_NAME_LEN 128



static struct list_head global_streams;



struct stream_state {
    char name[STREAM_NAME_LEN];

    struct ringbuf * out_ring;

    int connected;

    wait_queue_head_t user_poll_queue;

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
	    ERROR("ERROR: Could not locate vm stream state for extension STREAM_INTERFACE\n");
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


// host->user nonblocking data flow 
static ssize_t stream_read(struct file * filp, char __user * buf, size_t size, loff_t * offset) {
  struct stream_state * stream = filp->private_data;
  ssize_t bytes_read = 0;
  unsigned long flags;
  char *kern_buf;
  
  
  kern_buf = palacios_alloc(size);

  if (!kern_buf) {
    ERROR("Cannot allocate space for buffer\n");
    return -1;
  }
  
  palacios_spinlock_lock_irqsave(&(stream->lock), flags);
  bytes_read = ringbuf_read(stream->out_ring,kern_buf,size);
  palacios_spinlock_unlock_irqrestore(&(stream->lock), flags);
    
  if (bytes_read>0) { 
    if (copy_to_user(buf, kern_buf, bytes_read)) {
      ERROR("Read Fault\n");
      palacios_free(kern_buf);
      return -EFAULT;
    } else {
      palacios_free(kern_buf);
      return bytes_read;
    }
  } else if (bytes_read==0) { 
    // out of space
    palacios_free(kern_buf);
    return -EWOULDBLOCK;
  } else { // bytes_read<0
    ERROR("Read failed\n");
    palacios_free(kern_buf);
    return -EFAULT;
  }
}
  

static unsigned int 
stream_poll(struct file * filp, struct poll_table_struct * poll_tb) {
    struct stream_state * stream = filp->private_data;
    unsigned long flags;
    int data_avail = 0;

    if (!stream) { 
      return POLLERR;
    }

    poll_wait(filp, &(stream->user_poll_queue), poll_tb);

    palacios_spinlock_lock_irqsave(&(stream->lock), flags);
    data_avail = ringbuf_data_len(stream->out_ring);
    palacios_spinlock_unlock_irqrestore(&(stream->lock), flags);

    if (data_avail > 0) {
	return POLLIN | POLLRDNORM;
    }

    return 0;

}

//
// Non-blocking user->Host->VM data flow
//
static ssize_t stream_write(struct file * filp, const char __user * buf, size_t size, loff_t * offset) {
    struct stream_state * stream = filp->private_data;
    char * kern_buf = NULL;
    ssize_t bytes_written = 0;
    
    kern_buf = palacios_alloc(size);
    
    if (!kern_buf) { 
	ERROR("Cannot allocate buffer in stream interface\n");
	return -EFAULT;
    }

    if (copy_from_user(kern_buf, buf, size)) {
	ERROR("Stream Write Failed\n");
	palacios_free(kern_buf);
	return -EFAULT;
    };
    
    bytes_written = stream->v3_stream->input(stream->v3_stream, kern_buf, size);

    // could end up with zero here
    if (bytes_written<0) { 
      ERROR("Error on writing to stream\n");
      palacios_free(kern_buf);
      return -EFAULT;
    }

    if (bytes_written==0) { 
      // This is somewhat bogus, since 
      // the FD is treated as non-blocking regardless
      palacios_free(kern_buf);
      return -EWOULDBLOCK;
    } 

    palacios_free(kern_buf);
    return bytes_written;
}


static int stream_release(struct inode * i, struct file * filp) {
    struct stream_state * stream = filp->private_data;
    unsigned long flags;
    
    palacios_spinlock_lock_irqsave(&(stream->lock), flags);
    stream->connected = 0;
    palacios_spinlock_unlock_irqrestore(&(stream->lock), flags);

    
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
	    ERROR("ERROR: Could not locate vm stream state for extension STREAM_INTERFACE\n");
	    return NULL;
	}
    }

    if (find_stream_by_name(guest, name) != NULL) {
	ERROR("Stream already exists\n");
	return NULL;
    }

    stream = palacios_alloc(sizeof(struct stream_state));
    if (!stream) { 
	ERROR("Unable to allocate stream\n");
	return NULL;
    }
    memset(stream, 0, sizeof(struct stream_state));

    stream->out_ring = create_ringbuf(STREAM_RING_LEN);
    stream->v3_stream = v3_stream;
    stream->guest = guest;
    stream->connected = 0;

    strncpy(stream->name, name, STREAM_NAME_LEN - 1);

    init_waitqueue_head(&(stream->user_poll_queue));
    palacios_spinlock_init(&(stream->lock));

    if (guest == NULL) {
	list_add(&(stream->stream_node), &(global_streams));
    } else {
	list_add(&(stream->stream_node), &(vm_state->open_streams));
    } 

    return stream;
}


//
// Non-blocking VM->host data flow
//
static sint64_t palacios_stream_output(struct v3_stream * v3_stream, uint8_t * buf, sint64_t len) {
    struct stream_state * stream = (struct stream_state *)v3_stream->host_stream_data;
    sint64_t bytes_written = 0;
    unsigned long flags;
    


    palacios_spinlock_lock_irqsave(&(stream->lock), flags);
    bytes_written=ringbuf_write(stream->out_ring, buf, len);
    palacios_spinlock_unlock_irqrestore(&(stream->lock), flags);

    if (bytes_written<0) { 
      // we ended in an error, so push back to VM
      return bytes_written;
    } else {
      // normal situation, tell it how much we handled
      wake_up_interruptible(&(stream->user_poll_queue));
      return bytes_written;
    } 

}


static void palacios_stream_close(struct v3_stream * v3_stream) {
    struct stream_state * stream = (struct stream_state *)v3_stream->host_stream_data;

    free_ringbuf(stream->out_ring);
    list_del(&(stream->stream_node));
    palacios_spinlock_deinit(&(stream->lock));
    palacios_free(stream);

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
    struct stream_state * stream = NULL;
    struct stream_state * tmp = NULL;

    list_for_each_entry_safe(stream, tmp, &(global_streams), stream_node) {
        free_ringbuf(stream->out_ring);
        list_del(&(stream->stream_node));
        palacios_free(stream);
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
	ERROR("%s(%d): copy from user error...\n", __FILE__, __LINE__);
	return -EFAULT;
    }

    stream = find_stream_by_name(guest, name);

    if (stream == NULL) {
	ERROR("Could not find stream (%s)\n", name);
	return -EFAULT;
    }

    palacios_spinlock_lock_irqsave(&(stream->lock), flags);
    if (stream->connected == 0) {
	stream->connected = 1;
	ret = 1;
    }
    palacios_spinlock_unlock_irqrestore(&(stream->lock), flags);


    if (ret == -1) {
	ERROR("Stream (%s) already connected\n", name);
	return -EFAULT;
    }

    
    stream_fd = anon_inode_getfd("v3-stream", &stream_fops, stream, O_RDWR);

    if (stream_fd < 0) {
	ERROR("Error creating stream inode for (%s)\n", name);
	return stream_fd;
    }

    INFO("Stream (%s) connected\n", name);

    return stream_fd;
}


static int guest_stream_init(struct v3_guest * guest, void ** vm_data) {
    struct vm_global_streams * state = palacios_alloc(sizeof(struct vm_global_streams));

    if (!state) { 
	ERROR("Unable to allocate state in stream init\n");
	return -1;
    }

    INIT_LIST_HEAD(&(state->open_streams));
    *vm_data = state;

    add_guest_ctrl(guest, V3_VM_STREAM_CONNECT, stream_connect, state);

    return 0;
}


static int guest_stream_deinit(struct v3_guest * guest, void * vm_data) {
    struct vm_global_streams * state = vm_data;

    struct stream_state * stream = NULL;
    struct stream_state * tmp = NULL;


    remove_guest_ctrl(guest, V3_VM_STREAM_CONNECT);

    list_for_each_entry_safe(stream, tmp, &(global_streams), stream_node) {
        free_ringbuf(stream->out_ring);
        list_del(&(stream->stream_node));
        palacios_free(stream);
    }
    
    palacios_free(state);

    
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
