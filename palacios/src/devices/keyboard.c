/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <devices/keyboard.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>


#ifndef DEBUG_KEYBOARD
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define KEYBOARD_DEBUG_80H   0



#define KEYBOARD_60H          0x60  // keyboard microcontroller
#define KEYBOARD_64H          0x64  // onboard microcontroller

#define KEYBOARD_DELAY_80H    0x80  // written for timing

#define KEYBOARD_IRQ          0x1
#define MOUSE_IRQ             0xc   


// extract bits for status byte
#define STATUS_OUTPUT_BUFFER_FULL   0x01  // 1=full (data for system)
#define STATUS_INPUT_BUFFER_FULL    0x02  // 1=full (data for 8042)
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

// bits for the output port 


#define OUTPUT_RESET        0x01  // System reset on 0
#define OUTPUT_A20          0x02  // A20 gate (1= A20 is gated)
#define OUTPUT_RES1         0x04  // reserved
#define OUTPUT_RES2         0x08  // reserved
#define OUTPUT_OUTPUT_FULL  0x10  // output buffer full
#define OUTPUT_INPUT_EMPTY  0x20  // input buffer empty
#define OUTPUT_KBD_CLOCK    0x40  // keyboard clock (?)
#define OUTPUT_KBD_DATA     0x80  // keyboard data

// bits for the input port

#define INPUT_RES0          0x01  // reserved
#define INPUT_RES1          0x02  // reserved
#define INPUT_RES2          0x04  // reserved
#define INPUT_RES3          0x08  // reserved
#define INPUT_RAM           0x10  // set to 1 if RAM exists?
#define INPUT_JUMPER        0x20  // manufacturing jumper?
#define INPUT_DISPLAY       0x40  // 0=color, 1=mono
#define INPUT_KBD_INHIBIT   0x80  // 1=inhibit keyboard ?


// for queue operations
#define QUEUE               0
#define OVERWRITE           1

// for queue operations - whether it's data or cmd waiting on 60h
#define DATA                0
#define COMMAND             1

// for queue operations - whether this is keyboard or mouse data on 60h
#define KEYBOARD            0
#define MOUSE               1



//#define QUEUE_SIZE          32


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
    enum {// Normal mode measn we deliver keys
        // to the vm and accept commands from it
        NORMAL,
	// after receiving cmd 0x60
	// keybaord uC cmd will subsequently arrive
	WRITING_CMD_BYTE,  
	// after recieving 0xa5
	// password arrives on data port, null terminated
	TRANSMIT_PASSWD,
	// after having reset sent to 0x60
	// we immediately ack, and then
	// push BAT success (0xaa) after the ack
	RESET,
        // after having a d1 sent to 64
	// we wait for a new output byte on 60
	WRITING_OUTPUT_PORT,
	// after having a d2 sent to 64
	// we wait for a new output byte on 60
	// then make it available as a keystroke
	INJECTING_KEY,
	// after having a d3 sent to 64
	// we wait for a new output byte on 60
	// then make it available as a mouse event
	INJECTING_MOUSE,
        // after having a d4 sent to 64
	// we wait for a new output byte on 60
	// then send it to the mouse
	IN_MOUSE,
	// After the Keyboard LEDs are enabled
	// we wait for the output byte on 64?
	SET_LEDS,
	// After the Keyboard SET_RATE is called
	// we wait for the output byte on 64?
	SET_RATE,
	// The keyboard requests an ID which 
	// generates 3 bytes of output...
	KBD_ID1,
	KBD_ID2,
	
	
    } state;


    enum {
	// after receiving a mouse command 0f 0xff
	// we return the ack and then the next thing will be the 
	// bat code (aa - success)
	RESET1,
	// followed by the device id (00 - mouse)
	RESET2, 
	// Then it goes into stream mode
	STREAM1,  //
	STREAM2,  //
	STREAM3,  // for each of the following bytes in mouse_packet
	// this is used for setting sample rate
	SAMPLE1,  
	// this is used for getting device id
	DEVICE1, 
	// just like the stream moes
	REMOTE1,
	REMOTE2,
	REMOTE3,
	// For getting status info
	STATUS1,
	STATUS2,
	STATUS3, 
	// set resolution
	SETRES1,
    } mouse_state;


    uchar_t wrap;             
    uchar_t mouse_packet[3];  // byte 1: y over, xover, y sign, x sign, 1, middle, right, left
    // byte 2: x movement
    // byte 3: y movement

    uchar_t mouse_needs_ack;  //
    uchar_t mouse_done_after_ack; 

    uchar_t cmd_byte;         //  for keyboard uC - read/written 
    //     via read/write cmd byte command
    uchar_t status_byte;      //  for on-board uC - read via 64h

    uchar_t output_byte;      //  output port of onboard uC (e.g. A20)

    uchar_t input_byte;       //  input port of onboard uC

    // Data for system
    uchar_t output_queue;     //  
    uint_t  output_queue_len; //  
    //uint_t  output_queue_read;
    //uint_t  output_queue_write;


};


