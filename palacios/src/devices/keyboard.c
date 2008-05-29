#include <devices/keyboard.h>
#include <geekos/io.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>

#define KEYBOARD_DEBUG 1

#if KEYBOARD_DEBUG
#define KEYBOARD_DEBUG_PRINT(first, rest...) PrintDebug(first, ##rest)
#else
#define KEYBOARD_DEBUG_PRINT(first, rest...)
#endif


#define KEYBOARD_DATA_REG          0x60
#define KEYBOARD_CONTROL_REG       0x64

#define KEYBOARD_IRQ    0x1



// The currently targetted keyboard
static struct vm_device *thekeyboard=NULL;


struct keyboard_internal {
  // read* is what is seen when reads are done from the VM
  uchar_t read_status;
  uchar_t read_scancode;
  // write* is where we put the writes from the VM
  uchar_t write_status;
  uchar_t write_scancode;

  uchar_t status_byte;      //  for on-board uC

  uchar_t input_queue;      //  input queue is for communication *to* the on-board uC
  uint_t  input_queue_len;  //  num items queued
  uchar_t output_queue;     //  output queue is for communcation *from* the on-board uC
  uint_t  output_queue_len; //  num items queued 
};


static struct vm_device *demultiplex_injected_key(uchar_t status, uchar_t scancode)
{
  // this currently does nothing
  return thekeyboard;
}

int keyboard_interrupt(uint_t irq,struct vm_device * dev);

void deliver_key_to_vmm(uchar_t status, uchar_t scancode)
{
  struct vm_device *dev = demultiplex_injected_key(status,scancode);

  struct keyboard_internal *state = (struct keyboard_internal *)dev->private_data;

  KEYBOARD_DEBUG_PRINT("keyboard: injected status 0x%x, and scancode 0x%x\n", status,scancode);
  
  // This is wrong - the read status should be some combination of the 
  // status/scancode within the VM, and that of the actual device
  state->read_status=status;
  state->read_scancode=scancode;

  keyboard_interrupt(KEYBOARD_IRQ,dev);

}

int keyboard_reset_device(struct vm_device * dev)
{
  struct keyboard_internal *data = (struct keyboard_internal *) dev->private_data;
  
  memset(data,0,sizeof(struct keyboard_internal));
  
  KEYBOARD_DEBUG_PRINT("keyboard: reset device\n");
 
  return 0;

}





int keyboard_start_device(struct vm_device *dev)
{
  KEYBOARD_DEBUG_PRINT("keyboard: start device\n");
  return 0;
}


int keyboard_stop_device(struct vm_device *dev)
{
  KEYBOARD_DEBUG_PRINT("keyboard: stop device\n");
  return 0;
}




int keyboard_write_control_port(ushort_t port,
				void * src, 
				uint_t length,
				struct vm_device * dev)
{
  struct keyboard_internal *state = (struct keyboard_internal *) dev->private_data;

  if (length==1) { 
    uchar_t data = *((uchar_t*)src); 
    KEYBOARD_DEBUG_PRINT("keyboard: write of 0x%x to control port\n",data);
    state->write_status=data;
    return 1;
  } else {
    KEYBOARD_DEBUG_PRINT("keyboard: unknown size write to control port!\n");
    return -1;
  }
}

int keyboard_read_control_port(ushort_t port,
			       void * dest, 
			       uint_t length,
			       struct vm_device * dev)
{
  struct keyboard_internal *state = (struct keyboard_internal *) dev->private_data;

  if (length==1) { 
    uchar_t data;
    KEYBOARD_DEBUG_PRINT("keyboard: read from control port: ");
    data=state->read_status;
    KEYBOARD_DEBUG_PRINT("0x%x\n",data);
    memcpy(dest,&data,length);
    return 1;
  } else {
    KEYBOARD_DEBUG_PRINT("keyboard: unknown size read from control port!\n");
    return -1;
  }
}

int keyboard_write_data_port(ushort_t port,
			     void * src, 
			     uint_t length,
			     struct vm_device * dev)
{
  struct keyboard_internal *state = (struct keyboard_internal *) dev->private_data;

  if (length==1) { 
    uchar_t data = *((uchar_t*)src); 
    KEYBOARD_DEBUG_PRINT("keyboard: write of 0x%x to data port\n",data);
    state->write_scancode=data;
    return 1;
  } else {
    KEYBOARD_DEBUG_PRINT("keyboard: unknown size write to data port!\n");
    return -1;
  }
}

int keyboard_read_data_port(ushort_t port,
			    void * dest, 
			    uint_t length,
			    struct vm_device * dev)
{
  struct keyboard_internal *state = (struct keyboard_internal *) dev->private_data;

  if (length==1) { 
    uchar_t data;
    KEYBOARD_DEBUG_PRINT("keyboard: read from data port: ");
    data=state->read_scancode;
    KEYBOARD_DEBUG_PRINT("0x%x\n",data);
    memcpy(dest,&data,1);
    return 1;
  } else {
    KEYBOARD_DEBUG_PRINT("keyboard: unknown size read from data port!\n");
    return -1;
  }
}


int keyboard_interrupt(uint_t irq,
		       struct vm_device * dev) 
{
  KEYBOARD_DEBUG_PRINT("keyboard: interrupt\n");

  dev->vm->vm_ops.raise_irq(dev->vm,irq);

  return 0;

}


int keyboard_init_device(struct vm_device * dev) 
{
 
  //  struct keyboard_internal *data = (struct keyboard_internal *) dev->private_data;

  KEYBOARD_DEBUG_PRINT("keyboard: init_device\n");

  // Would read state here

  keyboard_reset_device(dev);

  // hook ports
  dev_hook_io(dev, KEYBOARD_DATA_REG, &keyboard_read_data_port, &keyboard_write_data_port);
  dev_hook_io(dev, KEYBOARD_CONTROL_REG, &keyboard_read_control_port, &keyboard_write_control_port);
  
  //
  // We do not hook the IRQ here.  Instead, the underlying device driver
  // is responsible to call us back
  // 

  return 0;
}

int keyboard_deinit_device(struct vm_device *dev)
{

  dev_unhook_io(dev, KEYBOARD_DATA_REG);
  dev_unhook_io(dev, KEYBOARD_CONTROL_REG);

  keyboard_reset_device(dev);
  return 0;
}





static struct vm_device_ops dev_ops = { 
  .init = keyboard_init_device, 
  .deinit = keyboard_deinit_device,
  .reset = keyboard_reset_device,
  .start = keyboard_start_device,
  .stop = keyboard_stop_device,
};




struct vm_device *create_keyboard() {
  
  if (thekeyboard!=NULL) { 
    KEYBOARD_DEBUG_PRINT("keyboard: creating >1 keyboard device.  This will probably fail!\n");
  }
  
  struct keyboard_internal * keyboard_state = (struct keyboard_internal *)V3_Malloc(sizeof(struct keyboard_internal));

  struct vm_device *device = create_device("KEYBOARD", &dev_ops, keyboard_state);

  thekeyboard=device;
  
  return device;
}
