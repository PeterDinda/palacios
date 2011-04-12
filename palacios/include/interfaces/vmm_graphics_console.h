/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMM_GRAPHICS_CONSOLE_H__
#define __VMM_GRAPHICS_CONSOLE_H__

#include <palacios/vmm.h>


/* A graphics console is opaque to the palacios */
typedef void * v3_graphics_console_t;


#ifdef __V3VEE__


struct v3_frame_buffer_spec ;


/* we give a desired spec and get back the actual spec */
v3_graphics_console_t v3_graphics_console_open(struct v3_vm_info * vm, 
					       struct v3_frame_buffer_spec *desired_spec,
					       struct v3_frame_buffer_spec *actual_spec);

/* Spec is an optional output argument the indicates the current FB specification */
/* The data will only be read or read/written between a get and release */
void *v3_graphics_console_get_frame_buffer_data_read(v3_graphics_console_t cons, struct v3_frame_buffer_spec *spec);
void  v3_graphics_console_release_frame_buffer_data_read(v3_graphics_console_t cons);
void *v3_graphics_console_get_frame_buffer_data_rw(v3_graphics_console_t cons, struct v3_frame_buffer_spec *spec);
void  v3_graphics_console_release_frame_buffer_data_rw(v3_graphics_console_t cons);
// returns >0 if a redraw in response to this update would be useful now
int   v3_graphics_console_inform_update(v3_graphics_console_t cons);

void v3_graphics_console_close(v3_graphics_console_t cons);

#endif



/*
   A graphics console provides a frame buffer into which to render
   which is specified as follows.


   The data of the actual frame buffer is in row-major order.
*/
struct v3_frame_buffer_spec {
    uint32_t height;
    uint32_t width;
    uint8_t  bytes_per_pixel; 
    uint8_t  bits_per_channel;
    uint8_t  red_offset;   // byte offset in pixel to get to red channel
    uint8_t  green_offset; // byte offset in pixel to get to green channel
    uint8_t  blue_offset;  // byte offset in pixel to get to blue channel
};



struct v3_graphics_console_hooks {
    // Note that the minimum spec argument may be null, indicating that it is not provided 
    v3_graphics_console_t (*open)(void * priv_data, 
				  struct v3_frame_buffer_spec *desired_spec, 
				  struct v3_frame_buffer_spec *actual_spec);
    void                  (*close)(v3_graphics_console_t cons);

    void * (*get_data_read)(v3_graphics_console_t cons, struct v3_frame_buffer_spec *cur_spec);
    void   (*release_data_read)(v3_graphics_console_t cons);
    void * (*get_data_rw)(v3_graphics_console_t cons, struct v3_frame_buffer_spec *cur_spec);
    void   (*release_data_rw)(v3_graphics_console_t cons);

    // callback to indicate that the FB is stale and that an update can occur
    // a positive return value indicates we should re-render now
    int (*changed)(v3_graphics_console_t cons);
};


extern void V3_Init_Graphics_Console(struct v3_graphics_console_hooks *hooks);

#endif