static int keyboard_interrupt(struct vm_device * dev, uint_t irq) {
    struct keyboard_internal *state = (struct keyboard_internal *)(dev->private_data);

    PrintDebug("keyboard: interrupt 0x%x\n", irq);

    if (state->cmd_byte & CMD_INTR) { 
	v3_raise_irq(dev->vm, irq);
    }

    return 0;
}



// 
// push item onto outputqueue, optionally overwriting if there is no room
// returns 0 if successful
//
static int PushToOutputQueue(struct vm_device * dev, uchar_t value, uchar_t overwrite, uchar_t cmd, uchar_t mouse) 
{
    struct keyboard_internal *state = (struct keyboard_internal *)(dev->private_data);
  
    if ((state->output_queue_len == 0) || overwrite) { 
    
	state->output_queue = value;
	state->output_queue_len = 1;
    
	if (cmd) {
	    state->status_byte |= STATUS_COMMAND_DATA_AVAIL;
	} else {
	    state->status_byte &= ~STATUS_COMMAND_DATA_AVAIL;
	}
    
	if (mouse) { 
	    state->status_byte |= STATUS_MOUSE_BUFFER_FULL;
	    keyboard_interrupt(dev, MOUSE_IRQ);
	} else {
	    keyboard_interrupt(dev, KEYBOARD_IRQ);
	}

	state->status_byte |= STATUS_OUTPUT_BUFFER_FULL;

	return 0;

    } else {
	PrintError("keyboard: PushToOutputQueue Failed - Queue Full\n");
	return -1;
    }
}


// 
// pull item from outputqueue 
// returns 0 if successful
//
static int PullFromOutputQueue(struct vm_device * dev, uchar_t * value) 
{
    struct keyboard_internal *state = (struct keyboard_internal *)(dev->private_data);

    if (1 || (state->output_queue_len == 1)) { 

	*value = state->output_queue;
	state->output_queue_len = 0;
    
	if (state->status_byte & STATUS_OUTPUT_BUFFER_FULL) { 
	    state->status_byte &= ~STATUS_OUTPUT_BUFFER_FULL;
	} 
    
	if (state->status_byte & STATUS_MOUSE_BUFFER_FULL) { 
	    state->status_byte &= ~STATUS_MOUSE_BUFFER_FULL;
	}
    
	if (state->status_byte & STATUS_COMMAND_DATA_AVAIL) { 
	    state->status_byte &= ~STATUS_COMMAND_DATA_AVAIL;
	} // reset to data
    
    
	return 0;
    } else {
	PrintError("keyboard: PullFromOutputQueue Failed - Queue Empty\n");
	return -1;
    }
}




static int key_event_handler(struct guest_info * info, 
			     struct v3_keyboard_event * evt, 
			     void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct keyboard_internal *state = (struct keyboard_internal *)(dev->private_data);

    PrintDebug("keyboard: injected status 0x%x, and scancode 0x%x\n", evt->status, evt->scan_code);


    if (evt->scan_code == 0x44) { // F10 debug dump
	v3_print_guest_state(info);
	//	PrintGuestPageTables(info, info->shdw_pg_state.guest_cr3);
    }

    if ( (state->status_byte & STATUS_ENABLED)      // onboard is enabled
	 && (!(state->cmd_byte & CMD_DISABLE)) )  {   // keyboard is enabled
    
	PushToOutputQueue(dev, evt->scan_code, OVERWRITE, DATA, KEYBOARD);
    }
  
    return 0;
}


static int mouse_event_handler(struct guest_info * info, 
			       struct v3_mouse_event * evt, 
			       void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct keyboard_internal * state = (struct keyboard_internal *)(dev->private_data);

    PrintDebug("keyboard: injected mouse packet 0x %x %x %x\n",
	       evt->data[0], evt->data[1], evt->data[2]);
  
    memcpy(state->mouse_packet, evt->data, 3);
  
    state->status_byte |= STATUS_MOUSE_BUFFER_FULL;
  
    
    switch (state->mouse_state) { 
	case STREAM1:
	case STREAM2:
	case STREAM3:
	    if (!(state->cmd_byte & CMD_MOUSE_DISABLE)) { 
		keyboard_interrupt(dev, MOUSE_IRQ);
	    }
	    break;
	default:
	    return -1;
	    break;
    }

    return 0;
}


