#include "palacios/vmm_time.h"
#include "palacios/vmm.h"


void v3_init_time(struct vm_time * time_state) {

  time_state->cpu_freq = V3_CPU_KHZ();
 
  time_state->guest_tsc = 0;
  time_state->cached_host_tsc = 0;
  // time_state->pending_cycles = 0;
  
  INIT_LIST_HEAD(&(time_state->timers));
  time_state->num_timers = 0;
}


int v3_add_timer(struct guest_info * info, struct vm_timer_ops * ops, void * private_data) {
  struct vm_timer * timer = NULL;
  timer = (struct vm_timer *)V3_Malloc(sizeof(struct vm_timer));
  V3_ASSERT(timer != NULL);

  timer->ops = ops;
  timer->private_data = private_data;

  list_add(&(timer->timer_link), &(info->time_state.timers));
  info->time_state.num_timers++;

  return 0;
}


int v3_remove_timer(struct guest_info * info, struct vm_timer * timer) {
  list_del(&(timer->timer_link));
  info->time_state.num_timers--;

  V3_Free(timer);
  return 0;
}



void v3_update_time(struct guest_info * info, ullong_t cycles) {
  struct vm_timer * tmp_timer;
  
  info->time_state.guest_tsc += cycles;

  list_for_each_entry(tmp_timer, &(info->time_state.timers), timer_link) {
    tmp_timer->ops->update_time(cycles, info->time_state.cpu_freq, tmp_timer->private_data);
  }
  


  //info->time_state.pending_cycles = 0;
}
