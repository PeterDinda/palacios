/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */

#include <devices/8254.h>
#include <palacios/vmm.h>
#include <palacios/vmm_time.h>
#include <palacios/vmm_util.h>
#include <palacios/vmm_intr.h>



#ifndef DEBUG_PIT
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif



// constants
#define OSC_HZ 1193182


/* The 8254 has three counters and one control port */
#define CHANNEL0_PORT 0x40
#define CHANNEL1_PORT 0x41
#define CHANNEL2_PORT 0x42
#define COMMAND_PORT 0x43


#define PIT_INTR_NUM 0

/* The order of these typedefs is important because the numerical values correspond to the 
 * values coming from the io ports
 */
typedef enum {NOT_RUNNING, PENDING, RUNNING} channel_run_state_t;
typedef enum {NOT_WAITING, WAITING_LOBYTE, WAITING_HIBYTE} channel_access_state_t;
typedef enum {LATCH_COUNT, LOBYTE_ONLY, HIBYTE_ONLY, LOBYTE_HIBYTE} channel_access_mode_t;
typedef enum {IRQ_ON_TERM_CNT, ONE_SHOT, RATE_GEN, SQR_WAVE, SW_STROBE, HW_STROBE} channel_op_mode_t;


struct channel {
  channel_access_mode_t access_mode;
  channel_access_state_t access_state;
  channel_run_state_t run_state;

  channel_op_mode_t op_mode;


  // Time til interrupt trigger 

  ushort_t counter;
  ushort_t reload_value;

  ushort_t latched_value;
  
  enum {NOTLATCHED, LATCHED} latch_state;

  enum {LSB, MSB} read_state;

  uint_t output_pin : 1;
  uint_t gate_input_pin : 1;
};


struct pit {

  ullong_t pit_counter;
  ullong_t pit_reload;


  struct channel ch_0;
  struct channel ch_1;
  struct channel ch_2;
};


struct pit_cmd_word {
  uint_t bcd_mode    : 1;
  uint_t op_mode     : 3;
  uint_t access_mode : 2;
  uint_t channel     : 2;
};

struct pit_rdb_cmd_word {
  uint_t rsvd         : 1; // SBZ
  uint_t ch_0         : 1;
  uint_t ch_1         : 1;
  uint_t ch_2         : 1;
  uint_t latch_status : 1;
  uint_t latch_count  : 1;
  uint_t readback_cmd : 2; // Must Be 0x3
};

struct pit_rdb_status_word {
  uint_t bcd_mode     : 1;
  uint_t op_mode      : 3;
  uint_t access_mode  : 2;
  uint_t null_count   : 1;
  uint_t pin_state    : 1; 
};



/* 
 * This should call out to handle_SQR_WAVE_tics, etc... 
 */
// Returns true if the the output signal changed state
static int handle_crystal_tics(struct vm_device * dev, struct channel * ch, uint_t oscillations) {
  uint_t channel_cycles = 0;
  uint_t output_changed = 0;
  
  // PrintDebug("8254 PIT: %d crystal tics\n", oscillations);
  if (ch->run_state == PENDING) {
    oscillations--;
    ch->counter = ch->reload_value;

    if (ch->op_mode == SQR_WAVE) {
      ch->counter -= ch->counter % 2;
    }

    ch->run_state = RUNNING;
  } else if (ch->run_state != RUNNING) {
    return output_changed;
  }

  /*
  PrintDebug("8254 PIT: Channel Run State = %d, counter=", ch->run_state);
  PrintTraceLL(ch->counter);
  PrintDebug("\n");
  */
  if (ch->op_mode == SQR_WAVE) {
    oscillations *= 2;
  }

  if (ch->counter > oscillations) {
    ch->counter -= oscillations;
    return output_changed;
  } else {
    ushort_t reload_val = ch->reload_value; 
    oscillations -= ch->counter;
    ch->counter = 0;
    channel_cycles = 1;

    
    if (ch->op_mode == SQR_WAVE) {
      reload_val -= reload_val % 2;
    }
    
    channel_cycles += oscillations / reload_val;
    oscillations = oscillations % reload_val;

    ch->counter = reload_val - oscillations;
  }

  //  PrintDebug("8254 PIT: Channel Cycles: %d\n", channel_cycles);
  


  switch (ch->op_mode) {
  case IRQ_ON_TERM_CNT:
    if ((channel_cycles > 0) && (ch->output_pin == 0)) {
      ch->output_pin = 1; 
      output_changed = 1;
    }
    break;
  case ONE_SHOT:
    if ((channel_cycles > 0) && (ch->output_pin == 0)) {
      ch->output_pin = 1; 
      output_changed = 1;
    }
    break;
  case RATE_GEN:
    // See the data sheet: we ignore the output pin cycle...
    if (channel_cycles > 0) {
      output_changed = 1;
    }
    break;
  case SQR_WAVE:
    ch->output_pin = (ch->output_pin + 1) % 2;

    if (ch->output_pin == 1) {
      output_changed = 1;
    }

    break;
  case SW_STROBE:
    return -1;
    break;
  case HW_STROBE:
    return -1;
    break;
  default:
    break;
  }

  return output_changed;
}
				


