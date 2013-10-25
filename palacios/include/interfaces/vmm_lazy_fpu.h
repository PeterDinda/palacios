/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2013, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_LAZY_FPU
#define __VMM_LAZY_FPU

#include <palacios/vmm_types.h>


struct v3_lazy_fpu_iface {

    // if these two are provided then lazy FP save/restore handled by host
    // indicate that the calling thread has used floating point
    void (*used_fpu)(void);
    // indicate that the calling thread wants to use floating point again
    void (*need_fpu)(void);

};


/*
 *  function prototypes
 */

extern void V3_Init_Lazy_FPU(struct v3_lazy_fpu_iface * palacios_lazy_fpu);

#ifdef __V3VEE__

#define V3_LAZY_FPU_USED()                                                  \
  do {							                    \
    extern struct v3_lazy_fpu_iface * palacios_lazy_fpu_hooks;              \
    if ((palacios_lazy_fpu_hooks) && (palacios_lazy_fpu_hooks)->used_fpu)         { \
      (palacios_lazy_fpu_hooks)->used_fpu();                                \
    }                                                                       \
  } while (0)

#define V3_LAZY_FPU_NEED()						    \
  do {							                    \
    extern struct v3_lazy_fpu_iface * palacios_lazy_fpu_hooks;		    \
    if ((palacios_lazy_fpu_hooks) && (palacios_lazy_fpu_hooks)->need_fpu)         { \
	(palacios_lazy_fpu_hooks)->need_fpu();                   	    \
    }						                            \
  } while (0)

#endif

#endif
