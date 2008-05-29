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


extern struct vmm_os_hooks *os_hooks;

extern void SerialPrint(const char *format, ...);



#define KEYBOARD_60H          0x60  // keyboard microcontroller
#define KEYBOARD_64H          0x64  // onboard microcontroller

#define KEYBOARD_IRQ    0x1


// extract bits for status byte
#define STATUS_OUTPUT_BUFFER_FULL   0x01  // 1=full
#define STATUS_INPUT_BUFFER_FULL    0x02  // 1=full
#define STATUS_SYSTEM               0x04  // 1=self-test-passed
#define STATUS_COMMAND_DATA_AVAIL   0x08  // internal: 0=data on 60h, 0=cmd on 64h
#define STATUS_ENABLED              0x10  // 1=keyboard is enabled
#define STATUS_MOUSE_BUFFER_FULL    0x20  // 1= mouse output buffer full
#define STATUS_TIMEOUT              0x40  // 1=timeout of keybd
#define STATUS_PARITY               0x80  // 1=parity error

// bits for cmd byte

#define CMD_INTR                0x01  // 1=interrupts enabled
#define CMD_MOUSE_INTR          0x02  // 1=interrupts enabled for mouse
#define CMD_SYSTEM              0x04  // 1= self test passed
#define CMD_OVERRIDE            0x08  // FORCE 0 for  PS2
#define CMD_DISABLE             0x10  // 1=disabled keyboard
#define CMD_MOUSE_DISABLE       0x20  // 1=disabled mouse
#define CMD_SCANCODE_XLATE      0x40  // 1=translate to set 1 scancodes
#define CMD_RESERVED            0x80  // should be zero

// The currently targetted keyboard
static struct vm_device *thekeyboard=NULL;


struct keyboard_internal {
  // 
  // 0x60 is the port for the keyboard microcontroller
  //   writes are commands
  //   reads from it usually return scancodes
  //   however, it can also return other data 
  //   depending on the state of the onboard microcontroller
  //
  // 0x64 is the port for the onboard microcontroller
  //   writes are commands
  //   reads are status
  //

  // state of the onboard microcontroller
  // this is needed because sometimes 0x60 reads come
  // from the onboard microcontroller
  enum {NORMAL,
	// after receiving cmd 0x20 
	// keyboard uC cmd byte pushed onto output queue
	WRITING_CMD_BYTE,  
	// after receiving cmd 0x60
	// keybaord uC cmd will subsequently arive on data port
	TRANSMIT_PASSWD,
	// after recieving 0xa5
	// password arrives on data port, null terminated
  } state;
	


  uchar_t cmd_byte;         //  for keyboard uC - read/written 
                            //     via read/write cmd byte command
  uchar_t status_byte;      //  for on-board uC - read via 64h

  uchar_t input_queue;      //  Read via 60h
  uint_t  input_queue_len;  //  num items queued
  uchar_t output_queue;     //  Written by 60h
  uint_t  output_queue_len; //  num items queued 
};


// 
// push item onto outputqueue, optionally overwriting if there is no room
// returns 0 if successful
//
static int PushToOutputQueue(struct vm_device *dev, uchar_t value, uchar_t overwrite) 
{
  struct keyboard_internal *state = (struct keyboard_internal *)dev->private_data;
  
  if (state->output_queue_len==0 || overwrite) { 
    state->output_queue=value;
    state->output_queue_len=1;
    state->status_byte |= STATUS_OUTPUT_BUFFER_FULL;
    return 0;
  } else {
    KEYBOARD_DEBUG_PRINT("keyboard: PushToOutputQueue Failed - Queue Full\n");
    return -1;
  }
}

#if 0
// 
// pull item from outputqueue 
// returns 0 if successful
//
static int PullFromOutputQueue(struct vm_device *dev,uchar_t *value) 
{
  struct keyboard_internal *state = (struct keyboard_internal *)dev->private_data;
  if (state->output_queue_len==1) { 
    *value=state->input_queue;
    state->output_queue_len=0;
    state->status_byte &= ~STATUS_OUTPUT_BUFFER_FULL;
    return 0;
  } else {
    KEYBOARD_DEBUG_PRINT("keyboard: PullFromOutputQueue Failed - Queue Empty\n");
    return -1;
  }
}
#endif

