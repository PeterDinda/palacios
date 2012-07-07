/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Kyle C. Hale <kh@u.norhtwestern.edu>
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_extensions.h>

#include <gears/process_environment.h>
#include <gears/execve_hook.h>
#include <gears/env_inject.h>

static struct v3_env_injects env_injects;

static int free_env_inject (struct v3_vm_info * vm, struct v3_env_inject_info * inject) {
    list_del(&(inject->inject_node)); 
    V3_Free(inject);
    return 0;
}

static int v3_env_inject_handler (struct guest_info * core, void * priv_data) {
    int i = 0;
    struct v3_env_inject_info * inject = (struct v3_env_inject_info*)priv_data;

    for (; i < inject->num_env_vars; i++) {
        PrintDebug("Envvar[%d]: %s\n", i, inject->env_vars[i]);
    }

    int ret = v3_inject_strings(core, (const char**)NULL, 
                                (const char**)inject->env_vars, 0, inject->num_env_vars);
    if (ret == -1) {
        PrintDebug("Error injecting strings in v3_env_inject_handler\n");
        return -1;
    }

    return 0;
}

static int init_env_inject (struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data) {
    struct v3_env_injects * injects = &env_injects;
    INIT_LIST_HEAD(&(injects->env_inject_list));
    return 0;
}


static int deinit_env_inject (struct v3_vm_info * vm, void * priv_data) {
    struct v3_env_injects * injects = &env_injects;
    struct v3_env_inject_info * inject = NULL;
    struct v3_env_inject_info * tmp = NULL;

    list_for_each_entry_safe(inject, tmp, &(injects->env_inject_list), inject_node) {
        free_env_inject(vm, inject);
    }

    return 0;
}


int v3_insert_env_inject (void * ginfo, char ** strings, int num_strings, char * bin_name) {
    struct v3_env_injects * injects = &env_injects;
    struct v3_env_inject_info * inject = V3_Malloc(sizeof(struct v3_env_inject_info));

    if (!inject) {
	PrintError("Cannot allocate in inserting environment inject\n");
	return -1;
    }


    memset(inject, 0, sizeof(struct v3_env_inject_info));

    inject->env_vars = strings;
    inject->num_env_vars = num_strings;
    inject->bin_name = bin_name;

    list_add(&(inject->inject_node), &(injects->env_inject_list));

    v3_hook_executable((struct v3_vm_info *)ginfo, bin_name, v3_env_inject_handler, (void*)inject);

    return 0;
}


int v3_remove_env_inject (struct v3_vm_info * vm, struct v3_env_inject_info * inject) {

    if (v3_unhook_executable(vm, inject->bin_name) < 0) {
        PrintError("Problem unhooking executable in v3_remove_env_inject\n");
        return -1;
    }

    free_env_inject(vm, inject);
    return 0;
}


static struct v3_extension_impl env_inject_impl = {
	.name = "env_inject",
	.init = init_env_inject,
	.deinit = deinit_env_inject,
	.core_init = NULL,
	.core_deinit = NULL,
	.on_entry = NULL,
	.on_exit = NULL
};

register_extension(&env_inject_impl);