static int keyboard_reset_device(struct vm_device * dev)
{
    struct keyboard_internal *data = (struct keyboard_internal *)(dev->private_data);
  
    memset(data, 0, sizeof(struct keyboard_internal));

    data->state = NORMAL;
    data->mouse_state = STREAM1;

    data->cmd_byte =  
	CMD_INTR          // interrupts on
	| CMD_MOUSE_INTR  // mouse interupts on
	| CMD_SYSTEM ;    // self test passed
    // PS2, keyboard+mouse enabled, generic translation    
  
    data->status_byte = 
	STATUS_SYSTEM       // self-tests passed
	| STATUS_ENABLED ;  // keyboard ready
    // buffers empty, no errors

    data->output_byte = 0;  //  ?

    data->input_byte = INPUT_RAM;  // we have some
    // also display=color, jumper 0, keyboard enabled 

  

    PrintDebug("keyboard: reset device\n");
 
    return 0;

}



static int keyboard_start_device(struct vm_device *dev)
{
    PrintDebug("keyboard: start device\n");
    return 0;
}


static int keyboard_stop_device(struct vm_device *dev)
{
    PrintDebug("keyboard: stop device\n");
    return 0;
}


static int mouse_read_input(struct vm_device *dev)
{
    struct keyboard_internal *state = (struct keyboard_internal *)(dev->private_data);

    if (state->mouse_needs_ack) { 
	state->mouse_needs_ack = 0;

	// the ack has been stuffed previously
	if (state->mouse_done_after_ack) { 
	    return 1;
	} else {
	    return 0;
	}
    }

    switch (state->mouse_state) { 

	case RESET1: // requesting the BAT code
	    PushToOutputQueue(dev, 0xaa, OVERWRITE, DATA, MOUSE) ;  // BAT successful
	    PrintDebug(" mouse sent BAT code (sucesssful) ");
	    state->mouse_state = RESET2;

	    return 0;  // not done with mouse processing yet
	    break;

	case RESET2: // requesting the device id
	    PushToOutputQueue(dev, 0x00, OVERWRITE, DATA, MOUSE) ;  // normal mouse type
	    PrintDebug(" mouse sent device id ");
	    state->mouse_state = STREAM1;

	    return 1;  // done with mouse processing 
	    break;

	case STREAM1: // send data
	    PushToOutputQueue(dev, state->mouse_packet[0], OVERWRITE, DATA, MOUSE); 
	    PrintDebug(" mouse sent stream data1 ");
	    state->mouse_state = STREAM2;

	    return 0;
	    break;

	case STREAM2: // send data
	    PushToOutputQueue(dev, state->mouse_packet[1], OVERWRITE, DATA, MOUSE); 
	    PrintDebug(" mouse sent stream data2 ");
	    state->mouse_state = STREAM3;

	    return 0;
	    break;

	case STREAM3: // send data
	    PushToOutputQueue(dev, state->mouse_packet[2], OVERWRITE, DATA, MOUSE); 
	    PrintDebug(" mouse sent stream data3 ");
	    state->mouse_state = STREAM1;

	    return 1; // now done
	    break;

	case REMOTE1: // send data
	    PushToOutputQueue(dev, state->mouse_packet[0], OVERWRITE, DATA, MOUSE); 
	    PrintDebug(" mouse sent remote data1 ");
	    state->mouse_state = REMOTE2;

	    return 0;
	    break;

	case REMOTE2: // send data
	    PushToOutputQueue(dev, state->mouse_packet[1], OVERWRITE, DATA, MOUSE); 
	    PrintDebug(" mouse sent remote data2 ");
	    state->mouse_state = REMOTE3;

	    return 0;
	    break;

	case REMOTE3: // send data
	    PushToOutputQueue(dev, state->mouse_packet[2], OVERWRITE, DATA, MOUSE); 
	    PrintDebug(" mouse sent remote data3 ");
	    state->mouse_state = REMOTE1;

	    return 1; // now done
	    break;

	case STATUS1: // send data
	    PushToOutputQueue(dev, 0x0, OVERWRITE, DATA, MOUSE); 
	    PrintDebug(" mouse sent status data1 ");
	    state->mouse_state = STATUS2;

	    return 0;
	    break;

	case STATUS2: // send data
	    PushToOutputQueue(dev, 0x0, OVERWRITE, DATA, MOUSE); 
	    PrintDebug(" mouse sent status data2 ");
	    state->mouse_state = STATUS3;

	    return 0;
	    break;

	case STATUS3: // send data
	    PushToOutputQueue(dev, 0x0, OVERWRITE, DATA, MOUSE); 
	    PrintDebug(" mouse sent status data3 ");
	    state->mouse_state = STREAM1;

	    return 1; // now done
	    break;

	case DEVICE1: // send device id
	    PushToOutputQueue(dev, 0x0, OVERWRITE, DATA, MOUSE); 
	    PrintDebug(" mouse sent device id ");
	    state->mouse_state = STREAM1;

	    return 1; // now done
	    break;

	default:
	    PrintDebug(" mouse has no data ");
	    return 1; // done
	    break;
    }
}

