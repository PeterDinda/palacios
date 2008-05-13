#include <devices/simple_pic.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm.h>

struct pic_internal {
  int pending_irq;
  int error_code;
};


static int pic_intr_pending(void * private_data) {
  struct pic_internal * data = (struct pic_internal *)private_data;
  
  return (data->pending_irq > 0);
}

static int pic_raise_intr(void * private_data, int irq, int error_code) {
  struct pic_internal * data = (struct pic_internal *)private_data;

  data->pending_irq = irq;
  data->error_code = error_code;

  return 0;
}


static int pic_get_intr_number(void * private_data) {
  struct pic_internal * data = (struct pic_internal *)private_data;

  return data->pending_irq;
}


static struct intr_ctrl_ops intr_ops = {
  .intr_pending = pic_intr_pending,
  .get_intr_number = pic_get_intr_number,
  .raise_intr = pic_raise_intr
};




int pic_init_device(struct vm_device * dev) {
  struct pic_internal * data = (struct pic_internal *)dev->private_data;
  set_intr_controller(dev->vm, &intr_ops, data);
  data->pending_irq = 0;

  return 0;
}


int pic_deinit_device(struct vm_device * dev) {
  return 0;
}





static struct vm_device_ops dev_ops = {
  .init = pic_init_device,
  .deinit = pic_deinit_device,
  .reset = NULL,
  .start = NULL,
  .stop = NULL
};


struct vm_device * create_simple_pic() {
  struct pic_internal * state = NULL;
  V3_Malloc(struct pic_internal *, state, sizeof(struct pic_internal));

  struct vm_device * pic_dev = create_device("Simple Pic", &dev_ops, state);


  return pic_dev;
}