static void pit_update_time(ullong_t cpu_cycles, ullong_t cpu_freq, void * private_data) {
  struct vm_device * dev = (struct vm_device *)private_data;
  struct pit * state = (struct pit *)dev->private_data;
  //  ullong_t tmp_ctr = state->pit_counter;
  ullong_t tmp_cycles;
  uint_t oscillations = 0;


  /*
    PrintDebug("updating cpu_cycles=");
    PrintTraceLL(cpu_cycles);
    PrintDebug("\n");
    
    PrintDebug("pit_counter=");
    PrintTraceLL(state->pit_counter);
    PrintDebug("\n");
    
    PrintDebug("pit_reload=");
    PrintTraceLL(state->pit_reload);
    PrintDebug("\n");
  */

  if (state->pit_counter > cpu_cycles) {
    // Easy...
    state->pit_counter -= cpu_cycles;
  } else {
    
    // Take off the first part
    cpu_cycles -= state->pit_counter;
    state->pit_counter = 0;
    oscillations = 1;
    
    if (cpu_cycles > state->pit_reload) {
      // how many full oscillations
      tmp_cycles = cpu_cycles;

      cpu_cycles = do_divll(tmp_cycles, state->pit_reload);

      oscillations += tmp_cycles;
    }

    // update counter with remainder (mod reload)
    state->pit_counter = state->pit_reload - cpu_cycles;    

    //PrintDebug("8254 PIT: Handling %d crystal tics\n", oscillations);
    if (handle_crystal_tics(dev, &(state->ch_0), oscillations) == 1) {
      // raise interrupt
      PrintDebug("8254 PIT: Injecting Timer interrupt to guest\n");
      v3_raise_irq(dev->vm, 0);
    }

    //handle_crystal_tics(dev, &(state->ch_1), oscillations);
    //handle_crystal_tics(dev, &(state->ch_2), oscillations);
  }
  


 
  return;
}



/* This should call out to handle_SQR_WAVE_write, etc...
 */
