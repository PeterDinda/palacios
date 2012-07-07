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

#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_types.h>

#include <palacios/vmm_ringbuffer.h>
#include <palacios/vmm_lock.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_host_events.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_debug.h>


#ifndef V3_CONFIG_DEBUG_KEYBOARD
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define KEYBOARD_DEBUG_80H   0



#define KEYBOARD_60H          0x60  // keyboard microcontroller
#define KEYBOARD_64H          0x64  // onboard microcontroller

#define KEYBOARD_DELAY_80H    0x80  // written for timing

#define KEYBOARD_IRQ          0x1
#define MOUSE_IRQ             0xc   



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


#define MOUSE_ACK           0xfa

// for queue operations
#define QUEUE               0
#define OVERWRITE           1

// for queue operations - whether it's data or cmd waiting on 60h
#define DATA                0
#define COMMAND             1

// for queue operations - whether this is keyboard or mouse data on 60h
#define KEYBOARD            0
#define MOUSE               1



struct cmd_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t irq_en        : 1;  // 1=interrupts enabled
	    uint8_t mouse_irq_en  : 1;  // 1=interrupts enabled for mouse
	    uint8_t self_test_ok  : 1;  // 1= self test passed
	    uint8_t override      : 1;  // MBZ for  PS2
	    uint8_t disable       : 1;  // 1=disabled keyboard
	    uint8_t mouse_disable : 1;  // 1=disabled mouse
	    uint8_t translate     : 1;  // 1=translate to set 1 scancodes (For PC Compatibility)
	    uint8_t rsvd          : 1;  // must be zero
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));




struct status_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t out_buf_full        : 1; // 1=full (data for system)
	    uint8_t in_buf_full         : 1; // 1=full (data for 8042)
	    uint8_t self_test_ok        : 1; // 1=self-test-passed
	    uint8_t cmd                 : 1; // 0=data on 60h, 1=cmd on 64h
	    uint8_t enabled             : 1; // 1=keyboard is enabled
	    uint8_t mouse_buf_full      : 1; // 1= mouse output buffer full
	    uint8_t timeout_err         : 1; // 1=timeout of keybd
	    uint8_t parity_err          : 1; // 1=parity error
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));






/* This QUEUE_SIZE must be 256 */
/* Its designed this way to cause the start/end index to automatically
   wrap around (2^8 = 256) so an overrun will automatically readjust the 
   indexes 
*/
#define QUEUE_SIZE 256
struct queue {
    uint8_t queue[QUEUE_SIZE];

    uint8_t start;
    uint8_t end;
    int count;
};

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
        // after having a f0 sent to 60
	// we wait for a new output byte on 60
    	GETSET_SCANCODES,
	// first send ACK (0xfa)
	// then wait for reception, and reset kb state
	SET_DEFAULTS,
    } state;


    enum {
	// Normal mouse state
	STREAM, 
	// this is used for setting sample rate
	SAMPLE,
	// set resolution
	SET_RES,
    } mouse_state;



    struct cmd_reg cmd;
    struct status_reg status;

    uint8_t output_byte;      //  output port of onboard uC (e.g. A20)
    uint8_t input_byte;       //  input port of onboard uC

    // Data for system
    uint8_t wrap;     

    uint8_t mouse_enabled;
    uint8_t scancode_set;

    struct queue kbd_queue;
    struct queue mouse_queue;

    struct v3_vm_info * vm;

    v3_lock_t kb_lock;
};


static int keyboard_reset_device(struct keyboard_internal * kbd);


static int update_kb_irq(struct keyboard_internal * state) {
    int irq_num = 0;


    state->status.out_buf_full = 0;
    state->status.mouse_buf_full = 0;


    // If there is pending Keyboard data then it overrides mouse data
    if (state->kbd_queue.count > 0) {
	irq_num = KEYBOARD_IRQ;
    } else if (state->mouse_queue.count > 0) {
	irq_num = MOUSE_IRQ;
	state->status.mouse_buf_full = 1;
    } 
    
    PrintDebug("keyboard: interrupt 0x%d\n", irq_num);
    
    if (irq_num) {
	// Global output buffer flag (for both Keyboard and mouse)
	state->status.out_buf_full = 1;
	
	if (state->cmd.irq_en == 1) { 
	    v3_raise_irq(state->vm, irq_num);
	}
    }

    return 0;
}



