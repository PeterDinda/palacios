/*
 * Palacios VM Graphics Console Interface (shared framebuffer between palacios and host)
 * Copyright (c) 2011 Peter Dinda <pdinda@northwestern.edu>
 */

#ifndef __PALACIOS_GRAPHICS_CONSOLE_H__
#define __PALACIOS_GRAPHICS_CONSOLE_H__

#include <interfaces/vmm_graphics_console.h>

struct palacios_graphics_console {
    // descriptor for the data in the shared frame buffer
    struct v3_frame_buffer_spec spec;
    // the actual shared frame buffer
    // Note that "shared" here means shared between palacios and us
    // This data could of course also be shared with userland
    void *data;

    struct v3_guest * guest;

    int cons_refcount;
    int data_refcount;

    uint32_t num_updates;

    // Currently keystrokes and mouse movements are ignored

    // currently, we will not worry about locking this
    // lock_t ...
};


// This is the data structure that is passed back and forth with user-land
// ioctl
struct v3_fb_query_response {
    enum { V3_FB_DATA_ALL, V3_FB_DATA_BOX, V3_FB_UPDATE, V3_FB_SPEC } request_type;
    struct v3_frame_buffer_spec spec;    // in: desired spec; out: actual spec
    uint32_t x, y, w, h;                 // region to copy (0s = all) in/out args
    int updated;                         // whether this region has been updated or not
    void __user *data;                   // user space pointer to copy data to
};

// This is what userland sends down for input events
struct v3_fb_input {
    enum { V3_FB_KEY, V3_FB_MOUSE, V3_FB_BOTH}             data_type;
    uint8_t                              scan_code;
    uint8_t                              mouse_data[3];
};


int palacios_init_graphics_console(void);

int palacios_graphics_console_user_query(struct palacios_graphics_console *cons, 
					 struct v3_fb_query_response __user *fb);

int palacios_graphics_console_user_input(struct palacios_graphics_console *cons,
					 struct v3_fb_input __user  *in);


#endif
