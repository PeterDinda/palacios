#ifndef __KEYBOARD_H
#define __KEYBOARD_H

#include <palacios/vm_dev.h>

//
// The underlying driver needs to call this on each key that
// it wants to inject into the VMM for delivery to a VM
//
void deliver_key_to_vmm(uchar_t status, uchar_t scancode);

struct vm_device *create_keyboard();

#endif
