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


#include <interfaces/vmm_graphics_console.h>
#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>

struct v3_graphics_console_hooks * graphics_console_hooks = 0;

v3_graphics_console_t v3_graphics_console_open(struct v3_vm_info * vm, 
					       struct v3_frame_buffer_spec *desired_spec,
					       struct v3_frame_buffer_spec *actual_spec)
{					       
    V3_ASSERT(graphics_console_hooks != NULL);
    V3_ASSERT(graphics_console_hooks->open != NULL);

    return graphics_console_hooks->open(vm->host_priv_data, desired_spec, actual_spec);
}

void v3_graphics_console_close(v3_graphics_console_t cons) 
{
    V3_ASSERT(graphics_console_hooks);
    V3_ASSERT(graphics_console_hooks->close);

    graphics_console_hooks->close(cons);
}

void * v3_graphics_console_get_frame_buffer_data_read(v3_graphics_console_t cons, struct v3_frame_buffer_spec *spec) 
{
    V3_ASSERT(graphics_console_hooks != NULL);
    V3_ASSERT(graphics_console_hooks->get_data_read != NULL);
    
    return graphics_console_hooks->get_data_read(cons, spec);
}


void  v3_graphics_console_release_frame_buffer_data_read(v3_graphics_console_t cons)
{
    V3_ASSERT(graphics_console_hooks != NULL);
    V3_ASSERT(graphics_console_hooks->release_data_read != NULL);
    
    return graphics_console_hooks->release_data_read(cons);
}

void * v3_graphics_console_get_frame_buffer_data_rw(v3_graphics_console_t cons, struct v3_frame_buffer_spec *spec) 
{
    V3_ASSERT(graphics_console_hooks != NULL);
    V3_ASSERT(graphics_console_hooks->get_data_rw != NULL);
    
    return graphics_console_hooks->get_data_rw(cons, spec);
}

void v3_graphics_console_release_frame_buffer_data_rw(v3_graphics_console_t cons)
{
    V3_ASSERT(graphics_console_hooks != NULL);
    V3_ASSERT(graphics_console_hooks->release_data_rw != NULL);
    
    return graphics_console_hooks->release_data_rw(cons);
}



int v3_graphics_console_inform_update(v3_graphics_console_t cons) {
    V3_ASSERT(graphics_console_hooks != NULL);
    V3_ASSERT(graphics_console_hooks->changed != NULL);
    
    return graphics_console_hooks->changed(cons);
}

void V3_Init_Graphics_Console(struct v3_graphics_console_hooks * hooks) {
    graphics_console_hooks = hooks;
    PrintDebug("V3 graphics console inited\n");

    return;
}
