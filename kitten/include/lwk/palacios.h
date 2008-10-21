/* Copyright (c) 2007,2008 Sandia National Laboratories */

#ifndef _LWK_PALACIOS_H_
#define _LWK_PALACIOS_H_

#ifdef CONFIG_V3VEE

#include <palacios/vmm.h>

extern uint8_t rombios_start, rombios_end;
extern uint8_t vgabios_start, vgabios_end;

#endif // CONFIG_V3VEE

#endif
