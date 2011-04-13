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
#include <interfaces/vmm_console.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_host_events.h>
#include <palacios/vmm_lock.h>
#include <palacios/vmm_string.h>
#include <palacios/vm_guest.h>

#include <devices/console.h>

#ifndef DEBUG_CURSES_CONS
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define BYTES_PER_COL 2

struct cons_state {
    v3_console_t cons;
    int rows;
    int cols;
    struct vm_device * frontend_dev;
};

static int screen_update(uint_t x, uint_t y, uint_t length, void *private_data);

static int screen_update_all(void * private_data) {
    struct vm_device *dev = (struct vm_device *) private_data;
    struct cons_state *state = (struct cons_state *)dev->private_data;
    uint_t screen_size;

    screen_size = state->cols * state->rows * BYTES_PER_COL;
    return screen_update(0, 0, screen_size, private_data);
}

static int cursor_update(uint_t x, uint_t y, void *private_data) 
{
    struct vm_device *dev = (struct vm_device *) private_data;
    struct cons_state *state = (struct cons_state *) dev->private_data;
    uint_t offset;

    PrintDebug("cursor_update(%d, %d, %p)\n", x, y, private_data);

    /* avoid out-of-range coordinates */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= state->cols) x = state->cols - 1;
    if (y >= state->rows) y = state->rows - 1;
    offset = (x + y * state->cols) * BYTES_PER_COL;
    
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
    struct vm_device * dev = (struct vm_device *)private_data;
    struct cons_state * state = (struct cons_state *)dev->private_data;
    uint_t offset = (x + y * state->cols) * BYTES_PER_COL;
    uint8_t fb_buf[length];
    int i;
    uint_t cur_x = x;
    uint_t cur_y = y;
    
    PrintDebug("screen_update(%d, %d, %d, %p)\n", x, y, length, private_data);

    /* grab frame buffer */
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
	cur_y = cur_y + ((cur_x + 1) / state->cols);
	cur_x = (cur_x + 1) % state->cols;
    }
    
    /* done with console update */
    if (v3_console_update(state->cons) < 0) {
	PrintError("console update(0x%p) failed\n", state->cons);
	return -1;
    }
    
    return 0;
}

static int scroll(int rows, void * private_data) {
    struct vm_device *dev = (struct vm_device *)private_data;
    struct cons_state *state = (struct cons_state *)dev->private_data;

    PrintDebug("scroll(%d, %p)\n", rows, private_data);

    if (rows < 0) {
	/* simply update the screen */
	return screen_update_all(private_data);
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
    }
	
    return 0;
}

static int set_text_resolution(int cols, int rows, void * private_data) {
    struct vm_device *dev = (struct vm_device *)private_data;
    struct cons_state *state = (struct cons_state *)dev->private_data;

    PrintDebug("set_text_resolution(%d, %d, %p)\n", cols, rows, private_data);

    /* store resolution for internal use */
    V3_ASSERT(cols >= 1);
    V3_ASSERT(rows >= 1);
    state->cols = cols;
    state->rows = rows;

    /* set notification regarding resolution change */
    if (v3_console_set_text_resolution(state->cons, cols, rows) < 0) {
	PrintError("console set_text_resolution (0x%p, %u, %u) failed\n", state->cons, cols, rows);
	return -1;
    }

    /* update the screen */
    return screen_update_all(private_data);
}

static int cons_free(struct cons_state * state) {
    v3_console_close(state->cons);

    // remove host event

    V3_Free(state);

    return 0;
}

static int console_event_handler(struct v3_vm_info * vm, 
				 struct v3_console_event * evt, 
				 void * priv_data) {
    return screen_update_all(priv_data);
}

static struct v3_console_ops cons_ops = {
    .update_screen = screen_update, 
    .update_cursor = cursor_update,
    .scroll = scroll,
    .set_text_resolution = set_text_resolution,
};

static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))cons_free,
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
    state->cols = 80;
    state->rows = 25;

    /* open tty for screen display */
    state->cons = v3_console_open(vm, state->cols, state->rows);

    if (!state->cons) {
	PrintError("Could not open console\n");
	V3_Free(state);
	return -1;
    }

    /* allocate device */
    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }

    /* attach to front-end display adapter */
    v3_console_register_cga(frontend, &cons_ops, dev);

    v3_hook_host_event(vm, HOST_CONSOLE_EVT, V3_HOST_EVENT_HANDLER(console_event_handler), dev);

    return 0;
}

device_register("CURSES_CONSOLE", cons_init)

