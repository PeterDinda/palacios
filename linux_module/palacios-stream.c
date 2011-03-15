
/* 
 * VM specific Controls
 * (c) Lei Xia, 2010
 */
#include <linux/errno.h>
#include <linux/percpu.h>
#include <linux/sched.h>

#include <palacios/vmm_stream.h>
#include "palacios-stream.h"
#include "palacios-ringbuf.h"

static struct list_head global_streams;

int stream_enqueue(struct stream_buffer * stream, char * buf, int len) {
    int bytes = 0;

    bytes = ringbuf_write(stream->buf, buf, len);

    return bytes;
}


int stream_dequeue(struct stream_buffer * stream, char * buf, int len) {
    int bytes = 0;

    bytes = ringbuf_read(stream->buf, buf, len);

    return bytes;
}

int stream_datalen(struct stream_buffer * stream){
    return ringbuf_data_len(stream->buf);
}


struct stream_buffer * find_stream_by_name(struct v3_guest * guest, const char * name) {
    struct stream_buffer * stream = NULL;
    struct list_head * stream_list = NULL;

    if (guest == NULL) {
	stream_list = &global_streams;
    } else {
	stream_list = &(guest->streams);
    }

    list_for_each_entry(stream,  stream_list, stream_node) {
	if (strncmp(stream->name, name, STREAM_NAME_LEN) == 0) {
	    return stream;
	}
    }

    return NULL;
}


static void * palacios_stream_open(const char * name, void * private_data) {
    struct v3_guest * guest = (struct v3_guest *)private_data;
    struct stream_buffer * stream = NULL;

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
	list_add(&(stream->stream_node), &(guest->streams));
    } 

    return stream;
}


static int palacios_stream_write(void * stream_ptr, char * buf, int len) {
    struct stream_buffer * stream = (struct stream_buffer *)stream_ptr;
    int ret = 0;

    ret = stream_enqueue(stream, buf, len);

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

struct v3_stream_hooks palacios_stream_hooks = {
    .open = palacios_stream_open,
    .write = palacios_stream_write,
    .close = palacios_stream_close,
};


void palacios_init_stream() {
    INIT_LIST_HEAD(&(global_streams));
    V3_Init_Stream(&palacios_stream_hooks);
}