static int mouse_write_output(struct vm_device * dev, uchar_t data)
{
    struct keyboard_internal * state = (struct keyboard_internal *)(dev->private_data);

    switch (state->mouse_state) { 
	case STREAM1:
	case STREAM2:
	case STREAM3:
	case REMOTE1:
	case REMOTE2:
	case REMOTE3:
	    switch (data) {

		case 0xff: //reset
		    PushToOutputQueue(dev, 0xfe, OVERWRITE, DATA, MOUSE) ;   // no mouse!
		    PrintDebug(" mouse reset begins (no mouse) ");

		    return 1;  // not done;
		    break;

		    /*
		      case 0xff: //reset
		      PushToOutputQueue(dev,0xfa,OVERWRITE,DATA,MOUSE) ; 
		      PrintDebug(" mouse reset begins ");
		      state->mouse_done_after_ack=0;
		      state->mouse_needs_ack=1;
		      state->mouse_state=RESET1;
		      return 0;  // not done;
		      break;
		    */
		case 0xfe: //resend
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    PrintDebug(" mouse resend begins ");
		    state->mouse_done_after_ack = 0;
		    state->mouse_needs_ack = 0;
		    state->mouse_state = STREAM1;
		    return 0;  // not done
		    break;
      
		case 0xf6: // set defaults
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    PrintDebug(" mouse set defaults ");
		    state->mouse_done_after_ack = 1;
		    state->mouse_needs_ack = 1;
		    state->mouse_state = STREAM1;
		    return 0; // not done
		    break;
      
		case 0xf5: // disable data reporting 
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    state->mouse_done_after_ack = 1;
		    state->mouse_needs_ack = 1;
		    PrintDebug(" mouse disable data reporting ");
		    state->mouse_state = STREAM1;
		    return 0; // not done
		    break;
      
		case 0xf4: // enable data reporting 
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    state->mouse_done_after_ack = 1;
		    state->mouse_needs_ack = 1;
		    PrintDebug(" mouse enable data reporting ");
		    state->mouse_state = STREAM1;
		    return 0; // not done
		    break;
      
		case 0xf3: // set sample rate
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    state->mouse_done_after_ack = 0;
		    state->mouse_needs_ack = 1;
		    PrintDebug(" mouse set sample rate begins ");
		    state->mouse_state = SAMPLE1;
		    return 0; // not done
		    break;
      
		case 0xf2: // get device id
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    state->mouse_done_after_ack = 0;
		    state->mouse_needs_ack = 1;
		    PrintDebug(" mouse get device id begins ");
		    state->mouse_state = DEVICE1;
		    return 0; // not done
		    break;
      
		case 0xf0: // set remote mode
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    state->mouse_done_after_ack = 1;
		    state->mouse_needs_ack = 1;
		    PrintDebug(" mouse set remote mode  ");
		    state->mouse_state = REMOTE1;
		    return 0; // not done
		    break;

		case 0xee: // set wrap mode
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    state->mouse_done_after_ack = 1;
		    state->mouse_needs_ack = 1;
		    PrintDebug(" mouse set wrap mode (ignored)  ");
		    state->mouse_state = STREAM1;
		    return 0; // not done
		    break;

		case 0xec: // reset wrap mode
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    state->mouse_done_after_ack = 1;
		    state->mouse_needs_ack = 1;
		    PrintDebug(" mouse reset wrap mode (ignored)  ");
		    state->mouse_state = STREAM1;
		    return 0; // done
		    break;

		case 0xeb: // read data
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    state->mouse_done_after_ack = 0;
		    state->mouse_needs_ack = 1;
		    PrintDebug(" mouse switch to wrap mode (ignored)  ");
		    state->mouse_state = REMOTE1;
		    return 0; // not done
		    break;
      
		case 0xea: // set stream mode
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    state->mouse_done_after_ack = 1;
		    state->mouse_needs_ack = 1;
		    PrintDebug(" mouse set stream mode  ");
		    state->mouse_state = STREAM1;
		    return 0; // not done
		    break;

		case 0xe9: // status request
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    state->mouse_done_after_ack = 0;
		    state->mouse_needs_ack = 1;
		    PrintDebug(" mouse status request begins  ");
		    state->mouse_state = STATUS1;
		    return 0; // notdone
		    break;

		case 0xe8: // set resolution
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    state->mouse_done_after_ack = 0;
		    state->mouse_needs_ack = 1;
		    PrintDebug(" mouse set resolution begins  ");
		    state->mouse_state = SETRES1;
		    return 0; // notdone
		    break;

		case 0xe7: // set scaling 2:1
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    state->mouse_done_after_ack = 1;
		    state->mouse_needs_ack = 1;
		    PrintDebug(" mouse set scaling 2:1 ");
		    state->mouse_state = STREAM1;
		    return 0; // not done
		    break;

		case 0xe6: // set scaling 1:1
		    PushToOutputQueue(dev, 0xfa, OVERWRITE, DATA, MOUSE) ; 
		    state->mouse_done_after_ack = 1;
		    state->mouse_needs_ack = 1;
		    PrintDebug(" mouse set scaling 1:1 ");
		    state->mouse_state = STREAM1;
		    return 0; // done
		    break;
      
		default:
		    PrintDebug(" receiving unknown mouse command (0x%x) in acceptable state ", data);
		    return 1; // done
		    break;

	    }
    
	default:
	    PrintDebug(" receiving mouse output in unhandled state (0x%x) ", state->mouse_state);
	    break;
	    return 1; // done?
	    break;
    }

    PrintDebug(" HUH? ");
    return 1; // done
}



