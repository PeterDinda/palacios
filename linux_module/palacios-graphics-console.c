/*
 * Palacios VM Graphics Console Interface (shared framebuffer between palacios and host)
 * Copyright (c) 2011 Peter Dinda <pdinda@northwestern.edu>
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

#include "palacios.h"
#include "palacios-graphics-console.h"

#include <linux/vmalloc.h>

/*

  This is an implementation of the Palacios Graphics Console interface that
  is designed to interact with a vnc server running in user space, 
  typically something based on x0vncserver.  

  The basic idea is that we manage a frame buffer that we share with
  palacios.   Palacios draws whatever it likes on it.  
  The user-land vncserver will send us requests   for fb updates, which
  we implement by copying the FB to it.   When the user-land sends us characters, etc,
  we deliver those immediately to palacios via the deliver_key and deliver_mouse
  event interfaces.  The end-result is that whatever the graphics system
  in palacios renders is visible via vnc.

*/

static v3_graphics_console_t g_open(void * priv_data, 
				    struct v3_frame_buffer_spec *desired_spec,
				    struct v3_frame_buffer_spec *actual_spec)
{
    struct v3_guest * guest = (struct v3_guest *)priv_data;
    struct palacios_graphics_console *gc = (struct palacios_graphics_console *) &(guest->graphics_console);
    uint32_t mem;

    if(gc->data) { 
	printk("palacios: framebuffer already allocated - returning it\n");
	*actual_spec=gc->spec;
	gc->cons_refcount++;
	gc->data_refcount++;
	return gc;
    }

    mem = desired_spec->width * desired_spec->height * desired_spec->bytes_per_pixel;

    printk("palacios: allocating %u bytes for %u by %u by %u buffer\n",
	   mem, desired_spec->width, desired_spec->height, desired_spec->bytes_per_pixel);

    gc->data = vmalloc(mem);

    if (!(gc->data)) { 
	printk("palacios: unable to allocate memory for frame buffer\n");
	return 0;
    }

    gc->spec = *desired_spec;

    *actual_spec = gc->spec;

    gc->guest=guest;
    
    gc->cons_refcount++;
    gc->data_refcount++;

    printk("palacios: allocated frame buffer\n");

    return gc;
}

static  void g_close(v3_graphics_console_t cons)
{
    struct palacios_graphics_console *gc = (struct palacios_graphics_console *) cons;

    gc->cons_refcount--;
    gc->data_refcount--;

    if (gc->data_refcount<gc->cons_refcount) { 
	printk("palacios: error!   data refcount is less than console refcount for graphics console\n");
    }

    if (gc->cons_refcount>0) { 
	return;
    } else {
	if (gc->cons_refcount<0) { 
	    printk("palacios: error!  refcount for graphics console is negative on close!\n");
	}
	if (gc->data_refcount>0) { 
	    printk("palacios: error!  refcount for graphics console data is positive on close - LEAKING MEMORY\n");
	    return;
	}
	if (gc->data) { 
	    kfree(gc->data);
	    gc->data=0;
	}
    }
}

static void * g_get_data_read(v3_graphics_console_t cons, 
			      struct v3_frame_buffer_spec *cur_spec)
{
    struct palacios_graphics_console *gc = (struct palacios_graphics_console *) cons;
    
    if (gc->data_refcount<=0) { 
	printk("palacios: error!  data refcount is <= 0 in get_data_read\n");
    }

    gc->data_refcount++;
    
    *cur_spec=gc->spec;
    
    return gc->data;
}

static void g_release_data_read(v3_graphics_console_t cons)
{
    struct palacios_graphics_console *gc = (struct palacios_graphics_console *) cons;
    
    gc->data_refcount--;

    if (gc->data_refcount<=0) { 
	printk("palacios: error!  data refcount is <= zero in release_data_read\n");
    }
    
}

static void *g_get_data_rw(v3_graphics_console_t cons, 
			   struct v3_frame_buffer_spec *cur_spec)
{
    return g_get_data_read(cons,cur_spec);
}


static void g_release_data_rw(v3_graphics_console_t cons)
{
    return g_release_data_read(cons);
}


