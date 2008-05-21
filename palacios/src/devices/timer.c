#include <devices/timer.h>
#include <palacios/vmm.h>


#define TIMER_IRQ 32

struct timer_state {
  int foo;
};



int irq_handler(uint_t irq, struct vm_device * dev) {
  PrintDebug("Timer interrupt\n");
  return 0;

}

int timer_init(struct vm_device * dev) {
  //dev_hook_irq(dev, TIMER_IRQ, &irq_handler);

  return 0;
}

int timer_deinit(struct vm_device * dev) {

  return 0;
}


static struct vm_device_ops dev_ops = {
  .init = timer_init,
  .deinit = timer_deinit,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,

};


struct vm_device * create_timer() {
  struct timer_state * timer = NULL;
  timer = (struct timer_state *)V3_Malloc( sizeof(struct timer_state));
  V3_ASSERT(timer != NULL);

  struct vm_device * dev = create_device("Timer", &dev_ops, timer);
  
  return dev;

}