static int handle_channel_write(struct channel * ch, char val) {

    switch (ch->access_state) {      
    case WAITING_HIBYTE:
      {
	ushort_t tmp_val = ((ushort_t)val) << 8;
	ch->reload_value &= 0x00ff;
	ch->reload_value |= tmp_val;
	

	if ((ch->op_mode != RATE_GEN) || (ch->run_state != RUNNING)){
	  ch->run_state = PENDING;  
	}
	
	if (ch->access_mode == LOBYTE_HIBYTE) {
	  ch->access_state = WAITING_LOBYTE;
	}

	PrintDebug("8254 PIT: updated channel counter: %d\n", ch->reload_value); 	
	PrintDebug("8254 PIT: Channel Run State=%d\n", ch->run_state);
	break;
      }
    case WAITING_LOBYTE:
      ch->reload_value &= 0xff00;
      ch->reload_value |= val;
      
      if (ch->access_mode == LOBYTE_HIBYTE) {
	ch->access_state = WAITING_HIBYTE;
      } else if ((ch->op_mode != RATE_GEN) || (ch->run_state != RUNNING)) {
	ch->run_state = PENDING;
      }
      
      PrintDebug("8254 PIT: updated channel counter: %d\n", ch->reload_value);
      PrintDebug("8254 PIT: Channel Run State=%d\n", ch->run_state);
      break;
    default:
      return -1;
    }
    

    switch (ch->op_mode) {
    case IRQ_ON_TERM_CNT:
      ch->output_pin = 0;
      break;
    case ONE_SHOT:
      ch->output_pin = 1;
      break;
    case RATE_GEN:
      ch->output_pin = 1;
      break;
    case SQR_WAVE:
      ch->output_pin = 1;
      break;
    default:
      return -1;
      break;
    }


  return 0;
}


static int handle_channel_read(struct channel * ch, char * val) {

  ushort_t * myval;

  if (ch->latch_state == NOTLATCHED) { 
    myval = &(ch->counter);
  } else {
    myval = &(ch->latched_value);
  }

  if (ch->read_state == LSB) { 
    *val = ((char*)myval)[0];  // little endian
    ch->read_state = MSB;
  } else {
    *val = ((char*)myval)[1];
    ch->read_state = LSB;
    if (ch->latch_state == LATCHED) { 
      ch->latch_state = NOTLATCHED;
    }
  }

  return 0;

}





static int handle_channel_cmd(struct channel * ch, struct pit_cmd_word cmd) {
  ch->op_mode = cmd.op_mode;
  ch->access_mode = cmd.access_mode;




  switch (cmd.access_mode) {
  case LATCH_COUNT:
    if (ch->latch_state == NOTLATCHED) { 
      ch->latched_value = ch->counter;
      ch->latch_state = LATCHED;
    }
    break;
  case HIBYTE_ONLY:
    ch->access_state = WAITING_HIBYTE;
    break;
  case LOBYTE_ONLY: 
  case LOBYTE_HIBYTE:
    ch->access_state = WAITING_LOBYTE;
    break;
  }


  switch (cmd.op_mode) {
  case IRQ_ON_TERM_CNT:
    ch->output_pin = 0;
    break;
  case ONE_SHOT: 
    ch->output_pin = 1;
    break;
  case RATE_GEN: 
    ch->output_pin = 1;
    break;
  case SQR_WAVE:
    ch->output_pin = 1;
    break;
  default:
    return -1;
  }

  return 0;
    
}




static int pit_read_channel(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  struct pit * state = (struct pit *)dev->private_data;
  char * val = (char *)dst;

  if (length != 1) {
    PrintError("8254 PIT: Invalid Read Write length \n");
    return -1;
  }

  PrintDebug("8254 PIT: Read of PIT Channel %d\n", port - CHANNEL0_PORT);

  switch (port) {
  case CHANNEL0_PORT: 
    if (handle_channel_read(&(state->ch_0), val) == -1) {
      return -1;
    }
    break;
  case CHANNEL1_PORT:
    if (handle_channel_read(&(state->ch_1), val) == -1) {
      return -1;
    }
    break;
  case CHANNEL2_PORT:
    if (handle_channel_read(&(state->ch_2), val) == -1) {
      return -1;
    }
    break;
  default:
    PrintError("8254 PIT: Read from invalid port (%d)\n", port);
    return -1;
  }

  return length;
}



static int pit_write_channel(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  struct pit * state = (struct pit *)dev->private_data;
  char val = *(char *)src;

  if (length != 1) {
    PrintError("8254 PIT: Invalid Write Length\n");
    return -1;
  }

  PrintDebug("8254 PIT: Write to PIT Channel %d (%x)\n", port - CHANNEL0_PORT, *(char*)src);


  switch (port) {
  case CHANNEL0_PORT:
    if (handle_channel_write(&(state->ch_0), val) == -1) {
      return -1;
    } 
    break;
  case CHANNEL1_PORT:
    if (handle_channel_write(&(state->ch_1), val) == -1) {
      return -1;
    }
    break;
  case CHANNEL2_PORT:
    if (handle_channel_write(&(state->ch_2), val) == -1) {
      return -1;
    }
    break;
  default:
    PrintError("8254 PIT: Write to invalid port (%d)\n", port);
    return -1;
  }

  return length;
}




