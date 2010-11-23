/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Robert Deloatch <rtdeloatch@gmail.com>
 * Copyright (c) 2009, Steven Jaconette <stevenjaconette2007@u.northwestern.edu> 
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Robdert Deloatch <rtdeloatch@gmail.com>
 *         Steven Jaconette <stevenjaconette2007@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_io.h>

#include <devices/console.h>




#ifndef DEBUG_CGA
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define START_ADDR 0xB8000
#define END_ADDR 0xC0000

#define FRAMEBUF_SIZE (END_ADDR - START_ADDR)
#define SCREEN_SIZE 4000

#define BASE_CGA_PORT 0x3B0

#define NUM_COLS 80
#define NUM_ROWS 25
#define BYTES_PER_ROW (NUM_COLS * 2)
#define BYTES_PER_COL 2


struct video_internal {
    uint8_t * framebuf;
    addr_t framebuf_pa;

    // These store the values for unhandled ports, in case of a read op
    uint8_t port_store[44];


    uint8_t crtc_index_reg;          // io port 3D4
    uint8_t crtc_data_regs[25];      // io port 3D5
    
    /* IMPORTANT: These are column offsets _NOT_ byte offsets */
    uint16_t screen_offset; // relative to the framebuffer
    uint16_t cursor_offset; // relative to the framebuffer
    /* ** */

    // updating the screen offset is not atomic, 
    // so we need a temp variable to hold the partial update
    uint16_t tmp_screen_offset; 
    

    uint8_t passthrough;


    struct v3_console_ops * ops;
    void * private_data;



};




static void passthrough_in(uint16_t port, void * src, uint_t length) {
    switch (length) {
	case 1:
	    *(uint8_t *)src = v3_inb(port);
	    break;
	case 2:
	    *(uint16_t *)src = v3_inw(port);
	    break;
	case 4:
	    *(uint32_t *)src = v3_indw(port);
	    break;
	default:
	    break;
    }
}


static void passthrough_out(uint16_t port, void * src, uint_t length) {
    switch (length) {
	case 1:
	    v3_outb(port, *(uint8_t *)src);
	    break;
	case 2:
	    v3_outw(port, *(uint16_t *)src);
	    break;
	case 4:
	    v3_outdw(port, *(uint32_t *)src);
	    break;
	default:
	    break;
    }
}

static int video_write_mem(struct guest_info * core, addr_t guest_addr, void * dest, uint_t length, void * priv_data) {
    struct vm_device * dev = (struct vm_device *)priv_data;
    struct video_internal * state = (struct video_internal *)dev->private_data;
    uint_t fb_offset = guest_addr - START_ADDR;
    uint_t screen_byte_offset = state->screen_offset * BYTES_PER_COL;
    uint_t screen_length;

    PrintDebug("Guest address: %p length = %d, fb_offset=%d, screen_offset=%d\n", 
	       (void *)guest_addr, length, fb_offset, screen_byte_offset);

    if (state->passthrough) {
	memcpy(state->framebuf + fb_offset, V3_VAddr((void *)guest_addr), length);
    }

    if ((fb_offset >= screen_byte_offset) && (fb_offset < (screen_byte_offset + SCREEN_SIZE))) {
	uint_t screen_pos = fb_offset - screen_byte_offset;
	uint_t x = (screen_pos % BYTES_PER_ROW) / BYTES_PER_COL;
	uint_t y = screen_pos / BYTES_PER_ROW;
	PrintDebug("Sending Screen update\n");
	
	if (state->ops) {
	    PrintDebug("\tcalling update_screen()\n");
	    
	    /* avoid updates past end of screen */
	    screen_length = SCREEN_SIZE - screen_byte_offset;
	    if (screen_length > length) screen_length = length;
	    state->ops->update_screen(x, y, screen_length, state->private_data);
	}
    }

    return length;
}

static int video_read_port(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;


    PrintDebug("Video: Read port 0x%x\n", port);

    if (video_state->passthrough) {
	passthrough_in(port, dest, length);
    }

    return length;
}



static int video_write_port(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;


    PrintDebug("Video: write port 0x%x...\n", port);

    if (video_state->passthrough) {
	passthrough_out(port, src, length);
    } 

    return length;
}



static int crtc_data_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    uint8_t val = *(uint8_t *)src;
    uint_t index = video_state->crtc_index_reg;

    if (length != 1) {
	PrintError("Invalid write length for port 0x%x\n", port);
	return -1;
    }

    PrintDebug("Video: write on port 0x%x... (val=0x%x)\n", port, val);

    video_state->crtc_data_regs[index] = val;

    switch (index) {
	case 0x0c: { // scroll high byte
	    uint16_t tmp_val = val;
	    video_state->tmp_screen_offset = ((tmp_val << 8) & 0xff00);
	    break;
	}
	case 0x0d: {  // Scroll low byte
	    int diff = 0;

	    video_state->tmp_screen_offset += val;
	    diff = (video_state->tmp_screen_offset - video_state->screen_offset) / NUM_COLS;

	    // Update the true offset value
	    video_state->screen_offset = video_state->tmp_screen_offset;
	    video_state->tmp_screen_offset = 0;

	    PrintDebug("Scroll lines = %d, new screen offset=%d\n", 
		       diff, video_state->screen_offset * BYTES_PER_COL);

	    if (video_state->ops) {
		if (video_state->ops->scroll(diff, video_state->private_data) == -1) {
		    PrintError("Error sending scroll event\n");
		    return -1;
		}
	    }
	    break;
	}
	case 0x0E: {  // Cursor adjustment High byte
	    uint16_t tmp_val = val;
	    video_state->cursor_offset = ((tmp_val << 8) & 0xff00);

	    break;
	}
	case 0x0F: { // cursor adjustment low byte
 	    uint_t x = 0;
	    uint_t y = 0;
	    
	    video_state->cursor_offset += val;
	    
	    x = video_state->cursor_offset % NUM_COLS;
	    y = (video_state->cursor_offset - video_state->screen_offset) / NUM_COLS;
	    
	    PrintDebug("New Cursor Location; X=%d Y=%d\n", x, y);
	    
	    if (video_state->ops) {
		if (video_state->ops->update_cursor(x, y, video_state->private_data) == -1) {
		    PrintError("Error updating cursor\n");
		    return -1;
		}
	    } 

	    break;
	}
	default:
	    break;
    }

    if (video_state->passthrough) {
	passthrough_out(port, src, length);
    }

    return length;
}


