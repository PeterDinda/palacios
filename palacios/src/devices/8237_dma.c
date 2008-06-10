#include <devices/8237_dma.h>




struct dma_state {
  int tmp;

};


static int dma_init(struct vm_device * dev) {

  return 0;
}



static struct vm_device_ops dev_ops = {
  .init = dma_init,
  .deinit = NULL,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,
};

struct vm_device * create_dma() {
  struct dma_state * dma = NULL;

  dma = (struct dma_state *)V3_Malloc(sizeof(struct dma_state));
  V3_ASSERT(dma != NULL);

  struct vm_device * dev = create_device("DMA", &dev_ops, dma);

  return dma;
}