/* Only one byte is read per irq 
 * So if the queue is still full after a data read, we re-raise the irq
 * If we keep reading an empty queue we return the last queue entry
 */

static int push_to_output_queue(struct keyboard_internal * state, uint8_t value, uint8_t cmd, uint8_t mouse) {
    struct queue * q = NULL;


    if (mouse) {
	q = &(state->mouse_queue);
    } else {
	q = &(state->kbd_queue);
    }

    if (q->count >= QUEUE_SIZE) {
	return 0;
    }

    if (cmd) {
	state->status.cmd = 1;
    } else {
	state->status.cmd = 0;
    }

    q->queue[q->end] = value;

    if (q->end >= (QUEUE_SIZE - 1)) {
	q->end = 0;
    } else {
	q->end++;
    }

    q->count++;


    update_kb_irq(state);

    return 0;
}



static int pull_from_output_queue(struct keyboard_internal * state, uint8_t * value) {
    struct queue * q = NULL;

    if (state->kbd_queue.count > 0) {
	q = &(state->kbd_queue);
	PrintDebug("Reading from Keyboard Queue\n");
    } else if (state->mouse_queue.count > 0) {
	q = &(state->mouse_queue);
	PrintDebug("Reading from Mouse Queue\n");
    } else {
	uint8_t idx = state->kbd_queue.start - 1;
	PrintDebug("No Data in any queue\n");
	*value = state->kbd_queue.queue[idx];
	return 0;
    }

    *value = q->queue[q->start];

    if (q->start >= (QUEUE_SIZE - 1)) {
	q->start = 0;
    } else {
	q->start++;
    }

    q->count--;


    PrintDebug("Read from Queue: %x\n", *value);
    PrintDebug("QStart=%d, QEnd=%d\n", q->start, q->end);

    update_kb_irq(state);

    return 0;
}


#include <palacios/vmm_telemetry.h>
#ifdef V3_CONFIG_SYMMOD
#include <palacios/vmm_symmod.h>
#endif