#if KEYBOARD_DEBUG_80H
static int keyboard_write_delay(ushort_t port,
				void * src, 
				uint_t length,
				struct vm_device * dev)
{

    if (length == 1) { 
	PrintDebug("keyboard: write of 0x%x to 80h\n", *((uchar_t*)src));

	return 1;
    } else {
	PrintDebug("keyboard: write of >1 byte to 80h\n", *((uchar_t*)src));

	return length;
    }
}

static int keyboard_read_delay(ushort_t port,
			       void * dest, 
			       uint_t length,
			       struct vm_device * dev)
{

    if (length == 1) { 
	*((uchar_t*)dest) = v3_inb(port);

	PrintDebug("keyboard: read of 0x%x from 80h\n", *((uchar_t*)dest));

	return 1;
    } else {
	PrintDebug("keyboard: read of >1 byte from 80h\n");

	return length;
    }
}
#endif
    
  



static int keyboard_write_command(ushort_t port,
				  void * src, 
				  uint_t length,
				  struct vm_device * dev)
{
    struct keyboard_internal *state = (struct keyboard_internal *)(dev->private_data);
    uchar_t cmd;

    // Should always be single byte write

    if (length != 1) { 
	PrintError("keyboard: write of >1 bytes (%d) to 64h\n", length);
	return -1;
    }

    cmd = *((uchar_t*)src); 

    if (state->state != NORMAL) { 
	PrintDebug("keyboard: warning - receiving command on 64h but state != NORMAL\n");
    }
  
    PrintDebug("keyboard: command 0x%x on 64h\n", cmd);

