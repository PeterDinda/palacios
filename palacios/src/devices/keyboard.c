#include <devices/keyboard.h>
#include <geekos/io.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>

extern struct vmm_os_hooks *os_hooks;

extern void SerialPrint(const char *format, ...);



#define KEYBOARD_DATA_REG          0x60
#define KEYBOARD_CONTROL_REG       0x64

#define KEYBOARD_IRQ    0x1



struct keyboard_internal {
  int x;
  // figure this out later - it should be the internal state of the keyboard buffer and
  // the emulated status, etc.
  // which should be fed from the underlying OS and drained via this interface
};




int keyboard_reset_device(struct vm_device * dev)
{
  //  struct keyboard_internal *data = (struct keyboard_internal *) dev->private_data;
  
  SerialPrint("keyboard: reset device\n");

 
  return 0;

}





int keyboard_start_device(struct vm_device *dev)
{
  SerialPrint("keyboard: start device\n");
  return 0;
}


int keyboard_stop_device(struct vm_device *dev)
{
  SerialPrint("keyboard: stop device\n");
  return 0;
}




int keyboard_write_control_port(ushort_t port,
				void * src, 
				uint_t length,
				struct vm_device * dev)
{
  //struct keyboard_internal *data = (struct keyboard_internal *) dev->private_data;

  if (length==1) { 
    uchar_t data = *((uchar_t*)src); 
    PrintDebug("keyboard: write of 0x%x to control port\n",data);
    Out_Byte(KEYBOARD_CONTROL_REG,data);
    return 1;
  } else {
    PrintDebug("keyboard: unknown size write to control port!\n");
    return -1;
  }
}

int keyboard_read_control_port(ushort_t port,
			       void * dest, 
			       uint_t length,
			       struct vm_device * dev)
{
  //struct keyboard_internal *data = (struct keyboard_internal *) dev->private_data;

  if (length==1) { 
    uchar_t data;
    PrintDebug("keyboard: read from control port: ");
    data=In_Byte(KEYBOARD_CONTROL_REG);
    PrintDebug("0x%x\n",data);
    memcpy(dest,&data,1);
    return 1;
  } else {
    PrintDebug("keyboard: unknown size read from control port!\n");
    return -1;
  }
}

int keyboard_write_data_port(ushort_t port,
			     void * src, 
			     uint_t length,
			     struct vm_device * dev)
{
  //struct keyboard_internal *data = (struct keyboard_internal *) dev->private_data;

  if (length==1) { 
    uchar_t data = *((uchar_t*)src); 
    PrintDebug("keyboard: write of 0x%x to data port\n",data);
    Out_Byte(KEYBOARD_DATA_REG,data);
    return 1;
  } else {
    PrintDebug("keyboard: unknown size write to data port!\n");
    return -1;
  }
}

int keyboard_read_data_port(ushort_t port,
			    void * dest, 
			    uint_t length,
			    struct vm_device * dev)
{
  //struct keyboard_internal *data = (struct keyboard_internal *) dev->private_data;

  if (length==1) { 
    uchar_t data;
    PrintDebug("keyboard: read from data port: ");
    data=In_Byte(KEYBOARD_DATA_REG);
    PrintDebug("0x%x\n",data);
    memcpy(dest,&data,1);
    return 1;
  } else {
    PrintDebug("keyboard: unknown size read from data port!\n");
    return -1;
  }
}


int keyboard_interrupt(uint_t irq,
		       struct vm_device * dev) 
{
  PrintDebug("keyboard: interrupt\n");
  return 0;
  // Inject ?
}


int keyboard_init_device(struct vm_device * dev) 
{
 
  //  struct keyboard_internal *data = (struct keyboard_internal *) dev->private_data;

  SerialPrint("keyboard: init_device\n");

  // Would read state here

  keyboard_reset_device(dev);

  // hook ports
  dev_hook_io(dev, KEYBOARD_DATA_REG, &keyboard_read_data_port, &keyboard_write_data_port);
  dev_hook_io(dev, KEYBOARD_CONTROL_REG, &keyboard_read_control_port, &keyboard_write_control_port);
  
  // hook irq
  // currently assume this is done in vm.c?
  //dev_hook_irq(dev,KEYBOARD_IRQ,&keyboard_interrupt);


  return 0;
}

int keyboard_deinit_device(struct vm_device *dev)
{


  //dev_unhook_irq(dev,KEYBOARD_IRQ);
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
  struct keyboard_internal * keyboard_state = os_hooks->malloc(sizeof(struct keyboard_internal));

  struct vm_device *device = create_device("KEYBOARD", &dev_ops, keyboard_state);

  return device;
}
