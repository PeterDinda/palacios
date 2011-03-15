/* 
 * VM Serial Controls
 * (c) Lei Xia, 2010
 */

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>

#include <palacios/vmm.h>
#include <palacios/vmm_host_events.h>

#include "palacios.h"
#include "palacios-stream.h"


void
send_serial_input_to_palacios( unsigned char *input,
			       unsigned int len,
			       struct v3_vm_info * vm ) {
    struct v3_serial_event event;

    if (len > 128) {
	len = 128;
    }

    memcpy(event.data, input, len);
    event.len = len;
	
    v3_deliver_serial_event(vm, &event);
}

static ssize_t 
serial_read(struct file * filp, char __user * buf, size_t size, loff_t * offset) {

    int len = 0;
    char temp[128];
    struct stream_buffer * stream = filp->private_data;
	
    memset(temp, 0, 128);

    if (size > 128) {
	size = 128;
    }
	
    len =  stream_dequeue(stream, temp, size);
	
    if (copy_to_user(buf, temp, len)) {
	printk("Read fault\n");
	return -EFAULT;
    }

    printk("Returning %d bytes\n", len);

    return len;
}

static ssize_t 
serial_write(struct file * filp, const char __user * buf, size_t size, loff_t * offset) {
    char temp[128];
    struct stream_buffer * stream = filp->private_data;
    struct v3_vm_info * vm;

    memset(temp, 0, 128);

    if (size > 128) {
	size = 128;
    }

    if (copy_from_user(temp, buf, size)) {
	printk("Write fault\n");
	return -EFAULT;
    }

    vm = stream->guest->v3_ctx;
    send_serial_input_to_palacios(temp, size, vm);
   
    return size;
}


static unsigned int 
serial_poll(struct file * filp, struct poll_table_struct * poll_tb) {
    unsigned int mask = 0;
    struct stream_buffer *stream = filp->private_data;
  
    poll_wait(filp, &(stream->intr_queue), poll_tb);

    if(stream_datalen(stream) > 0){
	mask = POLLIN | POLLRDNORM;
    }
	
    printk("polling v3 serial\n");

    return mask;
}

static struct file_operations v3_cons_fops = {
    .read     = serial_read,
    .write    = serial_write,
    .poll     = serial_poll,
};


int open_serial(char * name) {
    int cons_fd;
    void *stream;

    printk("open path: %s\n", name);

    stream = find_stream_by_name(NULL, name);

    if (stream == NULL) {
	return -1;
    }

    cons_fd = anon_inode_getfd("v3-cons", &v3_cons_fops, stream, 0);
    
    if (cons_fd < 0) {
	printk("Error creating serial inode\n");
	return cons_fd;
    }

    printk("Returning new serial fd\n");
	    
    return cons_fd;
}
