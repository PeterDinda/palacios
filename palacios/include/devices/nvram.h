#ifndef __NVRAM_H
#define __NVRAM_H

#include <palacios/vm_dev.h>

struct vm_device *create_nvram();

// The host os needs to call this
void deliver_timer_interrupt_to_vmm(uint_t period_us);

#endif