static int key_event_handler(struct v3_vm_info * vm, 
			     struct v3_keyboard_event * evt, 
			     void * private_data) {
    struct keyboard_internal * state = (struct keyboard_internal *)private_data;

    PrintDebug("keyboard: injected status 0x%x, and scancode 0x%x\n", evt->status, evt->scan_code);

    if (evt->scan_code == 0x44) { // F10 debug dump
	int i = 0;
	for (i = 0; i < vm->num_cores; i++) {
	    v3_print_guest_state(&(vm->cores[i]));
	}
	//	PrintGuestPageTables(info, info->shdw_pg_state.guest_cr3);
    } 
#ifdef V3_CONFIG_SYMCALL
    else if (evt->scan_code == 0x43) { // F9 Sym test
	struct guest_info * core = &(vm->cores[0]);
	PrintDebug("Testing sym call\n");
	sym_arg_t a0 = 0x1111;
	sym_arg_t a1 = 0x2222;
	sym_arg_t a2 = 0x3333;
	sym_arg_t a3 = 0x4444;
	sym_arg_t a4 = 0x5555;
	uint64_t call_start = 0;
	uint64_t call_end = 0;
	
	V3_Print("Exits before symcall: %d\n", (uint32_t)core->num_exits);

	rdtscll(call_start);
	v3_sym_call5(core, SYMCALL_TEST, &a0, &a1, &a2, &a3, &a4);
	rdtscll(call_end);
	
	V3_Print("Symcall latency = %d cycles (%d exits)\n", (uint32_t)(call_end - call_start), (uint32_t)core->num_exits);

	V3_Print("Symcall  Test Returned arg0=%x, arg1=%x, arg2=%x, arg3=%x, arg4=%x\n",
		 (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3, (uint32_t)a4);

    } 
#endif
    else if (evt->scan_code == 0x42) { // F8 debug toggle
	extern int v3_dbg_enable;
	
	PrintDebug("Toggling Debugging\n");	
	v3_dbg_enable ^= 1;

    } 
#ifdef V3_CONFIG_TELEMETRY

    else if (evt->scan_code == 0x41) { // F7 telemetry dump
	v3_print_global_telemetry(vm);
    } 
#endif
#ifdef V3_CONFIG_SYMMOD
    else if (evt->scan_code == 0x40) { // F6 Test symmod load
	v3_load_sym_capsule(vm, "lnx_test");
    }
#endif


    addr_t irq_state = v3_lock_irqsave(state->kb_lock);

    if ( (state->status.enabled == 1)      // onboard is enabled
	 && (state->cmd.disable == 0) )  {   // keyboard is enabled
    
	push_to_output_queue(state, evt->scan_code, DATA, KEYBOARD);
    }

    v3_unlock_irqrestore(state->kb_lock, irq_state);
  
    return 0;
}


static int mouse_event_handler(struct v3_vm_info * vm, 
			       struct v3_mouse_event * evt, 
			       void * private_data) {
    struct keyboard_internal * kbd = (struct keyboard_internal *)private_data;
    int ret = 0;

    PrintDebug("keyboard: injected mouse packet 0x %x %x %x\n",
	       evt->data[0], evt->data[1], evt->data[2]);
  
    addr_t irq_state = v3_lock_irqsave(kbd->kb_lock);

    switch (kbd->mouse_state) { 
	case STREAM:

	    if (kbd->cmd.mouse_disable == 0) {
		push_to_output_queue(kbd, evt->data[0], DATA, MOUSE);
		push_to_output_queue(kbd, evt->data[1], DATA, MOUSE);
		push_to_output_queue(kbd, evt->data[2], DATA, MOUSE);
	    }
	    break;
	default:
	    PrintError("Invalid mouse state\n");
	    ret = -1;
	    break;
    }


    v3_unlock_irqrestore(kbd->kb_lock, irq_state);

    return ret;
}










static int mouse_write_output(struct keyboard_internal * kbd, uint8_t data) {
    switch (kbd->mouse_state) { 
	case NORMAL:
	    switch (data) {

		case 0xff: //reset
		    if (kbd->mouse_enabled == 0) {
			push_to_output_queue(kbd, 0xfe, DATA, MOUSE) ;   // no mouse!
		    } else {
			push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
			push_to_output_queue(kbd, 0xaa, DATA, MOUSE) ; 
			push_to_output_queue(kbd, 0x00, DATA, MOUSE) ; 
		    }
		    break;

/* 		case 0xfe: //resend */
/* 		    PushToOutputQueue(kbd, 0xfa, OVERWRITE, DATA, MOUSE) ;  */
/* 		    PrintDebug(" mouse resend begins "); */
/* 		    kbd->mouse_done_after_ack = 0; */
/* 		    kbd->mouse_needs_ack = 0; */
/* 		    kbd->mouse_state = STREAM1; */
/* 		    return 0;  // not done */
/* 		    break; */
      
		case 0xf6: // set defaults
		    push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
		    PrintDebug(" mouse set defaults ");

		    break;
      
		case 0xf5: // disable data reporting 
		    push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
		    PrintDebug(" mouse disable data reporting ");
		    break;
      
		case 0xf4: // enable data reporting 
		    push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
		    PrintDebug(" mouse enable data reporting ");
		    break;
      
		case 0xf3: // set sample rate
		    push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
		    kbd->mouse_state = SAMPLE;
		    PrintDebug(" mouse set sample rate begins ");
		    break;
      
		case 0xf2: // get device id
		    push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
		    push_to_output_queue(kbd, 0x0,  DATA, MOUSE); 
		    PrintDebug(" mouse get device id begins ");
		    break;
      
		case 0xf0: // set remote mode
		    push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
		    PrintDebug(" mouse set remote mode  ");
		    break;

		case 0xee: // set wrap mode
		    push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
		    PrintError(" mouse set wrap mode (ignored)  ");
		    break;

		case 0xec: // reset wrap mode
		    push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
		    PrintError(" mouse reset wrap mode (ignored)  ");
		    break;

		case 0xeb: // read data
		    push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
		    PrintError(" mouse switch to wrap mode (ignored)  ");
		    break;
      
		case 0xea: // set stream mode
		    push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
		    PrintDebug(" mouse set stream mode  ");
		    break;

		case 0xe9: // status request
		    push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
		    push_to_output_queue(kbd, 0x00, DATA, MOUSE); 
		    push_to_output_queue(kbd, 0x00, DATA, MOUSE);
		    push_to_output_queue(kbd, 0x00, DATA, MOUSE); 
		    PrintDebug(" mouse status request begins  ");
		    break;

		case 0xe8: // set resolution
		    push_to_output_queue(kbd, MOUSE_ACK,  DATA, MOUSE) ; 
		    PrintDebug(" mouse set resolution begins  ");
		    kbd->mouse_state = SET_RES;
		    break;

		case 0xe7: // set scaling 2:1
		    push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
		    PrintDebug(" mouse set scaling 2:1 ");
		    break;

		case 0xe6: // set scaling 1:1
		    push_to_output_queue(kbd, MOUSE_ACK, DATA, MOUSE) ; 
		    PrintDebug(" mouse set scaling 1:1 ");
		    break;
      
		default:
		    PrintDebug(" receiving unknown mouse command (0x%x) in acceptable kbd ", data);
		    break;
	    }

	    break;
	case SAMPLE:
	case SET_RES:
	default:
	    PrintDebug(" receiving mouse output in unhandled kbd (0x%x) ", kbd->mouse_state);
	    return -1;
    }

    return 0;
}



#if KEYBOARD_DEBUG_80H
static int keyboard_write_delay(struct guest_info *core, ushort_t port, void * src,  uint_t length, void * priv_data) {

    if (length == 1) { 
	PrintDebug("keyboard: write of 0x%x to 80h\n", *((uint8_t*)src));
	return 1;
    } else {
	PrintDebug("keyboard: write of >1 byte to 80h\n", *((uint8_t*)src));
	return length;
    }
}

static int keyboard_read_delay(struct guest_info * core, ushort_t port, void * dest, uint_t length, void * priv_data) {

    if (length == 1) { 
	*(uint8_t *)dest = v3_inb(port);

	PrintDebug("keyboard: read of 0x%x from 80h\n", *((uint8_t*)dest));

	return 1;
    } else {
	PrintDebug("keyboard: read of >1 byte from 80h\n");

	return length;
    }
}
#endif
    
  



static int keyboard_write_command(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct keyboard_internal * kbd = priv_data;
    uint8_t cmd = *(uint8_t *)src;

    // Should always be single byte write
    if (length != 1) { 
	PrintError("keyboard: write of >1 bytes (%d) to 64h\n", length);
	return -1;
    }


    addr_t irq_state = v3_lock_irqsave(kbd->kb_lock);

    if (kbd->state != NORMAL) { 
	PrintDebug("keyboard: warning - receiving command on 64h but state != NORMAL\n");
    }
  
    PrintDebug("keyboard: command 0x%x on 64h\n", cmd);

    switch (cmd) { 
	case 0x20:  // READ COMMAND BYTE (returned in 60h)
	    push_to_output_queue(kbd, kbd->cmd.val, COMMAND, KEYBOARD);
	    PrintDebug("keyboard: command byte 0x%x returned\n", kbd->cmd.val);
	    break;

	case 0x60:  // WRITE COMMAND BYTE (read from 60h)
	    kbd->state = WRITING_CMD_BYTE; // we need to make sure we send the next 0x60 byte appropriately
	    PrintDebug("keyboard: prepare to write command byte\n");
	    break;

	    // case 0x90-9f - write to output port  (?)

	case 0xa1: // Get version number
	    push_to_output_queue(kbd, 0x00, COMMAND, KEYBOARD);
	    PrintDebug("keyboard: version number 0x0 returned\n");
	    break;

	case 0xa4:  // is password installed?  send result to 0x60
	    // we don't support passwords
	    push_to_output_queue(kbd, 0xf1, COMMAND, KEYBOARD);
	    PrintDebug("keyboard: password not installed\n");
	    break;

	case 0xa5:  // new password will arrive on 0x60
	    kbd->state = TRANSMIT_PASSWD;
	    PrintDebug("keyboard: pepare to transmit password\n");
	    break;

	case 0xa6:  // check passwd;
	    // since we do not support passwords, we will simply ignore this
	    // the implication is that any password check immediately succeeds 
	    // with a blank password
	    PrintDebug("keyboard: password check succeeded\n");
	    break;

	case 0xa7:  // disable mouse
	    kbd->cmd.mouse_disable = 1;
	    PrintDebug("keyboard: mouse disabled\n");
	    break;

	case 0xa8:  // enable mouse
	    kbd->cmd.mouse_disable = 0;
	    PrintDebug("keyboard: mouse enabled\n");
	    break;

	case 0xa9:  // mouse interface test  (always succeeds)
	    push_to_output_queue(kbd, 0x00, COMMAND, KEYBOARD);
	    PrintDebug("keyboard: mouse interface test succeeded\n");
	    break;

	case 0xaa:  // controller self test (always succeeds)
	    push_to_output_queue(kbd, 0x55, COMMAND, KEYBOARD);
	    PrintDebug("keyboard: controller self test succeeded\n");
	    break;

	case 0xab:  // keyboard interface test (always succeeds)
	    push_to_output_queue(kbd, 0, COMMAND, KEYBOARD);
	    PrintDebug("keyboard: keyboard interface test succeeded\n");
	    break;

	case 0xad:  // disable keyboard
	    kbd->cmd.disable = 1;
	    PrintDebug("keyboard: keyboard disabled\n");
	    break;

	case 0xae:  // enable keyboard
	    kbd->cmd.disable = 0;
	    PrintDebug("keyboard: keyboard enabled\n");
	    break;

	case 0xaf:  // get version
	    push_to_output_queue(kbd, 0x00, COMMAND, KEYBOARD);
	    PrintDebug("keyboard: version 0 returned \n");
	    break;

	case 0xd0: // return microcontroller output on 60h
	    push_to_output_queue(kbd, kbd->output_byte, COMMAND, KEYBOARD);
	    PrintDebug("keyboard: output byte 0x%x returned\n", kbd->output_byte);
	    break;

	case 0xd1: // request to write next byte on 60h to the microcontroller output port
	    kbd->state = WRITING_OUTPUT_PORT;
	    PrintDebug("keyboard: prepare to write output byte\n");
	    break;

	case 0xd2:  //  write keyboard buffer (inject key)
	    kbd->state = INJECTING_KEY;
	    PrintDebug("keyboard: prepare to inject key\n");
	    break;

	case 0xd3: //  write mouse buffer (inject mouse)
	    kbd->state = INJECTING_MOUSE;
	    PrintDebug("keyboard: prepare to inject mouse\n");
	    break;

	case 0xd4: // write mouse device (command to mouse?)
	    kbd->state = IN_MOUSE;
	    PrintDebug("keyboard: prepare to inject mouse command\n");
	    break;

	case 0xc0: //  read input port 
	    push_to_output_queue(kbd, kbd->input_byte, COMMAND, KEYBOARD);
	    PrintDebug("keyboard: input byte 0x%x returned\n", kbd->input_byte);
	    break;

	case 0xc1:  //copy input port lsn to status msn
	    kbd->status.val &= 0x0f;
	    kbd->status.val |= (kbd->input_byte & 0xf) << 4;
	    PrintDebug("keyboard: copied input byte low 4 bits to status reg hi 4 bits\n");
	    break;

	case 0xc2: // copy input port msn to status msn
	    kbd->status.val &= 0x0f;
	    kbd->status.val |= (kbd->input_byte & 0xf0);
	    PrintDebug("keyboard: copied input byte hi 4 bits to status reg hi 4 bits\n");
	    break;
    
	case 0xe0: // read test port
	    push_to_output_queue(kbd, kbd->output_byte >> 6, COMMAND, KEYBOARD);
	    PrintDebug("keyboard: read 0x%x from test port\n", kbd->output_byte >> 6);
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
	    break;
   
	    // case ac  diagonstic - returns 16 bytes from keyboard microcontroler on 60h
	default:
	    PrintDebug("keyboard: ignoring command (unimplemented)\n");
	    break;
    }

    v3_unlock_irqrestore(kbd->kb_lock, irq_state);

    return length;
}

static int keyboard_read_status(struct guest_info * core, ushort_t port, void * dest, uint_t length, void * priv_data) {
    struct keyboard_internal * kbd = priv_data;

    if (length != 1) { 
	PrintError("keyboard: >1 byte read for status (64h)\n");
	return -1;
    }

    PrintDebug("keyboard: read status (64h): ");

    addr_t irq_state = v3_lock_irqsave(kbd->kb_lock);

    *(uint8_t *)dest = kbd->status.val;

    v3_unlock_irqrestore(kbd->kb_lock, irq_state);
    
    PrintDebug("0x%x\n", *(uint8_t *)dest);
    
    return length;
}

static int keyboard_write_output(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct keyboard_internal * kbd = priv_data;
    int ret = length;

    if (length != 1) { 
	PrintError("keyboard: write of 60h with >1 byte\n");
	return -1;
    }

    uint8_t data = *(uint8_t *)src;
  
    PrintDebug("keyboard: output 0x%x on 60h\n", data);

    addr_t irq_state = v3_lock_irqsave(kbd->kb_lock);

    switch (kbd->state) {
	case WRITING_CMD_BYTE:
	    kbd->cmd.val = data;
	    kbd->state = NORMAL;
	    PrintDebug("keyboard: wrote new command byte 0x%x\n", kbd->cmd.val);
	    break;

	case WRITING_OUTPUT_PORT:
	    kbd->output_byte = data;
	    kbd->state = NORMAL;
	    PrintDebug("keyboard: wrote new output byte 0x%x\n", kbd->output_byte);
	    break;

	case INJECTING_KEY:
	    push_to_output_queue(kbd, data, COMMAND, KEYBOARD);  // probably should be a call to deliver_key_to_vmm()
	    kbd->state = NORMAL;
	    PrintDebug("keyboard: injected key 0x%x\n", data);
	    break;

	case INJECTING_MOUSE:
	    push_to_output_queue(kbd, data, DATA, MOUSE);
	    //	    PrintDebug("keyboard: ignoring injected mouse event 0x%x\n", data);
	    PrintDebug("keyboard: injected mouse event 0x%x\n", data);
	    kbd->state = NORMAL;
	    break;

	case IN_MOUSE:
	    PrintDebug("keyboard: mouse action: ");
	    if (mouse_write_output(kbd, data)) { 
		kbd->state = NORMAL;
	    }
	    PrintDebug("\n");
	    break;

	case TRANSMIT_PASSWD:
	    if (data) {
		//ignore passwd
		PrintDebug("keyboard: ignoring password character 0x%x\n",data);
	    } else {
		// end of password
		kbd->state = NORMAL;
		PrintDebug("keyboard: done with password\n");
	    }
	    break;

	case SET_LEDS:
	    PrintDebug("Keyboard: LEDs being set...\n");
	    push_to_output_queue(kbd, 0xfa, COMMAND, KEYBOARD);
	    kbd->state = NORMAL;
	    break;

	case SET_RATE:
	    PrintDebug("Keyboard: Rate being set...\n");
	    push_to_output_queue(kbd, 0xfa, COMMAND, KEYBOARD);
	    kbd->state = NORMAL;
	    break;

	case GETSET_SCANCODES:
    	    switch (data) {
    	    	case 0:
	    	    PrintDebug("Keyboard: scancode set being read\n");
    	    	    push_to_output_queue(kbd, 0x45 - 2 * kbd->scancode_set, COMMAND, KEYBOARD);
    	    	    break;
    	    	case 1:
	    	    PrintError("keyboard: unsupported scancode set %d selected\n", data);
	    	    return -1;
    	    	case 2:
	    	    PrintDebug("Keyboard: scancode set being set to %d\n", data);
	    	    kbd->scancode_set = data;
	    	    push_to_output_queue(kbd, 0xfa, COMMAND, KEYBOARD);
    	    	    break;
    	    	case 3:
    	    	    /* OpenBSD wants scancode set 3, but falls back to 2 if a
    	    	     * subsequent read reveals that the request was ignored
    	    	     */
	    	    PrintError("keyboard: ignoring request for scancode set %d\n", data);
	    	    break;
    	    	default:
	    	    PrintError("keyboard: unknown scancode set %d selected\n", data);
	    	    ret = -1;
                    break;
  
    	    }
	    kbd->state = NORMAL;
	    break;

	case SET_DEFAULTS:
	    keyboard_reset_device(kbd);
	    kbd->state = NORMAL;
	    break;

	default:
	case NORMAL: {
	    // command is being sent to keyboard controller
	    switch (data) { 
		case 0xff: // reset
		    push_to_output_queue(kbd, 0xfa, COMMAND, KEYBOARD); // ack
		    push_to_output_queue(kbd, 0xaa, COMMAND, KEYBOARD);
		    PrintDebug("keyboard: reset complete and acked\n");
		    break;

		case 0xf5: // disable scanning
		case 0xf4: // enable scanning
		    // ack
		    push_to_output_queue(kbd, 0xfa, COMMAND, KEYBOARD);
		    // should do something here... PAD
		    PrintDebug("keyboard: %s scanning done and acked\n", (data == 0xf5) ? "disable" : "enable");
		    break;

		case 0xf3:
		    push_to_output_queue(kbd, 0xfa, COMMAND, KEYBOARD);
		    kbd->state = SET_RATE;
		    break;

		case 0xf2: // get keyboard ID
		    push_to_output_queue(kbd, 0xfa, COMMAND, KEYBOARD);
		    push_to_output_queue(kbd, 0xab, COMMAND, KEYBOARD);
		    push_to_output_queue(kbd, 0x83, COMMAND, KEYBOARD);
		    PrintDebug("Keyboard: Requesting Keyboard ID\n");
		    break;

		case 0xed: // enable keyboard LEDs
		    push_to_output_queue(kbd, 0xfa, COMMAND, KEYBOARD);
		    kbd->state = SET_LEDS;
		    break;

		case 0xee: // echo, used by FreeBSD to probe controller
		    push_to_output_queue(kbd, 0xee, COMMAND, KEYBOARD);
		    break;

		case 0xf0: // get/set scancode set
		    push_to_output_queue(kbd, 0xfa, COMMAND, KEYBOARD);
		    kbd->state = GETSET_SCANCODES;
		    break;


		case 0xf6: // set defaults
		    // ACK command
		    // clear output buffer
		    // reset to init state
		    push_to_output_queue(kbd, 0xfa, COMMAND, KEYBOARD);
		    kbd->state = SET_DEFAULTS;
		    break;

		case 0xfe: // resend
		case 0xfd: // set key type make
		case 0xfc: // set key typ make/break
		case 0xfb: // set key type typematic
		case 0xfa: // set all typematic make/break/typematic
		case 0xf9: // set all make
		case 0xf8: // set all make/break
		case 0xf7: // set all typemaktic

		    PrintError("keyboard: unhandled known command 0x%x on output buffer (60h)\n", data);
		    ret = -1;
		    break;

		default:
		    PrintError("keyboard: unhandled unknown command 0x%x on output buffer (60h)\n", data);
		    kbd->status.out_buf_full = 1;
		    ret = -1;
		    break;
	    }
	    break;
	}
    }
  
    v3_unlock_irqrestore(kbd->kb_lock, irq_state);

    return ret;
}

static int keyboard_read_input(struct guest_info * core, ushort_t port, void * dest, uint_t length, void * priv_data) {
    struct keyboard_internal * kbd = priv_data;

    if (length != 1) {
	PrintError("keyboard: unknown size read from input (60h)\n");
	return -1;
    }
    
    addr_t irq_state = v3_lock_irqsave(kbd->kb_lock);

    pull_from_output_queue(kbd, (uint8_t *)dest);
      
    v3_unlock_irqrestore(kbd->kb_lock, irq_state);

    PrintDebug("keyboard: read from input (60h): 0x%x\n", *(uint8_t *)dest);

    return length;
}






static int keyboard_free(struct keyboard_internal * kbd) {
    

    // unhook host events

    V3_Free(kbd);
    return 0;
}




static int keyboard_reset_device(struct keyboard_internal * kbd) {
  

    kbd->mouse_queue.start = 0;
    kbd->mouse_queue.end = 0;
    kbd->mouse_queue.count = 0;

    kbd->kbd_queue.start = 0;
    kbd->kbd_queue.end = 0;
    kbd->kbd_queue.count = 0;

    kbd->mouse_enabled = 0;
    kbd->scancode_set = 2;

    kbd->state = NORMAL;
    kbd->mouse_state = STREAM;

    // PS2, keyboard+mouse enabled, generic translation    
    kbd->cmd.val = 0;

    kbd->cmd.irq_en = 1;
    kbd->cmd.mouse_irq_en = 1;
    kbd->cmd.self_test_ok = 1;
    /** **/


    // buffers empty, no errors
    kbd->status.val = 0; 

    kbd->status.self_test_ok = 1; // self-tests passed
    kbd->status.enabled = 1;// keyboard ready
    /** **/

    
    kbd->output_byte = 0;  //  ?

    kbd->input_byte = INPUT_RAM;  // we have some
    // also display=color, jumper 0, keyboard enabled 

    PrintDebug("keyboard: reset device\n");
 
    return 0;

}

#ifdef V3_CONFIG_CHECKPOINT
static int keyboard_save(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct keyboard_internal * kbd = (struct keyboard_internal *)private_data;

    v3_chkpt_save_8(ctx, "CMD_REG", &(kbd->cmd.val));
    v3_chkpt_save_8(ctx, "STATUS_REG", &(kbd->status.val));
    v3_chkpt_save_8(ctx, "STATE", &(kbd->state));
    v3_chkpt_save_8(ctx, "MOUSE_STATE", &(kbd->mouse_state));
    v3_chkpt_save_8(ctx, "OUTPUT", &(kbd->output_byte));
    v3_chkpt_save_8(ctx, "INPUT", &(kbd->input_byte));
    v3_chkpt_save_8(ctx, "SCANCODE_SET", &(kbd->scancode_set));
    v3_chkpt_save_8(ctx, "MOUSE_ENABLED", &(kbd->mouse_enabled));


    return 0;
}


static int keyboard_load(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct keyboard_internal * kbd = (struct keyboard_internal *)private_data;
    keyboard_reset_device(kbd);

    v3_chkpt_load_8(ctx, "CMD_REG", &(kbd->cmd.val));
    v3_chkpt_load_8(ctx, "STATUS_REG", &(kbd->status.val));
    v3_chkpt_load_8(ctx, "STATE", &(kbd->state));
    v3_chkpt_load_8(ctx, "MOUSE_STATE", &(kbd->mouse_state));
    v3_chkpt_load_8(ctx, "OUTPUT", &(kbd->output_byte));
    v3_chkpt_load_8(ctx, "INPUT", &(kbd->input_byte));
    v3_chkpt_load_8(ctx, "SCANCODE_SET", &(kbd->scancode_set));
    v3_chkpt_load_8(ctx, "MOUSE_ENABLED", &(kbd->mouse_enabled));


    return 0;
}

#endif


static struct v3_device_ops dev_ops = { 
    .free = (int (*)(void *))keyboard_free,
#ifdef V3_CONFIG_CHECKPOINT
    .save = keyboard_save,
    .load = keyboard_load
#endif
};




static int keyboard_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct keyboard_internal * kbd = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    int ret = 0;

    PrintDebug("keyboard: init_device\n");

    kbd = (struct keyboard_internal *)V3_Malloc(sizeof(struct keyboard_internal));

    if (!kbd) {
	PrintError("Cannot allocate in init\n");
	return -1;
    }

    memset(kbd, 0, sizeof(struct keyboard_internal));

    kbd->vm = vm;

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, kbd);

    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(kbd);
	return -1;
    }

    keyboard_reset_device(kbd);


    v3_lock_init(&(kbd->kb_lock));


    // hook ports
    ret |= v3_dev_hook_io(dev, KEYBOARD_64H, &keyboard_read_status, &keyboard_write_command);
    ret |= v3_dev_hook_io(dev, KEYBOARD_60H, &keyboard_read_input, &keyboard_write_output);

    if (ret != 0) {
	PrintError("Error hooking keyboard IO ports\n");
	v3_remove_device(dev);
	return -1;
    }

    v3_hook_host_event(vm, HOST_KEYBOARD_EVT, V3_HOST_EVENT_HANDLER(key_event_handler), kbd);
    v3_hook_host_event(vm, HOST_MOUSE_EVT, V3_HOST_EVENT_HANDLER(mouse_event_handler), kbd);


#if KEYBOARD_DEBUG_80H
    v3_dev_hook_io(dev, KEYBOARD_DELAY_80H, &keyboard_read_delay, &keyboard_write_delay);
#endif

  
    //
    // We do not hook the IRQ here.  Instead, the underlying device driver
    // is responsible to call us back
    // 

    return 0;
}


device_register("KEYBOARD", keyboard_init)
