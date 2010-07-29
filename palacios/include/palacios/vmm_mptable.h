/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Peter Dinda <pdinda@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_MPTABLE_H__
#define __VMM_MPTABLE_H__

/*
  This module is responsible for injecting an appropriate description of
  the multicore guest into the the guest's memory in the form
  of an Intel Multiprocessor Specification-compatible MP table. 

  The guest BIOS must cooperate in having preallocated space for the table
*/

#include <palacios/vm_guest.h>
#include <palacios/vmm_mem.h>
#include <palacios/vmm_types.h>

// Note that this must be run *after* the rombios has been mapped in
// AND the rombios needs to be COPIED in so that we can edit it
int v3_inject_mptable(struct v3_vm_info *vm);

#endif