static int crtc_index_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    
    if (length > 2) {
	PrintError("Invalid write length for crtc index register port: %d (0x%x)\n",
		   port, port);
	return -1;
    }
		   

    video_state->crtc_index_reg = *(uint8_t *)src;

    // Only do the passthrough IO for the first byte
    // the second byte will be done in the data register handler
    if (video_state->passthrough) {
	passthrough_out(port, src, 1);
    }

    if (length == 2) {
	if (crtc_data_write(core, port + 1, src + 1, length - 1, video_state) != (length - 1)) {
	    PrintError("could not handle implicit crtc data write\n");
	    return -1;
	}
    }

    return length;
}



int v3_cons_get_fb(struct vm_device * frontend_dev, uint8_t * dst, uint_t offset, uint_t length) {
    struct video_internal * state = (struct video_internal *)frontend_dev->private_data;
    uint_t screen_byte_offset = state->screen_offset * BYTES_PER_COL;

    PrintDebug("Getting framebuffer for screen; framebuf=%p, screen_offset=%d, offset=%d, length=%d\n", 
	       state->framebuf, screen_byte_offset, offset, length);

    V3_ASSERT(screen_byte_offset <= FRAMEBUF_SIZE - SCREEN_SIZE);
    V3_ASSERT(offset < SCREEN_SIZE);
    V3_ASSERT(length <= SCREEN_SIZE);
    V3_ASSERT(offset + length <= SCREEN_SIZE);
    memcpy(dst, state->framebuf + screen_byte_offset + offset, length);

    return 0;
}



static int free_device(struct vm_device * dev) {
    struct video_internal * video_state = (struct video_internal *)dev->private_data;

    if (video_state->framebuf_pa) {
	V3_FreePages((void *)(video_state->framebuf_pa), (FRAMEBUF_SIZE / 4096));
    }

    v3_unhook_mem(dev->vm, V3_MEM_CORE_ANY, START_ADDR);


    V3_Free(video_state);

    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = free_device,
};

static int cga_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct video_internal * video_state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    char * passthrough_str = v3_cfg_val(cfg, "passthrough");
    int ret = 0;
    
    PrintDebug("video: init_device\n");

    video_state = (struct video_internal *)V3_Malloc(sizeof(struct video_internal));
    memset(video_state, 0, sizeof(struct video_internal));

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, video_state);

    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(video_state);
	return -1;
    }

    video_state->framebuf_pa = (addr_t)V3_AllocPages(FRAMEBUF_SIZE / 4096);
    video_state->framebuf = V3_VAddr((void *)(video_state->framebuf_pa));
    memset(video_state->framebuf, 0, FRAMEBUF_SIZE);

    PrintDebug("PA of array: %p\n", (void *)(video_state->framebuf_pa));

    if ((passthrough_str != NULL) &&
	(strcasecmp(passthrough_str, "enable") == 0)) {;
	video_state->passthrough = 1;
    }


    if (video_state->passthrough) {
	PrintDebug("Enabling CGA Passthrough\n");
	if (v3_hook_write_mem(vm, V3_MEM_CORE_ANY, START_ADDR, END_ADDR, 
			      START_ADDR, &video_write_mem, dev) == -1) {
	    PrintDebug("\n\nVideo Hook failed.\n\n");
	}
    } else {
	if (v3_hook_write_mem(vm, V3_MEM_CORE_ANY, START_ADDR, END_ADDR, 
			      video_state->framebuf_pa, &video_write_mem, dev) == -1) {
	    PrintDebug("\n\nVideo Hook failed.\n\n");
	}
    }


    ret |= v3_dev_hook_io(dev, 0x3b0, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3b1, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3b2, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3b3, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3b4, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3b5, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3b6, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3b7, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3b8, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3b9, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3ba, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3bb, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3c0, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3c1, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3c2, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3c3, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3c4, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3c5, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3c6, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3c7, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3c8, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3c9, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3ca, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3cb, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3cc, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3cd, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3ce, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3cf, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3d0, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3d1, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3d2, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3d3, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3d4, &video_read_port, &crtc_index_write);
    ret |= v3_dev_hook_io(dev, 0x3d5, &video_read_port, &crtc_data_write);
    ret |= v3_dev_hook_io(dev, 0x3d6, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3d7, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3d8, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3d9, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3da, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3db, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3dc, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3dd, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3de, &video_read_port, &video_write_port);
    ret |= v3_dev_hook_io(dev, 0x3df, &video_read_port, &video_write_port);

    if (ret != 0) {
	PrintError("Error allocating cga io port\n");
	v3_remove_device(dev);
	return -1;
    }

    return 0;
}

device_register("CGA_VIDEO", cga_init);


int v3_console_register_cga(struct vm_device * cga_dev, struct v3_console_ops * ops, void * private_data) {
    struct video_internal * video_state = (struct video_internal *)cga_dev->private_data;
    
    video_state->ops = ops;
    video_state->private_data = private_data;

    return 0;
}
