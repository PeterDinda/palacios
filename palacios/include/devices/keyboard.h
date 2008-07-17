#ifndef __KEYBOARD_H
#define __KEYBOARD_H

#include <palacios/vm_dev.h>

//
// The underlying driver needs to call this on each key that
// it wants to inject into the VMM for delivery to a VM
//
void deliver_key_to_vmm(uchar_t status, uchar_t scancode);
// And call this one each streaming mouse event
// that the VMM should deliver
void deliver_mouse_to_vmm(uchar_t mouse_packet[3]);

struct vm_device *create_keyboard();

#endif
