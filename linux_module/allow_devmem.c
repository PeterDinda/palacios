#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>

#include "palacios.h"
#include "allow_devmem.h"

/*
  The purpose of this component is to disable and reenable
  strict devmem control.

  On kernels compiled with CONFIG_STRICT_DEVMEM, /dev/mem access
  from user space is limited to the first 1 MB and to non-memory
  regions (ie, devices).  For various purposes in Palacios,
  for example linux_usr/v3_guest_mem.[ch], we want to allow 
  host user space access to guest memory via mmap.   

  This borrows from Dave Anderson @ Red Hat's implementation
*/

#define DEVMEM_CHECK_FUNC "devmem_is_allowed"

static enum { NOTRUN=0, OPEN, SET, NOTSET } devmem_state = NOTRUN;


// this is invoked after return from devmem_is_allowed()
static int devmem_ret_handler(struct kretprobe_instance *ri, 
			      struct pt_regs *regs)
{
  regs->ax = 1;  // yes, it's allowed - "ax"  now means "rax"
  return 0;
}

static struct kretprobe devmem_kretprobe = {
  .handler = devmem_ret_handler,
  .maxactive = 20 // up to 20 at a time
};


int palacios_allow_devmem(void)
{
#ifndef CONFIG_STRICT_DEVMEM
  INFO("System already has open /dev/mem - doing nothing\n");
  devmem_state = OPEN;
  return 0;
#else
  switch (devmem_state) { 
  case NOTRUN:
  case NOTSET: {
    int rc;

    devmem_kretprobe.kp.symbol_name = DEVMEM_CHECK_FUNC;
    
    rc = register_kretprobe(&devmem_kretprobe);
    
    if (rc<0) {
      ERROR("register_kretprobe failed, returned %d\n", rc);
      return -1;
    }
    
    devmem_state=SET;
    INFO("/dev/mem is now enabled (probe at %p)\n",devmem_kretprobe.kp.addr);

    return 0;
  }

    break;
  default:
    // already set
    return 0;
    break;
  }
#endif    
}


int palacios_restore_devmem(void) 
{
#ifndef CONFIG_STRICT_DEVMEM
  INFO("System already has open /dev/mem - doing nothing\n");
  devmem_state = OPEN;
  return 0;
#else 
  switch (devmem_state) { 
  case NOTRUN: 
    ERROR("Ignoring disable of dev mem\n");
    return 0;
  case NOTSET: 
    INFO("/dev/mem not explicitly enabled, ignoring restore request\n");
    return 0;
    break;

  case SET: {

    unregister_kretprobe(&devmem_kretprobe);
    
    if (devmem_kretprobe.nmissed>0) { 
      ERROR("Note: missed %d instances of %s\n", 
	    devmem_kretprobe.nmissed, DEVMEM_CHECK_FUNC);
    }
    
    devmem_state=NOTSET;

    INFO("Restored strict /dev/mem access\n");
    return 0;
  }

    break;
  default:
    // already set
    return 0;
    break;
  }

  return 0;
#endif
}