    switch (cmd) { 

	case 0x20:  // READ COMMAND BYTE (returned in 60h)
	    PushToOutputQueue(dev, state->cmd_byte, OVERWRITE, COMMAND,KEYBOARD);
	    state->state = NORMAL;  // the next read on 0x60 will get the right data
	    PrintDebug("keyboard: command byte 0x%x returned\n", state->cmd_byte);
	    break;

	case 0x60:  // WRITE COMMAND BYTE (read from 60h)
	    state->state = WRITING_CMD_BYTE; // we need to make sure we send the next 0x60 byte appropriately
	    PrintDebug("keyboard: prepare to write command byte\n");
	    break;

	    // case 0x90-9f - write to output port  (?)

	case 0xa1: // Get version number
	    PushToOutputQueue(dev, 0, OVERWRITE, COMMAND, KEYBOARD);
	    state->state = NORMAL;
	    PrintDebug("keyboard: version number 0x0 returned\n");
	    break;

	case 0xa4:  // is password installed?  send result to 0x60
	    // we don't support passwords
	    PushToOutputQueue(dev, 0xf1, OVERWRITE, COMMAND, KEYBOARD);
	    PrintDebug("keyboard: password not installed\n");
	    state->state = NORMAL;
	    break;

	case 0xa5:  // new password will arrive on 0x60
	    state->state = TRANSMIT_PASSWD;
	    PrintDebug("keyboard: pepare to transmit password\n");
	    break;

	case 0xa6:  // check passwd;
	    // since we do not support passwords, we will simply ignore this
	    // the implication is that any password check immediately succeeds 
	    // with a blank password
	    state->state = NORMAL;
	    PrintDebug("keyboard: password check succeeded\n");
	    break;

	case 0xa7:  // disable mouse
	    state->cmd_byte |= CMD_MOUSE_DISABLE;
	    state->state = NORMAL;
	    PrintDebug("keyboard: mouse disabled\n");
	    break;

	case 0xa8:  // enable mouse
	    state->cmd_byte &= ~CMD_MOUSE_DISABLE;
	    state->state = NORMAL;
	    PrintDebug("keyboard: mouse enabled\n");
	    break;

	case 0xa9:  // mouse interface test  (always succeeds)
	    PushToOutputQueue(dev, 0, OVERWRITE, COMMAND, KEYBOARD);
	    PrintDebug("keyboard: mouse interface test succeeded\n");
	    state->state = NORMAL;
	    break;

	case 0xaa:  // controller self test (always succeeds)
	    PushToOutputQueue(dev, 0x55, OVERWRITE, COMMAND, KEYBOARD);
	    PrintDebug("keyboard: controller self test succeeded\n");
	    state->state = NORMAL;
	    break;

	case 0xab:  // keyboard interface test (always succeeds)
	    PushToOutputQueue(dev, 0, OVERWRITE, COMMAND, KEYBOARD);
	    state->state = NORMAL;
	    PrintDebug("keyboard: keyboard interface test succeeded\n");
	    break;

	case 0xad:  // disable keyboard
	    state->cmd_byte |= CMD_DISABLE;
	    state->state = NORMAL;
	    PrintDebug("keyboard: keyboard disabled\n");
	    break;

	case 0xae:  // enable keyboard
	    state->cmd_byte &= ~CMD_DISABLE;
	    state->state = NORMAL;
	    PrintDebug("keyboard: keyboard enabled\n");
	    break;

	case 0xaf:  // get version
	    PushToOutputQueue(dev, 0x00, OVERWRITE, COMMAND, KEYBOARD);
	    state->state = NORMAL;
	    PrintDebug("keyboard: version 0 returned \n");
	    break;

	case 0xd0: // return microcontroller output on 60h
	    PushToOutputQueue(dev, state->output_byte, OVERWRITE, COMMAND, KEYBOARD);
	    state->state = NORMAL;
	    PrintDebug("keyboard: output byte 0x%x returned\n", state->output_byte);
	    break;

	case 0xd1: // request to write next byte on 60h to the microcontroller output port
	    state->state = WRITING_OUTPUT_PORT;
	    PrintDebug("keyboard: prepare to write output byte\n");
	    break;

	case 0xd2:  //  write keyboard buffer (inject key)
	    state->state = INJECTING_KEY;
	    PrintDebug("keyboard: prepare to inject key\n");
	    break;

	case 0xd3: //  write mouse buffer (inject mouse)
	    state->state = INJECTING_MOUSE;
	    PrintDebug("keyboard: prepare to inject mouse\n");
	    break;

	case 0xd4: // write mouse device (command to mouse?)
	    state->state = IN_MOUSE;
	    PrintDebug("keyboard: prepare to inject mouse command\n");
	    break;

	case 0xc0: //  read input port 
	    PushToOutputQueue(dev, state->input_byte, OVERWRITE, COMMAND, KEYBOARD);
	    state->state = NORMAL;
	    PrintDebug("keyboard: input byte 0x%x returned\n", state->input_byte);
	    break;

	case 0xc1:  //copy input port lsn to status msn
	    state->status_byte &= 0x0f;
	    state->status_byte |= (state->input_byte & 0xf) << 4;
	    state->state = NORMAL;
	    PrintDebug("keyboard: copied input byte lsn to status msn\n");
	    break;

	case 0xc2: // copy input port msn to status msn
	    state->status_byte &= 0x0f;
	    state->status_byte |= (state->input_byte & 0xf0);
	    state->state = NORMAL;
	    PrintDebug("keyboard: copied input byte msn to status msn\n");
	    break;
    
	case 0xe0: // read test port
	    PushToOutputQueue(dev, state->output_byte >> 6, OVERWRITE, COMMAND, KEYBOARD);
	    state->state = NORMAL;
	    PrintDebug("keyboard: read 0x%x from test port\n", state->output_byte >> 6);
	    break;

   
	case 0xf0:   // pulse output port
	case 0xf1:   // this should pulse 0..3 of cmd_byte on output port 
	case 0xf2:   // instead of what is currently in output_byte (I think)
	case 0xf3:   // main effect is taht if bit zero is zero
	case 0xf4:   // should cause reset
	case 0xf5:   // I doubt anything more recent than a 286 running 
	case 0xf6:   // OS2 with the penalty box will care
	case 0xf7:
	case 0xf8:
	case 0xf9:
	case 0xfa:
	case 0xfb:
	case 0xfc:
	case 0xfd:
	case 0xfe:
	case 0xff:
  
	    PrintDebug("keyboard: ignoring pulse of 0x%x (low=pulsed) on output port\n", (cmd & 0xf));
	    state->state = NORMAL;
	    break;
   

	    // case ac  diagonstic - returns 16 bytes from keyboard microcontroler on 60h
	default:
	    PrintDebug("keyboard: ignoring command (unimplemented)\n");
	    state->state = NORMAL;
	    break;
    }

