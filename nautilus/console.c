/*
  Interface to Nautilus screen and keyboard
*/

#include <nautilus/nautilus.h>
#include <nautilus/vc.h>
#include <dev/kbd.h>


#include <palacios/vmm.h>

#include <interfaces/vmm_console.h>
#include <palacios/vmm_host_events.h>

#include "palacios.h"

static void kbd_callback(nk_scancode_t scancode, void *priv)
{
  struct nk_vm_state *n = (struct nk_vm_state *) priv;

  struct v3_keyboard_event event = {0,scancode};

  if (n && n->vm) {
    v3_deliver_keyboard_event(n->vm, &event);
  } else {
    ERROR("Missing target for event... n=%p, n->vm=%p\n", n, n?n->vm:0);
  }
}


static void * palacios_tty_open(void * private_data, unsigned int width, unsigned int height) 
{
  struct nk_vm_state *n = palacios_get_selected_vm();

  if (!n) { 
    ERROR("Cannot create console without selected VM\n");
    return 0;
  }
  if (width!=80 || height!=25) { 
    ERROR("Console is wrong size\n");
    return 0;
  }

  if (n->vc) { 
    ERROR("Cannot open multiple consoles per selected VM\n");
    return 0;
  }

  
  n->vc = nk_create_vc(n->name,
		       RAW_NOQUEUE,
		       0x5f,
		       kbd_callback,
		       n);

  if (!n->vc) { 
    ERROR("Failed to create vc\n");
    return 0;
  }

  nk_vc_clear_specific(n->vc,0x5f);

  return n;

}


static int palacios_tty_cursor_set(void * console, int x, int y) 
{
  struct nk_vm_state *n = (struct nk_vm_state *) console;

  if (n && n->vc) { 
    nk_vc_setpos_specific(n->vc,x,y);
    return 0;
  } else {
    return -1;
  }
}

static int palacios_tty_character_set(void * console, int x, int y, char c, unsigned char style) 
{
  struct nk_vm_state *n = (struct nk_vm_state *) console;

  if (n && n->vc) { 
    nk_vc_display_char_specific(n->vc,c,style,x,y);
    nk_vc_setattr_specific(n->vc,style);
    return 0;
  } else {
    return -1;
  }
}

static int palacios_tty_scroll(void * console, int lines) 
{
  struct nk_vm_state *n = (struct nk_vm_state *) console;

  if (n && n->vc) { 
    int i;
    for (i=0;i<lines;i++) {
      nk_vc_scrollup_specific(n->vc);
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
    } else {
      return 0;
    }
  } else {
    return -1;
  }
}
 
static int palacios_tty_update(void * console) 
{
  // not used for VC
  return 0;
}

static void palacios_tty_close(void * console) 
{
  struct nk_vm_state *n = (struct nk_vm_state *) console;

  if (n && n->vc) { 
    nk_destroy_vc(n->vc);
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
  INFO("Palacios Console\n");
  
  V3_Init_Console(&palacios_console_hooks);
  
  return 0;
}

int nautilus_console_deinit(void)
{
    return 0;
}




