#include <devices/8254.h>
#include <palacios/vmm.h>
#include <palacios/vmm_time.h>

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
typedef enum {NOT_RUNNING, WAITING_LOBYTE, WAITING_HIBYTE, RUNNING} channel_access_state_t;
typedef enum {LATCH_COUNT, LOBYTE_ONLY, HIBYTE_ONLY, LOBYTE_HIBYTE} channel_access_mode_t;
typedef enum {IRQ_ON_TERM_CNT, ONE_SHOT, RATE_GEN, SQR_WAVE, SW_STROBE, HW_STROBE} channel_op_mode_t;


struct channel {
  channel_access_mode_t access_mode;
  channel_access_state_t access_state;

  channel_op_mode_t op_mode;



  // Time til interrupt trigger 

  ushort_t counter;
  ushort_t reload_value;

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





static void pit_update_time(ullong_t cpu_cycles, ullong_t cpu_freq, void * private_data) {
  PrintDebug("Adding %d cycles\n", cpu_cycles);
  
  return;
}


static int handle_channel_write(struct channel * ch, char val) {
  //  switch (ch->access_mode) {


  //}


  return -1;
}


static int handle_channel_read(struct channel * ch, char * val) {
  return -1;
}





static int handle_channel_cmd(struct channel * ch, struct pit_cmd_word cmd) {
  ch->op_mode = cmd.op_mode;
  ch->access_mode = cmd.access_mode;

  switch (cmd.access_mode) {
  case LATCH_COUNT:
    return -1;
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
  default:
    return -1;
  }

  return 0;
    
}




static int pit_read_channel(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  struct pit * state = (struct pit *)dev->private_data;
  char * val = (char *)dst;

  if (length != 1) {
    PrintDebug("8254 PIT: Invalid Read Write length \n");
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
    PrintDebug("8254 PIT: Read from invalid port (%d)\n", port);
    return -1;
  }

  return length;
}



static int pit_write_channel(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  struct pit * state = (struct pit *)dev->private_data;
  char val = *(char *)src;

  if (length != 1) {
    PrintDebug("8254 PIT: Invalid Write Length\n");
    return -1;
  }

  PrintDebug("8254 PIT: Write to PIT Channel %d\n", port - CHANNEL0_PORT);


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
    PrintDebug("8254 PIT: Write to invalid port (%d)\n", port);
    return -1;
  }

  return length;
}




static int pit_write_command(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  struct pit * state = (struct pit *)dev->private_data;
  struct pit_cmd_word * cmd = (struct pit_cmd_word *)src;

  PrintDebug("8254 PIT: Write to PIT Command port\n");

  if (length != 1) {
    PrintDebug("8254 PIT: Write of Invalid length to command port\n");
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


static int pit_init(struct vm_device * dev) {
  dev_hook_io(dev, CHANNEL0_PORT, &pit_read_channel, &pit_write_channel);
  dev_hook_io(dev, CHANNEL1_PORT, &pit_read_channel, &pit_write_channel);
  dev_hook_io(dev, CHANNEL2_PORT, &pit_read_channel, &pit_write_channel);
  dev_hook_io(dev, COMMAND_PORT, NULL, &pit_write_command);


  v3_add_timer(dev->vm, &timer_ops, dev);

  // Get cpu frequency and calculate the global pit oscilattor counter/cycle


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