    return 1;

}

static int keyboard_read_status(ushort_t port,
				void * dest, 
				uint_t length,
				struct vm_device * dev)
{
    struct keyboard_internal *state = (struct keyboard_internal *)(dev->private_data);

    if (length == 1) { 

	PrintDebug("keyboard: read status (64h): ");

	*((uchar_t*)dest) = state->status_byte;

	PrintDebug("0x%x\n", *((uchar_t*)dest));

	return 1;
    } else {
	PrintError("keyboard: >1 byte read for status (64h)\n");
	return -1;
    }
}

static int keyboard_write_output(ushort_t port,
				 void * src, 
				 uint_t length,
				 struct vm_device * dev)
{
    struct keyboard_internal *state = (struct keyboard_internal *)(dev->private_data);

    if (length != 1) { 
	PrintError("keyboard: write of 60h with >1 byte\n");
	return -1;
    }

    uchar_t data = *((uchar_t*)src);
  
    PrintDebug("keyboard: output 0x%x on 60h\n", data);

    switch (state->state) {
	case WRITING_CMD_BYTE:
	    state->cmd_byte = data;
	    state->state = NORMAL;
	    PrintDebug("keyboard: wrote new command byte 0x%x\n", state->cmd_byte);
	    break;
	case WRITING_OUTPUT_PORT:
	    state->output_byte = data;
	    state->state = NORMAL;
	    PrintDebug("keyboard: wrote new output byte 0x%x\n", state->output_byte);
	    break;
	case INJECTING_KEY:
	    PushToOutputQueue(dev, data, OVERWRITE, COMMAND, KEYBOARD);  // probably should be a call to deliver_key_to_vmm()
	    state->state = NORMAL;
	    PrintDebug("keyboard: injected key 0x%x\n", data);
	    break;
	case INJECTING_MOUSE:
	    PushToOutputQueue(dev, data, OVERWRITE, DATA, MOUSE);
	    //	    PrintDebug("keyboard: ignoring injected mouse event 0x%x\n", data);
	    PrintDebug("keyboard: injected mouse event 0x%x\n", data);
	    state->state = NORMAL;
	    break;
	case IN_MOUSE:
	    PrintDebug("keyboard: mouse action: ");
	    if (mouse_write_output(dev, data)) { 
		state->state = NORMAL;
	    }
	    PrintDebug("\n");
	    break;
	case TRANSMIT_PASSWD:
	    if (data) {
		//ignore passwd
		PrintDebug("keyboard: ignoring password character 0x%x\n",data);
	    } else {
		// end of password
		state->state = NORMAL;
		PrintDebug("keyboard: done with password\n");
	    }
	    break;
	case SET_LEDS:
	    PrintDebug("Keyboard: LEDs being set...\n");
	    PushToOutputQueue(dev, 0xfa, OVERWRITE, COMMAND, KEYBOARD);
	    state->state = NORMAL;
	    break;
	case SET_RATE:
	    PrintDebug("Keyboard: Rate being set...\n");
	    PushToOutputQueue(dev, 0xfa, OVERWRITE, COMMAND, KEYBOARD);
	    state->state = NORMAL;
	    break;
	default:
	case NORMAL:
	    {
		// command is being sent to keyboard controller
		switch (data) { 
		    case 0xff: // reset
			PushToOutputQueue(dev, 0xfa, OVERWRITE, COMMAND, KEYBOARD); // ack
			state->state = RESET;
			PrintDebug("keyboard: reset complete and acked\n");
			break;
		    case 0xf5: // disable scanning
		    case 0xf4: // enable scanning
			// ack
			PushToOutputQueue(dev, 0xfa, OVERWRITE, COMMAND, KEYBOARD);
			// should do something here... PAD
			state->state = NORMAL;
			PrintDebug("keyboard: %s scanning done and acked\n", (data == 0xf5) ? "disable" : "enable");
			break;
		    case 0xf3:
			PushToOutputQueue(dev, 0xfa, OVERWRITE, COMMAND, KEYBOARD);
			state->state = SET_RATE;
			break;
		    case 0xf2: // get keyboard ID
			PushToOutputQueue(dev, 0xfa, OVERWRITE, COMMAND, KEYBOARD);
			state->state = KBD_ID1;
			PrintDebug("Keyboard: Requesting Keyboard ID\n");

			//PushToOutputQueue(dev, 0xab, OVERWRITE, COMMAND, KEYBOARD);
			//state->state = KBD_ID2;
			break;
		    case 0xed: // enable keyboard LEDs
			PushToOutputQueue(dev, 0xfa, OVERWRITE, COMMAND, KEYBOARD);
			state->state = SET_LEDS;
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
			PrintDebug("keyboard: unhandled known command 0x%x on output buffer (60h)\n", data);
			return -1;
			break;
		    default:
			PrintDebug("keyboard: unhandled unknown command 0x%x on output buffer (60h)\n", data);
			state->status_byte |= 0x1;
			return -1;
			break;
		}
		break;
	    }

    }
  