static int pit_write_command(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  struct pit * state = (struct pit *)dev->private_data;
  struct pit_cmd_word * cmd = (struct pit_cmd_word *)src;

  PrintDebug("8254 PIT: Write to PIT Command port\n");
  PrintDebug("8254 PIT: Writing to channel %d (access_mode = %d, op_mode = %d)\n", cmd->channel, cmd->access_mode, cmd->op_mode);
  if (length != 1) {
    PrintError("8254 PIT: Write of Invalid length to command port\n");
    return -1;
  }

  switch (cmd->channel) {
  case 0:
    if (handle_channel_cmd(&(state->ch_0), *cmd) == -1) {
      return -1;
    }
    break;
  case 1:
    if (handle_channel_cmd(&(state->ch_1), *cmd) == -1) {
      return -1;
    }
    break;
  case 2:
    if (handle_channel_cmd(&(state->ch_2), *cmd) == -1) {
      return -1;
    }
    break;
  case 3:
    // Read Back command
    return -1;
    break;
  default:
    break;
  }


  return length;
}




static struct vm_timer_ops timer_ops = {
  .update_time = pit_update_time,
};


static void init_channel(struct channel * ch) {
  ch->run_state = NOT_RUNNING;
  ch->access_state = NOT_WAITING;
  ch->access_mode = 0;
  ch->op_mode = 0;

  ch->counter = 0;
  ch->reload_value = 0;
  ch->output_pin = 0;
  ch->gate_input_pin = 0;

  ch->latched_value = 0;
  ch->latch_state = NOTLATCHED;
  ch->read_state = LSB;

  return;
}


static int pit_init(struct vm_device * dev) {
  struct pit * state = (struct pit *)dev->private_data;
  uint_t cpu_khz = V3_CPU_KHZ();
  ullong_t reload_val = (ullong_t)cpu_khz * 1000;

  dev_hook_io(dev, CHANNEL0_PORT, &pit_read_channel, &pit_write_channel);
  dev_hook_io(dev, CHANNEL1_PORT, &pit_read_channel, &pit_write_channel);
  dev_hook_io(dev, CHANNEL2_PORT, &pit_read_channel, &pit_write_channel);
  dev_hook_io(dev, COMMAND_PORT, NULL, &pit_write_command);

#ifdef DEBUG_PIT
  PrintDebug("8254 PIT: OSC_HZ=%d, reload_val=", OSC_HZ);
  PrintTraceLL(reload_val);
  PrintDebug("\n");
#endif

  v3_add_timer(dev->vm, &timer_ops, dev);

  // Get cpu frequency and calculate the global pit oscilattor counter/cycle

  do_divll(reload_val, OSC_HZ);
  state->pit_counter = reload_val;
  state->pit_reload = reload_val;



  init_channel(&(state->ch_0));
  init_channel(&(state->ch_1));
  init_channel(&(state->ch_2));

#ifdef DEBUG_PIT
  PrintDebug("8254 PIT: CPU MHZ=%d -- pit count=", cpu_khz / 1000);
  PrintTraceLL(state->pit_counter);
  PrintDebug("\n");
#endif

  return 0;
}

static int pit_deinit(struct vm_device * dev) {

  return 0;
}


static struct vm_device_ops dev_ops = {
  .init = pit_init,
  .deinit = pit_deinit,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,

};


struct vm_device * create_pit() {
  struct pit * pit_state = NULL;
  pit_state = (struct pit *)V3_Malloc(sizeof(struct pit));
  V3_ASSERT(pit_state != NULL);

  struct vm_device * dev = create_device("PIT", &dev_ops, pit_state);
  
  return dev;
}