static int g_changed(v3_graphics_console_t cons)
{

#if 0
    struct palacios_graphics_console *gc = 
	(struct palacios_graphics_console *) cons;

    int rc =  !(gc->num_updates % 1000);
    
    gc->num_updates++;
    
    return rc;
#else
    return 1;
#endif
}



static int palacios_graphics_console_key(struct palacios_graphics_console *cons, uint8_t scancode)
{
    struct v3_keyboard_event e;
    e.status=0;
    e.scan_code=scancode;

    v3_deliver_keyboard_event(cons->guest->v3_ctx,&e);
    
    return 0;
}

static int palacios_graphics_console_mouse(struct palacios_graphics_console *cons, uint8_t x, uint8_t y, uint8_t buttons)
{
    struct v3_mouse_event e;
    e.data[0]=x;
    e.data[1]=y;
    e.data[2]=buttons;   // These three are completely wrong, of course - ignoring mouse for now

    v3_deliver_mouse_event(cons->guest->v3_ctx,&e);

    return 0;
}

static struct v3_graphics_console_hooks palacios_graphics_console_hooks = 
{
    .open  = g_open,
    .close = g_close,

    .get_data_read = g_get_data_read,
    .release_data_read = g_release_data_read,
    .get_data_rw = g_get_data_rw,
    .release_data_rw = g_release_data_rw,

    .changed = g_changed,
};


int palacios_init_graphics_console( void ) {

    V3_Init_Graphics_Console(&palacios_graphics_console_hooks);
    
    return 0;
}


int palacios_graphics_console_user_query(struct palacios_graphics_console *cons, 
					 struct v3_fb_query_response __user *u)
{
    struct v3_fb_query_response q;
    
    
    if (copy_from_user(&q,(void __user *) u, sizeof(struct v3_fb_query_response))) { 
	printk("palacios: copy from user in getting query in fb\n");
	return -EFAULT;
    }
    
    switch (q.request_type) { 
	case V3_FB_SPEC:
	    // returns only the spec for the FB
	    q.spec = cons->spec;

	    break;

	case V3_FB_UPDATE: 
	    // returns whether an update is available for the region
	    // currently always true
	    q.updated = 1;

	    break;

	case V3_FB_DATA_BOX: {
	    // Not curently implemented
	    printk("palacios: request for data in bounding box unsupported currently\n");
	    return -EFAULT;

	}

	    break;
	    
	case V3_FB_DATA_ALL: {
	    // First let's sanity check to see if they are requesting the same
	    // spec that we have
	    if (memcmp(&(q.spec),&(cons->spec),sizeof(struct v3_frame_buffer_spec))) { 
		printk("palacios: request for data with non-matching fb spec \n");
		return -EFAULT;
	    }
	    // Now let's indicate an update is in the pointer and copy across the data
	    if (copy_to_user(q.data,cons->data,cons->spec.width*cons->spec.height*cons->spec.bytes_per_pixel)) { 
		printk("palacios: unable to copy fb content to user\n");
		return -EFAULT;
	    }
	    q.updated=1;
	}
	    break;
	    
	default:
	    return -EFAULT;
    }

    // now we'll copy back any changes we made to the query/response structure
    if (copy_to_user((void __user *) u, (void*)&q, sizeof(struct v3_fb_query_response))) { 
	printk("palacios: unable to copy fb response to user\n");
	return -EFAULT;
    }

    return 0;

}

int palacios_graphics_console_user_input(struct palacios_graphics_console *cons,
					 struct v3_fb_input __user  *u)
{
    struct v3_fb_input inp;
    int rc=0;


    if (copy_from_user(&inp,(void __user *) u, sizeof(struct v3_fb_input))) { 
	printk("palacios: copy from user in getting input in fb\n");
	return -EFAULT;
    }
	
    if (inp.data_type==V3_FB_KEY || inp.data_type==V3_FB_BOTH) { 
	rc = palacios_graphics_console_key(cons,inp.scan_code);
    }

    if (inp.data_type==V3_FB_MOUSE || inp.data_type==V3_FB_BOTH) { 
	rc |= palacios_graphics_console_mouse(cons,inp.mouse_data[0],inp.mouse_data[1],inp.mouse_data[2]);
    }

    if (rc) { 
	return -EFAULT;
    } else {
	return 0;
    }
}
