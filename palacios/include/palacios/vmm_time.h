#ifndef __VMM_TIME_H
#define __VMM_TIME_H


#include <palacios/vmm_types.h>
#include <palacios/vmm_list.h>

struct guest_info;

struct vm_time {
  uint32_t cpu_freq; // in kHZ

  // Total number of guest run time cycles
  ullong_t guest_tsc;

  // Cache value to help calculate the guest_tsc
  ullong_t cached_host_tsc;

  // The number of cycles pending for notification to the timers
  //ullong_t pending_cycles;

  // Installed Timers 
  uint_t num_timers;
  struct list_head timers;
};


struct vm_timer_ops {
 void (*update_time)(ullong_t cpu_cycles, ullong_t cpu_freq, void * priv_data);

};

struct vm_timer {
  void * private_data;
  struct vm_timer_ops * ops;

  struct list_head timer_link;
};


void v3_init_time(struct vm_time * time_state);




int v3_add_timer(struct guest_info * info, struct vm_timer_ops * ops, void * private_data);
int v3_remove_timer(struct guest_info * info, struct vm_timer * timer);


void v3_update_time(struct guest_info * info, ullong_t cycles);

#endif
