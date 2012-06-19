/* 
 * Linux interface for guest-context code injection
 *
 * (c) Kyle C. Hale 2011
 *
 */

#include <linux/elf.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include <linux/module.h>

#include <interfaces/vmm_host_hypercall.h>

#include "palacios.h"
#include "vm.h"
#include "linux-exts.h"
#include "iface-host-hypercall.h"

static int host_hypercall_nop(palacios_core_t core,
			      unsigned int hcall_id, 
			      struct guest_accessors *acc,
			      void *priv_data) {
  DEBUG("palacios: host_hypercall_nop dummy handler invoked\n");
  DEBUG(" rip=%p\n rsp=%p\n rbp=%p\n rflags=%p\n",
	 (void*)(acc->get_rip(core)),
	 (void*)(acc->get_rsp(core)),
	 (void*)(acc->get_rbp(core)),
	 (void*)(acc->get_rflags(core)));

  DEBUG(" rax=%p\n rbx=%p\n rcx=%p\n rdx=%p\n rsi=%p\n rdi=%p\n",
	 (void*)(acc->get_rax(core)),
	 (void*)(acc->get_rbx(core)),
	 (void*)(acc->get_rcx(core)),
	 (void*)(acc->get_rdx(core)),
	 (void*)(acc->get_rsi(core)),
	 (void*)(acc->get_rdi(core)));
  DEBUG(" r8=%p\n r9=%p\n r10=%p\n r11=%p\n r12=%p\n r13=%p\n r14=%p\n r15=%p\n",
	 (void*)(acc->get_r8(core)),
	 (void*)(acc->get_r9(core)),
	 (void*)(acc->get_r10(core)),
	 (void*)(acc->get_r11(core)),
	 (void*)(acc->get_r12(core)),
	 (void*)(acc->get_r13(core)),
	 (void*)(acc->get_r14(core)),
	 (void*)(acc->get_r15(core)));
  DEBUG(" cr0=%p\n cr2=%p\n cr3=%p\n cr4=%p\n cr8=%p\n efer=%p\n",
	 (void*)(acc->get_cr0(core)),
	 (void*)(acc->get_cr2(core)),
	 (void*)(acc->get_cr3(core)),
	 (void*)(acc->get_cr4(core)),
	 (void*)(acc->get_cr8(core)),
	 (void*)(acc->get_efer(core)));
  return 0;
}


static int vm_hypercall_add (struct v3_guest *guest, 
			     unsigned int cmd, 
			     unsigned long arg, 
			     void *priv_data) {

  struct hcall_data hdata;
  void *func;

  if (copy_from_user(&hdata,(void __user *) arg, sizeof(struct hcall_data))) { 
    ERROR("palacios: copy from user in getting input for hypercall add\n");
    return -EFAULT;
  }

  if (0==strcmp(hdata.fn,"")) { 
    WARNING("palacios: no hypercall function supplied, using default\n");
    func = (void*) host_hypercall_nop;
  } else {
    func = __symbol_get(hdata.fn);
  }

  if (func == NULL) { 
    ERROR("palacios: cannot find function '%s' for hypercall addition - perhaps your module hasn't been loaded yet?\n",hdata.fn);
    return -EFAULT;
  }

  if (v3_register_host_hypercall(guest->v3_ctx, 
				 hdata.hcall_nr,
				 func,
				 NULL)) { 
    ERROR("palacios: cannot register hypercall 0x%x for function %s (%p)\n",
	   hdata.hcall_nr, hdata.fn, func);
    return -EFAULT;
  } 

  INFO("palacios: hypercall %d (0x%x) registered for function %s (%p)\n", 
	 hdata.hcall_nr,hdata.hcall_nr,hdata.fn,func);
  return 0;
}

static int vm_hypercall_remove (struct v3_guest *guest, 
				unsigned int cmd, 
				unsigned long arg, 
				void *priv_data) {

  struct hcall_data hdata;

  if (copy_from_user(&hdata,(void __user *) arg, sizeof(struct hcall_data))) { 
    ERROR("palacios: copy from user in getting input for hypercall remove\n");
    return -EFAULT;
  }
  if (v3_unregister_host_hypercall(guest->v3_ctx, 
				   hdata.hcall_nr)) {
    ERROR("palacios: cannot unregister hypercall 0x%x\n", hdata.hcall_nr);
    return -EFAULT;
  } 

  INFO("palacios: hypercall %d (0x%x) unregistered\n", 
	 hdata.hcall_nr,hdata.hcall_nr);

  return 0;
}

static int init_host_hypercall (void) {
    return 0;
}


static int deinit_host_hypercall (void) {
    return 0;
}

static int guest_init_host_hypercall (struct v3_guest * guest, void ** vm_data) {
    add_guest_ctrl(guest, V3_VM_HYPERCALL_ADD, vm_hypercall_add, NULL);
    add_guest_ctrl(guest, V3_VM_HYPERCALL_REMOVE, vm_hypercall_remove, NULL);
    return 0;
}


static int guest_deinit_host_hypercall (struct v3_guest * guest, void * vm_data) {
    return 0;
}


static struct linux_ext host_hypercall_ext = {
    .name = "HOST_HYPERCALL",
    .init = init_host_hypercall,
    .deinit = deinit_host_hypercall,
    .guest_init = guest_init_host_hypercall,
    .guest_deinit = guest_deinit_host_hypercall 
};

register_extension(&host_hypercall_ext);
