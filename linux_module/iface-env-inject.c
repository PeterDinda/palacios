/* 
 * Linux interface for guest-context environment variable injection
 *
 * (c) Kyle C. Hale 2012
 *
 */

#include <linux/uaccess.h>

#include <gears/env_inject.h>

#include "palacios.h"
#include "vm.h"
#include "linux-exts.h"
#include "iface-env-inject.h"


static struct env_data * env_map[MAX_ENV_INJECT] = {[0 ... MAX_ENV_INJECT - 1] = 0};


static int register_env(struct env_data * env) {
    int i;

    for (i = 0; i < MAX_ENV_INJECT; i++) {
        if (!env_map[i]) {
            env_map[i] = env;
            return i;
        }
    }
    return -1;
}


static void free_inject_data (void) {
    int i, j;

    for(i = 0; i < MAX_ENV_INJECT; i++) {
        if (env_map[i]) {
            for (j = 0; j < env_map[i]->num_strings; j++) {
                palacios_free(env_map[i]->strings[j]);
	    }
            palacios_free(env_map[i]->strings);
            palacios_free(env_map[i]);
        }
    }
}



static int vm_env_inject (struct v3_guest * guest, unsigned int cmd, unsigned long arg, void * priv_data) {
    struct env_data env_arg;
    struct env_data * env;
    int i;

    INFO("Palacios: Loading environment data...\n");
    if (copy_from_user(&env_arg, (void __user *)arg, sizeof(struct env_data))) {
        ERROR("palacios: error copying environment data from userspace\n");
        return -EFAULT;
    }

    env = palacios_alloc(sizeof(struct env_data));
    if (!env) {
        ERROR("Palacios Error: could not allocate space for environment data\n");
        return -EFAULT;
    }

    memset(env, 0, sizeof(struct env_data));

    env->num_strings = env_arg.num_strings;
    
    strcpy(env->bin_name, env_arg.bin_name);
    DEBUG("Binary hooked on: %s\n", env->bin_name);

    //DEBUG("Palacios: Allocating space for %u env var string ptrs...\n", env->num_strings);
    env->strings = palacios_alloc(env->num_strings*sizeof(char*));
    if (!(env->strings)) {
        ERROR("Palacios Error: could not allocate space for env var strings\n");
        return -EFAULT;
    }
    memset(env->strings, 0, env->num_strings*sizeof(char*));

    //INFO("Palacios: copying env var string pointers\n");
    if (copy_from_user(env->strings, (void __user *)env_arg.strings, env->num_strings*sizeof(char*))) {
        ERROR("Palacios: Error copying string pointers\n");
        return -EFAULT;
    }

    for (i = 0; i < env->num_strings; i++) {
        char * tmp  = palacios_alloc(MAX_STRING_LEN);
        if (!(tmp)) {
            ERROR("Palacios Error: could not allocate space for env var string #%d\n", i);
            return -EFAULT;
        }

        if (copy_from_user(tmp, (void __user *)env->strings[i], MAX_STRING_LEN)) {
            ERROR("Palacios: Error copying string #%d\n", i);
            return -EFAULT;
        }
        env->strings[i] = tmp;
    }

    INFO("Palacios: registering environment data...\n");
    if (register_env(env) < 0) 
        return -1;
    
    DEBUG("Palacios: passing data off to palacios...\n");
    if (v3_insert_env_inject(guest->v3_ctx, env->strings, env->num_strings, env->bin_name) < 0) {
        ERROR("Palacios: Error passing off environment data\n");
        return -1;
    }

    INFO("Palacios: environment injection registration complete\n");
    return 0;
}


static int init_env_inject (void) {
    return 0;
}


static int deinit_env_inject (void) {
    return 0;
}


static int guest_init_env_inject (struct v3_guest * guest, void ** vm_data) {
    add_guest_ctrl(guest, V3_VM_ENV_INJECT, vm_env_inject, NULL);
    return 0;
}


static int guest_deinit_env_inject (struct v3_guest * guest, void * vm_data) {
    free_inject_data();
    remove_guest_ctrl(guest, V3_VM_ENV_INJECT);
    return 0;
}


static struct linux_ext env_inject_ext = {
    .name = "ENV_INJECT",
    .init = init_env_inject,
    .deinit = deinit_env_inject,
    .guest_init = guest_init_env_inject,
    .guest_deinit = guest_deinit_env_inject 
};

register_extension(&env_inject_ext);
