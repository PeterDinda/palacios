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


#ifndef __VMM_CONSOLE_H__
#define __VMM_CONSOLE_H__

#include <palacios/vmm.h>


#ifdef __V3VEE__

typedef void * v3_console_t;

v3_console_t v3_console_open(struct v3_vm_info * vm, uint32_t width, uint32_t height);
void v3_console_close(v3_console_t cons);

int v3_console_set_cursor(v3_console_t cons, int x, int y);
int v3_console_set_char(v3_console_t cons, int x, int y, char c, uint8_t style);
int v3_console_scroll(v3_console_t cons, int lines);
int v3_console_update(v3_console_t cons);
int v3_console_set_text_resolution(v3_console_t cons, int cols, int rows);

#endif



struct v3_console_hooks {
    /* open console device, mode is a combination of TTY_OPEN_MODE_* flags */
    void *(*open)(void * priv_data, unsigned int width, unsigned int height);
    
    void (*close)(void * tty);

    /* set cursor position */
    int (*set_cursor)(void * tty, int x, int y);

    /* output character c with specified style at (x, y) */
    int (*set_character)(void * tty, int x, int y, char c, unsigned char style);

    /* scroll the console down the specified number of lines */
    int (*scroll)(void * tty, int lines);

    /* change the text resolution (always followed by a full screen update) */
    int (*set_text_resolution)(void * tty, int cols, int rows);

    /* force update of console display; all updates by above functions
     * may be defferred until the next tty_update call 
     */
    int (*update)(void * tty);
};


extern void V3_Init_Console(struct v3_console_hooks * hooks);

#endif
