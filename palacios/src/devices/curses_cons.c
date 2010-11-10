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
 * Author: Erik van der Kouwe (vdkouwe@cs.vu.nl)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

/* Interface between virtual video card and console */

#include <palacios/vmm.h>
#include <palacios/vmm_console.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_host_events.h>
#include <palacios/vmm_lock.h>
#include <palacios/vmm_string.h>
#include <palacios/vm_guest.h>

#include <devices/console.h>

#define NUM_ROWS 25
#define NUM_COLS 80
#define BYTES_PER_COL 2
#define BYTES_PER_ROW (NUM_COLS * BYTES_PER_COL)

#define SCREEN_SIZE (BYTES_PER_ROW * NUM_ROWS)

struct cons_state 
{
    v3_console_t cons;
    struct vm_device * frontend_dev;
};

static int screen_update(uint_t x, uint_t y, uint_t length, void *private_data);

static uint_t last_offset;

static int cursor_update(uint_t x, uint_t y, void *private_data) 
{
    struct vm_device *dev = (struct vm_device *) private_data;
    struct cons_state *state = (struct cons_state *) dev->private_data;
    uint_t offset = (x * BYTES_PER_COL) + (y * BYTES_PER_ROW);
    uint_t last_x, last_y;

    /* unfortunately Palacios sometimes misses some writes, 
     * but if they are accompanied by a cursor move we may be able to 
     * detect this
     */
    if (offset < last_offset) last_offset = 0;

    if (offset > last_offset) {
	last_x = (last_offset % BYTES_PER_ROW) / BYTES_PER_COL;
	last_y = last_offset / BYTES_PER_ROW;
	screen_update(last_x, last_y, offset - last_offset, private_data);
    }
    
    /* adjust cursor */	
    if (v3_console_set_cursor(state->cons, x, y) < 0) {
	PrintError("set cursor (0x%p, %d, %d) failed\n", state->cons, x, y);
	return -1;
    }
    
    /* done with console update */
    if (v3_console_update(state->cons) < 0) {
	PrintError("console update (0x%p) failed\n", state->cons);
	return -1;
    }
    
    return 0;
}

static int screen_update(uint_t x, uint_t y, uint_t length, void * private_data) {
    struct vm_device *dev = (struct vm_device *)private_data;
    struct cons_state *state = (struct cons_state *)dev->private_data;
    uint_t offset = (x * BYTES_PER_COL) + (y * BYTES_PER_ROW);
    uint8_t fb_buf[length];
    int i;
    uint_t cur_x = x;
    uint_t cur_y = y;
    
    /* grab frame buffer */
    memset(fb_buf, 0, length);
    v3_cons_get_fb(state->frontend_dev, fb_buf, offset, length);
    
    /* update the screen */
    for (i = 0; i < length; i += 2) {
	uint_t col_index = i;
	uint8_t col[2];
	
	col[0] = fb_buf[col_index];     // Character
	col[1] = fb_buf[col_index + 1]; // Attribute
	
	/* update current character */
	if (v3_console_set_char(state->cons, cur_x, cur_y, col[0], col[1]) < 0) {
	    PrintError("set cursor (0x%p, %d, %d, %d, %d) failed\n", 
		       state->cons, cur_x, cur_y, col[1], col[0]);
	    return -1;
	}
	
	// CAUTION: the order of these statements is critical
	// cur_y depends on the previous value of cur_x
	cur_y = cur_y + ((cur_x + 1) / NUM_COLS);
	cur_x = (cur_x + 1) % NUM_COLS;
    }
    
    /* done with console update */
    if (v3_console_update(state->cons) < 0) {
	PrintError("console update(0x%p) failed\n", state->cons);
	return -1;
    }
    
    /* store offset to catch missing notifications */
    last_offset = offset + length;
    
    return 0;
}

static int scroll(int rows, void * private_data) {
    struct vm_device *dev = (struct vm_device *)private_data;
    struct cons_state *state = (struct cons_state *)dev->private_data;

    if (rows < 0) {
	/* simply update the screen */
	return screen_update(0, 0, SCREEN_SIZE, private_data);
    }

    if (rows > 0) {
	/* scroll requested number of lines*/		
	if (v3_console_scroll(state->cons, rows) < 0) {
	    PrintError("console scroll (0x%p, %u) failed\n", state->cons, rows);
	    return -1;
	}

	/* done with console update */
	if (v3_console_update(state->cons) < 0) {
	    PrintError("console update (0x%p) failed\n", state->cons);
	    return -1;
	}
		
	last_offset = BYTES_PER_ROW * (NUM_ROWS - 1);		
    }
	
    return 0;
}

static struct v3_console_ops cons_ops = {
    .update_screen = screen_update, 
    .update_cursor = cursor_update,
    .scroll = scroll,
};

static struct v3_device_ops dev_ops = {
    .free = NULL,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};

static int cons_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) 
{
    struct cons_state * state = NULL;
    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");
    const char * frontend_tag = v3_cfg_val(frontend_cfg, "tag");
    struct vm_device * frontend = v3_find_dev(vm, frontend_tag);
    char * dev_id = v3_cfg_val(cfg, "ID");

    /* read configuration */
    V3_ASSERT(frontend_cfg);
    V3_ASSERT(frontend_tag);
    V3_ASSERT(frontend);


    /* allocate state */
    state = (struct cons_state *)V3_Malloc(sizeof(struct cons_state));
    V3_ASSERT(state);

    state->frontend_dev = frontend;

    /* open tty for screen display */
    state->cons = v3_console_open(vm);

    if (!state->cons) {
	PrintError("Could not open console\n");
	V3_Free(state);
	return -1;
    }

    /* allocate device */
    struct vm_device * dev = v3_allocate_device(dev_id, &dev_ops, state);
    V3_ASSERT(dev);

    /* attach device to virtual machine */
    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }

    /* attach to front-end display adapter */
    v3_console_register_cga(frontend, &cons_ops, dev);

    return 0;
}

device_register("CURSES_CONSOLE", cons_init)