    return 1;
}

static int keyboard_read_input(ushort_t port,
			       void * dest, 
			       uint_t length,
			       struct vm_device * dev)
{
    struct keyboard_internal *state = (struct keyboard_internal *)(dev->private_data);

    if (length == 1) { 
	uchar_t data;
	int done_mouse;

	PrintDebug("keyboard: read from input (60h): ");

	if (state->state == IN_MOUSE) { 
	    done_mouse = mouse_read_input(dev);
	    if (done_mouse) { 
		state->state = NORMAL;
	    }
	} 
      
	PullFromOutputQueue(dev, &data);
      
	if (state->state == RESET) { 
	    // We just delivered the ack for the reset
	    // now we will ready ourselves to deliver the BAT code (success)
	    PushToOutputQueue(dev, 0xaa, OVERWRITE, COMMAND, KEYBOARD);
	    state->state = NORMAL;
	    PrintDebug(" (in reset, pushing BAT test code 0xaa) ");
	} else if (state->state == KBD_ID1) {
	    PushToOutputQueue(dev, 0xab, OVERWRITE, COMMAND, KEYBOARD);
	    state->state = KBD_ID2;
	    PrintDebug(" (in kbd id request, pushing 1st ID val) ");
	} else if (state->state == KBD_ID2) {
	    PushToOutputQueue(dev, 0x83, OVERWRITE, COMMAND, KEYBOARD);
	    state->state = NORMAL;
	    PrintDebug(" (in kbd id request, pushing 2nd ID val) ");
	}

	PrintDebug("0x%x\n", data);

	*((uchar_t*)dest) = data;
    
	return 1;
    } else {
	PrintError("keyboard: unknown size read from input (60h)\n");
	return -1;
    }
}





static int keyboard_init_device(struct vm_device * dev) 
{
 
    //  struct keyboard_internal *data = (struct keyboard_internal *) dev->private_data;

    PrintDebug("keyboard: init_device\n");

    // Would read state here

    keyboard_reset_device(dev);

    // hook ports
    v3_dev_hook_io(dev, KEYBOARD_64H, &keyboard_read_status, &keyboard_write_command);
    v3_dev_hook_io(dev, KEYBOARD_60H, &keyboard_read_input, &keyboard_write_output);

    v3_hook_host_event(dev->vm, HOST_KEYBOARD_EVT, V3_HOST_EVENT_HANDLER(key_event_handler), dev);
    v3_hook_host_event(dev->vm, HOST_MOUSE_EVT, V3_HOST_EVENT_HANDLER(mouse_event_handler), dev);


#if KEYBOARD_DEBUG_80H
    v3_dev_hook_io(dev, KEYBOARD_DELAY_80H, &keyboard_read_delay, &keyboard_write_delay);
#endif

  
    //
    // We do not hook the IRQ here.  Instead, the underlying device driver
    // is responsible to call us back
    // 

    return 0;
}

static int keyboard_deinit_device(struct vm_device *dev)
{

    v3_dev_unhook_io(dev, KEYBOARD_60H);
    v3_dev_unhook_io(dev, KEYBOARD_64H);
#if KEYBOARD_DEBUG_80H
    v3_dev_unhook_io(dev, KEYBOARD_DELAY_80H);
#endif
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




struct vm_device * v3_create_keyboard() {
    struct keyboard_internal * keyboard_state = NULL;

    keyboard_state = (struct keyboard_internal *)V3_Malloc(sizeof(struct keyboard_internal));

    struct vm_device *device = v3_create_device("KEYBOARD", &dev_ops, keyboard_state);


    return device;
}
