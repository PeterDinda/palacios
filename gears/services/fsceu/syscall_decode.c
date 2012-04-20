/* 
 *   Kyle C. Hale 2012
 * Module to be injected into guest kernel to enable
 *  selective system call exiting
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "syscall_decode.h"

#define AUTHOR "Kyle C. Hale <kh@u.northwestern.edu>"
#define INFO "This kernel module is a paravirtualized module that will"\
              "reroute system calls to a handler stub. This stub will decide"\
              "based on a VMM-mapped vector whether or not the particular system call"\
              "should trap to the VMM."


extern void syscall_stub(void);

uint64_t * state_save_area;
uint8_t  * syscall_map;

int init_module (void) {
    uint64_t ret;

    state_save_area = kmalloc(sizeof(uint64_t)*(PAGE_SIZE), GFP_KERNEL);
    if (!state_save_area){
        printk("Problem allocating sate save area\n");
        return -1;
    }
    memset(state_save_area, 0, sizeof(uint64_t)*(PAGE_SIZE));

    syscall_map = kmalloc(NUM_SYSCALLS, GFP_KERNEL);
    if (!syscall_map) {
        printk("Problem allocating syscall map\n");
        return -1;
    }
    memset(syscall_map, 0, NUM_SYSCALLS);

    // vmm will return -1 on error, address of syscall_entry on success
    asm volatile ("vmmcall"
                : "=a" (ret)
                : "0" (SYSCALL_SETUP_HCALL), "b" (syscall_stub), "c" (syscall_map), 
                  "d" (state_save_area));

    if (ret < 0) {
        printk("syscall_decode: problem initing selective syscall exiting\n");
        return -1;
    } else {
        state_save_area[NUM_SAVE_REGS] = ret; 
    }

    printk("syscall_decode: inited\n");
    return 0;
}


void cleanup_module (void) {
  int ret;
  kfree(state_save_area);
  kfree(syscall_map);
  /* tell Palacios to restore the original system call entry point */
  asm volatile ("vmmcall"
                : "=a" (ret)
                : "0"(SYSCALL_CLEANUP_HCALL));
  if (ret < 0) {
    printk("syscall_decode: problem deiniting selective syscall exiting\n");
  }

  printk("syscall_page: deinited\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR);
MODULE_VERSION("0.2");
MODULE_DESCRIPTION(INFO);

