/* 
 * Linux interface for guarded module registration
 *
 * (c) Kyle C. Hale 2012
 *
 */

#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/list.h>
#include <gears/guard_mods.h>
#include "palacios.h"
#include "vm.h"
#include "linux-exts.h"
#include "iface-guard-mods.h"


static int
vm_register_mod (struct v3_guest * guest, 
                 unsigned int cmd,
                 unsigned long arg,
                 void * priv_data) 
{
    uint64_t ret;
    struct v3_guard_mod arg_mod;
    struct v3_guard_mod * gm;
    int i;

    gm = palacios_alloc(sizeof(struct v3_guard_mod));
    if (!gm) {
        ERROR("palacios: error allocating guarded module\n");
        return -EFAULT;
    }

    if (copy_from_user(&arg_mod, (void __user *)arg, sizeof(struct v3_guard_mod))) {
        ERROR("palacios: error copying guarded module data from userspace\n");
        return -EFAULT;
    }

    memcpy(gm, &arg_mod, sizeof(struct v3_guard_mod));

    /* get module name */
    gm->name = palacios_alloc(strnlen_user((char __user *)arg_mod.name, MAX_MOD_NAME_LEN)+1);
    if (!gm->name) {
        ERROR("Problem allocating space for mod name\n");
        return -1;
    }

    if (strncpy_from_user(gm->name, (char __user *)arg_mod.name, MAX_MOD_NAME_LEN) == -EFAULT) {
        ERROR("problem copying from userspace\n");
        return -1;
    }

    /* get module content hash */
    gm->content_hash = palacios_alloc(strnlen_user((char __user *)arg_mod.content_hash, MAX_HASH_LEN)+1);
    if (!gm->content_hash) {
        ERROR("Problem allocating space for content hash\n");
        return -1;
    }

    if (strncpy_from_user(gm->content_hash, (char __user *)arg_mod.content_hash, MAX_HASH_LEN) == -EFAULT) {
        ERROR("problem copying from userspace\n");
        return -1;
    }

    /* get valid entry points */
    gm->entry_points = palacios_alloc(sizeof(struct v3_guard_mod)*arg_mod.num_entries);
    if (!gm->entry_points) {
        ERROR("Problem allocating space for entry point array\n");
        return -1;
    }

    if (copy_from_user(gm->entry_points, (void __user *)arg_mod.entry_points, sizeof(struct v3_guard_mod)*arg_mod.num_entries)
        == -EFAULT) {
        ERROR("problem copying from userspace\n");
        return -1;
    }

    for (i = 0; i < gm->num_entries; i++) {
        int len;
        char * tmp;   

        if ((len = strnlen_user((char __user *)gm->entry_points[i].name, MAX_MOD_NAME_LEN)+1) == -EFAULT) {
            ERROR("problem getting strlen from userspace\n");
            return -1;
        }

        tmp = palacios_alloc(len);
        if (!tmp) {
            ERROR("Problem allocating space for string\n");
            return -1;
        }

        if (strncpy_from_user(tmp, (char __user *)gm->entry_points[i].name, MAX_MOD_NAME_LEN) == -EFAULT) {
                ERROR("problem copying from userspace\n");
                return -1;
        }

        gm->entry_points[i].name = tmp;
    }

    /* get list of privileges */
    gm->priv_array = palacios_alloc(sizeof(char*)*arg_mod.num_privs);
    if (!gm->priv_array) {
        ERROR("Problem allocating space for privilege array\n");
        return -1;
    }

    if (copy_from_user(gm->priv_array, (void __user *)arg_mod.priv_array, sizeof(char*)*arg_mod.num_privs)
        == -EFAULT) {
        ERROR("problem copying privilege array from userspace\n");
        return -1;
    }

    for (i = 0; i < gm->num_privs; i++) {
        int len;
        char * tmp;
        if ((len = strlen_user((char __user*)gm->priv_array[i]) + 1) == -EFAULT) {
            ERROR("problem getting strlen from userspace\n");
            return -1;
        }

        tmp = palacios_alloc(len);
        if (!tmp) {
            ERROR("Problem allocating space for privilege name\n");
            return -1;
        }

        if (strncpy_from_user(tmp, (char __user *)gm->priv_array[i], MAX_MOD_NAME_LEN) == -EFAULT) {
            ERROR("problem copying privilege from userspace\n");
            return -1;
        }

        gm->priv_array[i] = tmp;
    }
    
    INFO("Registering Guarded Module with Palacios\n");
    ret = v3_register_gm(guest->v3_ctx,
                          gm->name,
                          gm->content_hash,
                          gm->hcall_offset,
                          gm->text_size,
                          gm->num_entries,
                          gm->num_privs,
                          gm->priv_array,
                          NULL,
                          (void*)gm->entry_points);

    if (!ret) {
        ERROR("palacios: could not register guarded module: %s\n", arg_mod.name);
        return -1;
    }

    arg_mod.id = ret;

    if (copy_to_user((void __user *)arg, &arg_mod, sizeof(struct v3_guard_mod))) {
        ERROR("palacios: error copying guarded module back to userspace\n");
        return -1;
    }

    kfree(gm->name);
    kfree(gm->content_hash);
    for (i = 0; i < gm->num_entries; i++) {
        kfree(gm->entry_points[i].name);
    }
    kfree(gm->entry_points);
    kfree(gm);
    return 0;
}


static int 
init_guard_mods (void) 
{
    return 0;
}


static int 
deinit_guard_mods (void) 
{
    return 0;
}


static int 
guest_init_guard_mods (struct v3_guest * guest, void ** vm_data) 
{
    add_guest_ctrl(guest, V3_VM_REGISTER_MOD, vm_register_mod, NULL);
    return 0;
}


static int 
guest_deinit_guard_mods (struct v3_guest * guest, void * vm_data) 
{
    return 0;
}


static struct linux_ext guard_mods_ext = {
    .name = "GUARDED_MODULES",
    .init = init_guard_mods,
    .deinit = deinit_guard_mods,
    .guest_init = guest_init_guard_mods,
    .guest_deinit = guest_deinit_guard_mods
};

register_extension(&guard_mods_ext);
