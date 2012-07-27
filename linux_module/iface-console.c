/* 
 * VM Console 
 * (c) Jack Lange, 2010
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

#include <interfaces/vmm_console.h>
#include <palacios/vmm_host_events.h>

#include "vm.h"
#include "palacios.h"
#include "util-queue.h"
#include "linux-exts.h"

typedef enum { CONSOLE_CURS_SET = 1,
	       CONSOLE_CHAR_SET = 2,
	       CONSOLE_SCROLL = 3,
	       CONSOLE_UPDATE = 4,
               CONSOLE_RESOLUTION = 5} console_op_t;



struct palacios_console {
    struct gen_queue * queue;
    spinlock_t lock;

    int open;
    int connected;

    wait_queue_head_t intr_queue;

    unsigned int width;
    unsigned int height;

    struct v3_guest * guest;
};



struct cursor_msg {
    int x;
    int y;
} __attribute__((packed));

struct character_msg {
    int x;
    int y;
    char c;
    unsigned char style;
} __attribute__((packed));

struct scroll_msg {
    int lines;
} __attribute__((packed));


struct resolution_msg {
    int cols;
    int rows;
} __attribute__((packed));

struct cons_msg {
    unsigned char op;
    union {
	struct cursor_msg cursor;
	struct character_msg  character;
	struct scroll_msg scroll;
	struct resolution_msg resolution;
    };
} __attribute__((packed)); 


/* This is overkill...*/
#define CONSOLE_QUEUE_LEN 8096


static ssize_t 
console_read(struct file * filp, char __user * buf, size_t size, loff_t * offset) {
    struct palacios_console * cons = filp->private_data;
    struct cons_msg * msg = NULL;
    unsigned long flags;
    int entries = 0;

    if (cons->open == 0) {
	return 0;
    }


    if (size < sizeof(struct cons_msg)) {
	ERROR("Invalid Read operation size: %lu\n", size);
	return -EFAULT;
    }
    
    msg = dequeue(cons->queue);
    
    if (msg == NULL) {
	ERROR("ERROR: Null console message\n");
	return -EFAULT;
    }
    
    if (copy_to_user(buf, msg, size)) {
	ERROR("Read Fault\n");
	return -EFAULT;
    }


    palacios_free(msg);

    spin_lock_irqsave(&(cons->queue->lock), flags);
    entries =  cons->queue->num_entries;
    spin_unlock_irqrestore(&(cons->queue->lock), flags);
    
    if (entries > 0) {
	wake_up_interruptible(&(cons->intr_queue));
    }

    //    DEBUG("Read from console\n");
    return size;
}


static ssize_t 
console_write(struct file * filp, const char __user * buf, size_t size, loff_t * offset) {
    struct palacios_console * cons = filp->private_data;
    int i = 0;
    struct v3_keyboard_event event = {0, 0};
    
    if (cons->open == 0) {
	return 0;
    }


    for (i = 0; i < size; i++) {
	if (copy_from_user(&(event.scan_code), buf + i, 1)) {
	    ERROR("Console Write fault\n");
	    return -EFAULT;
	}

	v3_deliver_keyboard_event(cons->guest->v3_ctx, &event);
    }
    
    return size;
}

static unsigned int 
console_poll(struct file * filp, struct poll_table_struct * poll_tb) {
    struct palacios_console * cons = filp->private_data;
    unsigned int mask = POLLIN | POLLRDNORM;
    unsigned long flags;
    int entries = 0;

    //    DEBUG("Console=%p (guest=%s)\n", cons, cons->guest->name);


    poll_wait(filp, &(cons->intr_queue), poll_tb);

    spin_lock_irqsave(&(cons->queue->lock), flags);
    entries = cons->queue->num_entries;
    spin_unlock_irqrestore(&(cons->queue->lock), flags);

    if (entries > 0) {
	//	DEBUG("Returning from POLL\n");
	return mask;
    }
    
    return 0;
}


static int console_release(struct inode * i, struct file * filp) {
    struct palacios_console * cons = filp->private_data;
    struct cons_msg * msg = NULL;
    unsigned long flags;

    DEBUG("Releasing the Console File desc\n");
    
    spin_lock_irqsave(&(cons->queue->lock), flags);
    cons->connected = 0;
    spin_unlock_irqrestore(&(cons->queue->lock), flags);

    while ((msg = dequeue(cons->queue))) {
	palacios_free(msg);
    }

    return 0;
}


static struct file_operations cons_fops = {
    .read     = console_read,
    .write    = console_write,
    .poll     = console_poll,
    .release  = console_release,
};



static int console_connect(struct v3_guest * guest, unsigned int cmd, 
			   unsigned long arg, void * priv_data) {
    struct palacios_console * cons = priv_data;
    int cons_fd = 0;
    unsigned long flags;
    int acquired = 0;

    if (cons->open == 0) {
	ERROR("Attempted to connect to unopened console\n");
	return -1;
    }

    spin_lock_irqsave(&(cons->lock), flags);
    if (cons->connected == 0) {
	cons->connected = 1;
	acquired = 1;
    }
    spin_unlock_irqrestore(&(cons->lock), flags);

    if (acquired == 0) {
	ERROR("Console already connected\n");
	return -1;
    }

    cons_fd = anon_inode_getfd("v3-cons", &cons_fops, cons, O_RDWR);

    if (cons_fd < 0) {
	ERROR("Error creating console inode\n");
	return cons_fd;
    }

    v3_deliver_console_event(guest->v3_ctx, NULL);


    INFO("Console connected\n");

    return cons_fd;
}



