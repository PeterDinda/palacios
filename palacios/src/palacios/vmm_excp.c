/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_excp.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>

void v3_init_exception_state(struct guest_info * info) {
    info->excp_state.excp_pending = 0;
    info->excp_state.excp_num = 0;
    info->excp_state.excp_error_code = 0;

}



int v3_raise_exception_with_error(struct guest_info * info, uint_t excp, uint_t error_code) {
    struct v3_excp_state * excp_state = &(info->excp_state);

    if (excp_state->excp_pending == 0) {
	excp_state->excp_pending = 1;
	excp_state->excp_num = excp;
	excp_state->excp_error_code = error_code;
	excp_state->excp_error_code_valid = 1;
	//	PrintDebug("[v3_raise_exception_with_error] error code: %x\n", error_code);
    } else {
	PrintError("Error injecting exception_w_error (excp=%d) (error=%d) -- Exception (%d) (error=%d) already pending\n",
		   excp, error_code, excp_state->excp_num, excp_state->excp_error_code);
	return -1;
    }

    return 0;
}

int v3_raise_exception(struct guest_info * info, uint_t excp) {
    struct v3_excp_state * excp_state = &(info->excp_state);
    //PrintDebug("[v3_raise_exception]\n");
    if (excp_state->excp_pending == 0) {
	excp_state->excp_pending = 1;
	excp_state->excp_num = excp;
	excp_state->excp_error_code = 0;
	excp_state->excp_error_code_valid = 0;
    } else {
	PrintError("Error injecting exception (excp=%d) -- Exception (%d) (error=%d) already pending\n",
		   excp, excp_state->excp_num, excp_state->excp_error_code);
	return -1;
    }

    return 0;
}


int v3_excp_pending(struct guest_info * info) {
    struct v3_excp_state * excp_state = &(info->excp_state);
    
    if (excp_state->excp_pending == 1) {
	return 1;
    }

    return 0;
}


int v3_get_excp_number(struct guest_info * info) {
    struct v3_excp_state * excp_state = &(info->excp_state);

    if (excp_state->excp_pending == 1) {
	return excp_state->excp_num;
    }

    return 0;
}


int v3_injecting_excp(struct guest_info * info, uint_t excp) {
    struct v3_excp_state * excp_state = &(info->excp_state);
    
    excp_state->excp_pending = 0;
    excp_state->excp_num = 0;
    excp_state->excp_error_code = 0;
    excp_state->excp_error_code_valid = 0;
    
    return 0;
}
