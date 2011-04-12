/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2010, Erik van der Kouwe <vdkouwe@cs.vu.nl> 
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 * Author: Erik van der Kouwe <vdkouwe@cs.vu.nl> 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <interfaces/vmm_console.h>
#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>

struct v3_console_hooks * console_hooks = 0;

v3_console_t v3_console_open(struct v3_vm_info * vm, uint32_t width, uint32_t height) {
    V3_ASSERT(console_hooks != NULL);
    V3_ASSERT(console_hooks->open != NULL);

    return console_hooks->open(vm->host_priv_data, width, height);
}

void v3_console_close(v3_console_t cons) {
    V3_ASSERT(console_hooks);
    V3_ASSERT(console_hooks->close);

    console_hooks->close(cons);
}

int v3_console_set_cursor(v3_console_t cons, int x, int y) {
    V3_ASSERT(console_hooks != NULL);
    V3_ASSERT(console_hooks->set_cursor != NULL);

    return console_hooks->set_cursor(cons, x, y);
}

int v3_console_set_char(v3_console_t cons, int x, int y, char c, uint8_t style) {
    V3_ASSERT(console_hooks != NULL);
    V3_ASSERT(console_hooks->set_character != NULL);

    return console_hooks->set_character(cons, x, y, c, style);    
}

    
int v3_console_scroll(v3_console_t cons, int lines) {
    V3_ASSERT(console_hooks != NULL);
    V3_ASSERT(console_hooks->scroll != NULL);
    
    return console_hooks->scroll(cons, lines);
}

int v3_console_set_text_resolution(v3_console_t cons, int cols, int rows) {
    V3_ASSERT(console_hooks != NULL);
    V3_ASSERT(console_hooks->set_text_resolution != NULL);
    
    return console_hooks->set_text_resolution(cons, cols, rows);
}

int v3_console_update(v3_console_t cons) {
    V3_ASSERT(console_hooks != NULL);
    V3_ASSERT(console_hooks->update != NULL);
    
    return console_hooks->update(cons);
}

void V3_Init_Console(struct v3_console_hooks * hooks) {
    console_hooks = hooks;
    PrintDebug("V3 console inited\n");

    return;
}