static void * palacios_tty_open(void * private_data, unsigned int width, unsigned int height) {
    struct v3_guest * guest = (struct v3_guest *)private_data;
    struct palacios_console * cons = palacios_alloc(sizeof(struct palacios_console));

    if (!cons) { 
	ERROR("Cannot allocate memory for console\n");
	return NULL;
    }

    INFO("Guest initialized virtual console (Guest=%s)\n", guest->name);

    if (guest == NULL) {
	ERROR("ERROR: Cannot open a console on a NULL guest\n");
	palacios_free(cons);
	return NULL;
    }

    if (cons->open == 1) {
	ERROR("Console already open\n");
	palacios_free(cons);
	return NULL;
    }


    cons->queue = create_queue(CONSOLE_QUEUE_LEN);
    spin_lock_init(&(cons->lock));
    init_waitqueue_head(&(cons->intr_queue));

    cons->guest = guest;

    cons->connected = 0;
    cons->width = width;
    cons->height = height;
    cons->open = 1;


    add_guest_ctrl(guest, V3_VM_CONSOLE_CONNECT, console_connect, cons);

    return cons;
}

static int post_msg(struct palacios_console * cons, struct cons_msg * msg) {
    //    DEBUG("Posting Console message\n");

    while (enqueue(cons->queue, msg) == -1) {	
	wake_up_interruptible(&(cons->intr_queue));
	schedule();
    }

    wake_up_interruptible(&(cons->intr_queue));

    return 0;
}


static int palacios_tty_cursor_set(void * console, int x, int y) {
    struct palacios_console * cons = (struct palacios_console *)console;
    struct cons_msg * msg = NULL;

    if (cons->connected == 0) {
	return 0;
    }

    msg = palacios_alloc(sizeof(struct cons_msg));

    if (!msg) { 
	ERROR("Cannot allocate cursor set message in console\n");
	return -1;
    }

    msg->op = CONSOLE_CURS_SET;
    msg->cursor.x = x;
    msg->cursor.y = y;

    return post_msg(cons, msg);
}

static int palacios_tty_character_set(void * console, int x, int y, char c, unsigned char style) {
    struct palacios_console * cons = (struct palacios_console *) console;
    struct cons_msg * msg = NULL;

    if (cons->connected == 0) {
	return 0;
    }

    msg = palacios_alloc(sizeof(struct cons_msg));

    if (!msg) { 
	ERROR("Cannot allocate character set message in console\n");
	return -1;
    }

    msg->op = CONSOLE_CHAR_SET;
    msg->character.x = x;
    msg->character.y = y;
    msg->character.c = c;
    msg->character.style = style;

    return post_msg(cons, msg);
}

static int palacios_tty_scroll(void * console, int lines) {
    struct palacios_console * cons = (struct palacios_console *) console;
    struct cons_msg * msg = NULL;


    if (cons->connected == 0) {
	return 0;
    }

    msg = palacios_alloc(sizeof(struct cons_msg));

    if (!msg) { 
	ERROR("Cannot allocate scroll message in console\n");
	return -1;
    }

    msg->op = CONSOLE_SCROLL;
    msg->scroll.lines = lines;

    return post_msg(cons, msg);
}

static int palacios_set_text_resolution(void * console, int cols, int rows) {
    struct palacios_console * cons = (struct palacios_console *)console;
    struct cons_msg * msg = NULL;

    if (cons->connected == 0) {
	return 0;
    }

    msg = palacios_alloc(sizeof(struct cons_msg));

    if (!msg) { 
	ERROR("Cannot allocate text resolution message in console\n");
	return -1;
    }
    
    msg->op = CONSOLE_RESOLUTION;
    msg->resolution.cols = cols;
    msg->resolution.rows = rows;

    return post_msg(cons, msg);
}

static int palacios_tty_update(void * console) {
    struct palacios_console * cons = (struct palacios_console *) console;
    struct cons_msg * msg = NULL;

    if (cons->connected == 0) {
	return 0;
    }

    msg = palacios_alloc(sizeof(struct cons_msg));

    if (!msg) { 
	ERROR("Cannot allocate update message in console\n");
	return -1;
    }

    msg->op = CONSOLE_UPDATE;

    return post_msg(cons, msg);
}

static void palacios_tty_close(void * console) {
    struct palacios_console * cons = (struct palacios_console *) console;

    cons->open = 0;

    remove_guest_ctrl(cons->guest, V3_VM_CONSOLE_CONNECT);
    deinit_queue(cons->queue);
       
    kfree(cons);
}



static struct v3_console_hooks palacios_console_hooks = {
    .open			= palacios_tty_open,
    .set_cursor	                = palacios_tty_cursor_set,
    .set_character	        = palacios_tty_character_set,
    .scroll			= palacios_tty_scroll,
    .set_text_resolution        = palacios_set_text_resolution,
    .update			= palacios_tty_update,
    .close                      = palacios_tty_close,
};






static int console_init( void ) {
    V3_Init_Console(&palacios_console_hooks);
    
    return 0;
}




static struct linux_ext console_ext = {
    .name = "CONSOLE",
    .init = console_init,
    .deinit = NULL,
    .guest_init = NULL,
    .guest_deinit = NULL
};


register_extension(&console_ext);