// 
// push item onto inputqueue, optionally overwriting if there is no room
// returns 0 if successful
//
static int PushToInputQueue(struct vm_device *dev, uchar_t value, uchar_t overwrite) 
{
  struct keyboard_internal *state = (struct keyboard_internal *)dev->private_data;
  if (state->input_queue_len==0 || overwrite) { 
    state->input_queue=value;
    state->input_queue_len=1;
    state->status_byte |= STATUS_INPUT_BUFFER_FULL;
    return 0;
  } else {
    KEYBOARD_DEBUG_PRINT("keyboard: PushToOutputQueue Failed - Queue Full\n");
    return -1;
  }
}

// 
// pull item from inputqueue 
// returns 0 if successful
//
static int PullFromInputQueue(struct vm_device *dev, uchar_t *value) 
{
  struct keyboard_internal *state = (struct keyboard_internal *)dev->private_data;
  if (state->input_queue_len==1) { 
    *value=state->input_queue;
    state->input_queue_len=0;
    state->status_byte &=~STATUS_INPUT_BUFFER_FULL;
    return 0;
  } else {
    KEYBOARD_DEBUG_PRINT("keyboard: PullFromInputQueue Failed - Queue Empty\n");
    return -1;
  }
}

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
  
  if (    (state->status_byte & STATUS_ENABLED)      // onboard is enabled
	  && (!(state->cmd_byte & CMD_DISABLE)))  {   // keyboard is enabled

    PushToInputQueue(dev,scancode,1);

    if (state->cmd_byte & CMD_INTR) { 
      keyboard_interrupt(KEYBOARD_IRQ,dev);
    }
	
  }
}

