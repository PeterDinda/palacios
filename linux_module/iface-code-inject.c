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

#include <gears/code_inject.h>

#include "palacios.h"
#include "vm.h"
#include "linux-exts.h"
#include "iface-code-inject.h"


/* eventually this should probably be a hash table,
 * hashed on unique inject data 
 */
static struct top_half_data *top_map[MAX_INJ] = {[0 ... MAX_INJ - 1] = 0};

static int register_top(struct top_half_data *top) {
    int i;

    for (i = 0; i < MAX_INJ; i++) {
        if (!top_map[i]) {
            top_map[i] = top;
            return i;
        }
    }

    return -1;
}


static void free_inject_data (void) {
    int i;

    for(i = 0; i < MAX_INJ; i++) {
        if (top_map[i]) {
            kfree(top_map[i]->elf_data);
            kfree(top_map[i]);
        }
    }
}



static int vm_tophalf_inject (struct v3_guest * guest, unsigned int cmd, unsigned long arg, void * priv_data) {
    struct top_half_data top_arg;
    struct top_half_data * top;

    top = kmalloc(sizeof(struct top_half_data), GFP_KERNEL);
    if (IS_ERR(top)) {
        ERROR("Palacios Error: could not allocate space for top half data\n");
        return -EFAULT;
    }
    memset(top, 0, sizeof(struct top_half_data));
    
    INFO("Palacios: Loading ELF data...\n");
    if (copy_from_user(&top_arg, (void __user *)arg, sizeof(struct top_half_data))) {
        ERROR("palacios: error copying ELF from userspace\n");
        return -EFAULT;
    }

    top->elf_size = top_arg.elf_size;
    top->func_offset = top_arg.func_offset;
    top->is_dyn = top_arg.is_dyn;

    /* we have a binary name */
    if (top_arg.is_exec_hooked) {
        strcpy(top->bin_file, top_arg.bin_file);
        top->is_exec_hooked = 1;
        DEBUG("top->bin_file is %s\n", top->bin_file);
    } 

    DEBUG("Palacios: Allocating %lu B of kernel memory for ELF binary data...\n", top->elf_size);
    top->elf_data = kmalloc(top->elf_size, GFP_KERNEL);
    if (IS_ERR(top->elf_data)) {
        ERROR("Palacios Error: could not allocate space for binary image\n");
        return -EFAULT;
    }
    memset(top->elf_data, 0, top->elf_size);

    INFO("Palacios: Copying ELF image into kernel module...\n");
    if (copy_from_user(top->elf_data, (void __user *)top_arg.elf_data, top->elf_size)) {
        ERROR("Palacios: Error loading elf data\n");
        return -EFAULT;
    }

    if (register_top(top) < 0) 
        return -1;
    
    INFO("Palacios: setting up inject code...\n");
    if (v3_insert_code_inject(guest->v3_ctx, top->elf_data, top->elf_size, 
                         top->bin_file, top->is_dyn, top->is_exec_hooked, top->func_offset) < 0) {
        ERROR("Palacios Error: error setting up inject code\n");
        return -1;
    }

    INFO("Palacios: injection registration complete\n");
    return 0;
}


static int init_code_inject (void) {
    return 0;
}


static int deinit_code_inject (void) {
    return 0;
}


static int guest_init_code_inject (struct v3_guest * guest, void ** vm_data) {
    add_guest_ctrl(guest, V3_VM_TOPHALF_INJECT, vm_tophalf_inject, NULL);
    return 0;
}


static int guest_deinit_code_inject (struct v3_guest * guest, void * vm_data) {
    free_inject_data();
    return 0;
}


static struct linux_ext code_inject_ext = {
    .name = "CODE_INJECT",
    .init = init_code_inject,
    .deinit = deinit_code_inject,
    .guest_init = guest_init_code_inject,
    .guest_deinit = guest_deinit_code_inject 
};

register_extension(&code_inject_ext);
