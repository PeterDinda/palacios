/*
  Interface to Nautilus screen and keyboard
*/

#include <nautilus/nautilus.h>
#include <nautilus/printk.h>
#include <nautilus/cga.h>
#include <dev/kbd.h>


#include <palacios/vmm.h>

#include <interfaces/vmm_console.h>
#include <palacios/vmm_host_events.h>

#include "palacios.h"

/*
  This is a gruesome hack to allow the VM designated by the 
  host as "the_vm" to do I/O to the standard VGA text mode console
*/

extern void *the_vm;

static void * palacios_tty_open(void * private_data, unsigned int width, unsigned int height) 
{
    if (width!=80 || height!=25) { 
	ERROR("Console is wrong size\n");
	return 0;
    }
    INFO("Console connected\n");
    return (void*)1;
}


static int palacios_tty_cursor_set(void * console, int x, int y) 
{
    if (console) { 
	term_setpos(x,y);
	return 0;
    } else {
	return -1;
    }
}

static int palacios_tty_character_set(void * console, int x, int y, char c, unsigned char style) 
{
    if (console) {
	term_putc(c,style,x,y);
	return 0;
    } else {
	return -1;
    }
}

static int palacios_tty_scroll(void * console, int lines) 
{
    if (console) { 
	int i;
	for (i=0;i<lines;i++) {
	    term_scrollup();
	}
	return 0;
    } else {
	return -1;
    }
}


static int palacios_set_text_resolution(void * console, int cols, int rows) 
{
    if (console) { 
	if (cols!=80 || rows!=25) { 
	    ERROR("Cannot change resolution\n");
	    return -1;
	}
	else return 0;
    } else {
	return -1;
    }
}
 
static int palacios_tty_update(void * console) 
{
    return 0;
}

static void palacios_tty_close(void * console) 
{
    if (console) { 
	term_clear();
	term_print("Palacios Console Finished\n");
    }
}

static void kbd_callback(uint8_t scancode, uint8_t status)
{
    struct v3_keyboard_event event = {status,scancode};

    //INFO("kbd callback scancode=%x\n",scancode);
    if (the_vm) {
	//INFO("Deliver scancode 0x%x\n",scancode);
	v3_deliver_keyboard_event(the_vm, &event);
    }
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



int nautilus_console_init(void) 
{
    term_clear();
    term_print("Palacios Console\n");

    V3_Init_Console(&palacios_console_hooks);

    kbd_register_callback(kbd_callback);
    
    return 0;
}

int nautilus_console_deinit(void)
{
    // nothing to do
    return 0;
}