int keyboard_reset_device(struct vm_device * dev)
{
  struct keyboard_internal *data = (struct keyboard_internal *) dev->private_data;
  
  memset(data,0,sizeof(struct keyboard_internal));

  data->cmd_byte =   
      CMD_INTR        // interrupts on
    | CMD_MOUSE_INTR  // mouse interupts on
    | CMD_SYSTEM ;    // self test passed
                      // PS2, keyboard+mouse enabled, generic translation    
  
  data->status_byte = 
      STATUS_SYSTEM     // self-tests passed
    | STATUS_ENABLED ;  // keyboard ready
                        // buffers empty, no errors

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




int keyboard_write_command(ushort_t port,
			   void * src, 
			   uint_t length,
			   struct vm_device * dev)
{
  struct keyboard_internal *state = (struct keyboard_internal *) dev->private_data;
  uchar_t cmd;

  // Should always be single byte write

  if (length!=1) { 
    KEYBOARD_DEBUG_PRINT("keyboard: write of >1 bytes to 64h");
    return -1;
  }

  cmd =  *((uchar_t*)src); 

  if (state->state!=NORMAL) { 
    KEYBOARD_DEBUG_PRINT("keyboard: warning - receiving command on 64h but state!=NORMAL\n");
  }
  
  KEYBOARD_DEBUG_PRINT("keyboard: command 0x%x on 64h\n",cmd);

  switch (cmd) { 

  case 0x20:  // READ COMMAND BYTE (returned in 60h)
    PushToInputQueue(dev,state->cmd_byte,1);
    state->state=NORMAL;  // the next read on 0x60 will get the right data
    break;

  case 0x60:  // WRITE COMMAND BYTE (read from 60h)
    state->state=WRITING_CMD_BYTE; // we need to make sure we send the next 0x60 byte appropriately
    break;

  // case 0x90-9f - write to output port  (?)

  case 0xa1: // Get version number
    PushToInputQueue(dev,0,1);
    state->state=NORMAL;
    break;

  case 0xa4:  // is password installed?  send result to 0x60
    // we don't support passwords
    PushToOutputQueue(dev,0xf1,1);
    state->state=NORMAL;
    break;

  case 0xa5:  // new password will arrive on 0x60
    state->state=TRANSMIT_PASSWD;
    break;

  case 0xa6:  // check passwd;
    // since we do not support passwords, we will simply ignore this
    // the implication is that any password check immediately succeeds 
    // with a blank password
    state->state=NORMAL;
    break;

  case 0xa7:  // disable mouse
    state->cmd_byte |= CMD_MOUSE_DISABLE;
    state->state=NORMAL;
    break;

  case 0xa8:  // enable mouse
    state->cmd_byte &= ~CMD_MOUSE_DISABLE;
    state->state=NORMAL;
    break;

  case 0xa9:  // mouse interface test  (always succeeds)
    PushToInputQueue(dev,0,1);
    state->state=NORMAL;
    break;

  case 0xaa:  // controller self test (always succeeds)
    PushToInputQueue(dev,0x55,1);
    state->state=NORMAL;
    break;

  case 0xab:  // keyboard interface test (always succeeds)
    PushToInputQueue(dev,0,1);
    state->state=NORMAL;
    break;

  case 0xad:  // disable keyboard
    state->cmd_byte |= CMD_DISABLE;
    state->state=NORMAL;
    break;

  case 0xae:  // enable keyboard
    state->cmd_byte &= ~CMD_DISABLE;
    state->state=NORMAL;
    break;

  case 0xaf:  // get version
    PushToInputQueue(dev,0x00,1);
    state->state=NORMAL;
    break;

  // case c0  read input port ?
  // case c1  copy input port lsn to status
  // case c2  copy input port msn to status
  
  // case d0 read output port
  // case d1 write output port
  // case d2 write keyboard buffer (inject key)
  // case d3 write mouse buffer (inject mouse)
  // case d4 write mouse device (command to mouse?)

  // case e0 read test port
  
  // case f0..ff pulse output port ?

   
  default:
    KEYBOARD_DEBUG_PRINT("keyboard: ignoring command (unimplemented)\n");
    state->state=NORMAL;
    break;
  }

  return 1;

}

int keyboard_read_status(ushort_t port,
			 void * dest, 
			 uint_t length,
			 struct vm_device * dev)
{
  struct keyboard_internal *state = (struct keyboard_internal *) dev->private_data;

  if (length==1) { 
    KEYBOARD_DEBUG_PRINT("keyboard: read status (64h): ");
    *((uchar_t*)dest)=state->status_byte;
    KEYBOARD_DEBUG_PRINT("0x%x\n",*((uchar_t*)dest));
    return 1;
  } else {
    KEYBOARD_DEBUG_PRINT("keyboard: >1 byte read for status (64h)\n");
    return -1;
  }
}

int keyboard_write_output(ushort_t port,
			  void * src, 
			  uint_t length,
			  struct vm_device * dev)
{
  struct keyboard_internal *state = (struct keyboard_internal *) dev->private_data;

  if (length!=1) { 
    KEYBOARD_DEBUG_PRINT("keyboard: write of 60h with >1 byte\n");
    return -1;
  }

  uchar_t data=*((uchar_t*)src);
  
  switch (state->state) {
  case WRITING_CMD_BYTE:
    state->cmd_byte=data;
    state->state=NORMAL;
    break;
  case TRANSMIT_PASSWD:
    if (data) {
      //ignore passwd
    } else {
      // end of password
      state->state=NORMAL;
    }
    break;
  case NORMAL:
    // command is being sent to keyboard controller
    switch (data) { 
    case 0xff: // reset
      PushToInputQueue(dev,0xfa,1); // ack
      state->state=NORMAL;
      break;
    case 0xfe: // resend
    case 0xfd: // set key type make
    case 0xfc: // set key typ make/break
    case 0xfb: // set key type typematic
    case 0xfa: // set all typematic make/break/typematic
    case 0xf9: // set all make
    case 0xf8: // set all make/break
    case 0xf7: // set all typemaktic
    case 0xf6: // set defaults
    case 0xf5: // disable scanning
    case 0xf4: // enable scanning
    case 0xf3: // set typematic delay/rate
    default:
      KEYBOARD_DEBUG_PRINT("keyboard: unhandled command 0x%x on output buffer (60h)\n");
      break;
    }
  default:
    KEYBOARD_DEBUG_PRINT("keyboard: unknown state %x on command 0x%x on output buffer (60h)\n",state->state, data);
  }
  
  return 1;
}

int keyboard_read_input(ushort_t port,
			void * dest, 
			uint_t length,
			struct vm_device * dev)
{
  if (length==1) { 
    uchar_t data;
    KEYBOARD_DEBUG_PRINT("keyboard: read from input (60h): ");
    PullFromInputQueue(dev,&data);
    KEYBOARD_DEBUG_PRINT("0x%x\n",data);
    *((uchar_t*)dest)=data;
    return 1;
  } else {
    KEYBOARD_DEBUG_PRINT("keyboard: unknown size read from input (60h)\n");
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
  dev_hook_io(dev, KEYBOARD_64H, &keyboard_read_status, &keyboard_write_command);
  dev_hook_io(dev, KEYBOARD_60H, &keyboard_read_input, &keyboard_write_output);
  
  //
  // We do not hook the IRQ here.  Instead, the underlying device driver
  // is responsible to call us back
  // 

  return 0;
}

int keyboard_deinit_device(struct vm_device *dev)
{

  dev_unhook_io(dev, KEYBOARD_60H);
  dev_unhook_io(dev, KEYBOARD_64H);
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
