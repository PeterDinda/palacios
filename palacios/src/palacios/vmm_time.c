#include "palacios/vmm_time.h"
#include "palacios/vmm.h"


void v3_init_time(struct vm_time * time_state) {
  ullong_t cpu_khz = 0;

  V3_CPU_KHZ(cpu_khz);
  time_state->cpu_freq = cpu_khz;

  PrintDebug("CPU KHZ = HI=%x LO=%x\n", (uint_t)(cpu_khz >> 32), (uint_t)cpu_khz);
 
  time_state->guest_tsc = 0;
  time_state->cached_host_tsc = 0;
  time_state->pending_cycles = 0;
  
  INIT_LIST_HEAD(&(time_state->timers));
  time_state->num_timers = 0;
}


int v3_add_timer(struct guest_info * info, struct vm_timer_ops * ops, void * private_data) {
  //  V3_Malloc

  /*
  list_add(&(timer->timer_link), &(info->time_state.timers));
  info->time_state.num_timers++;
  */
  return 0;
}

int remove_timer(struct guest_info * info, struct vm_timer * timer) {
  list_del(&(timer->timer_link));
  info->time_state.num_timers--;

  return 0;
}


void update_timers(struct guest_info * info) {
  struct vm_timer * tmp_timer;
  
  list_for_each_entry(tmp_timer, &(info->time_state.timers), timer_link) {
    tmp_timer->ops.update_time(info->time_state.pending_cycles, info->time_state.cpu_freq, tmp_timer->private_data);
  }


  info->time_state.pending_cycles = 0;
}
