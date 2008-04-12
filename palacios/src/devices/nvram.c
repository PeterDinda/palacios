#include <devices/nvram.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>

extern struct vmm_os_hooks *os_hooks;


#define NVRAM_REG_PORT  0x70
#define NVRAM_DATA_PORT 0x71



typedef enum {NVRAM_READY,NVRAM_REG_POSTED} nvram_state_t;


#define NVRAM_REG_MAX   256


// These are borrowed from Bochs, which borrowed from
// Ralf Brown's interupt list
#define NVRAM_REG_SEC                     0x00
#define NVRAM_REG_SEC_ALARM               0x01
#define NVRAM_REG_MIN                     0x02
#define NVRAM_REG_MIN_ALARM               0x03
#define NVRAM_REG_HOUR                    0x04
#define NVRAM_REG_HOUR_ALARM              0x05
#define NVRAM_REG_WEEK_DAY                0x06
#define NVRAM_REG_MONTH_DAY               0x07
#define NVRAM_REG_MONTH                   0x08
#define NVRAM_REG_YEAR                    0x09
#define NVRAM_REG_STAT_A                  0x0a
#define NVRAM_REG_STAT_B                  0x0b
#define NVRAM_REG_STAT_C                  0x0c
#define NVRAM_REG_STAT_D                  0x0d
#define NVRAM_REG_DIAGNOSTIC_STATUS       0x0e  
#define NVRAM_REG_SHUTDOWN_STATUS         0x0f
#define NVRAM_REG_EQUIPMENT_BYTE          0x14
#define NVRAM_REG_CSUM_HIGH               0x2e
#define NVRAM_REG_CSUM_LOW                0x2f
#define NVRAM_REG_IBM_CENTURY_BYTE        0x32  
#define NVRAM_REG_IBM_PS2_CENTURY_BYTE    0x37  



struct nvram_internal {
  nvram_state_t dev_state;
  uchar_t       thereg;
  uchar_t       mem_state[NVRAM_REG_MAX];
};





int nvram_reset_device(struct vm_device *dev)
{
  struct nvram_internal *data = (struct nvram_internal *) dev->private_data;
  
  data->dev_state = NVRAM_READY;
  data->thereg=0;
  
  return 0;

}

int nvram_init_device(struct vm_device *dev, struct vm_guest *vm)
{
  struct nvram_internal *data = (struct nvram_internal *) dev->private_data;
 
  memset(data->mem_state,0,NVRAM_REG_MAX);

  nvram_reset_device(dev);

  // hook ports
  dev_mgr_hook_io(dev->vm, 
		  dev,
		  NVRAM_REG_PORT,
		  DEVICE_EMULATED,
		  DEVICE_WRITE);

  dev_mgr_hook_io(dev->vm, 
		  dev,
		  NVRAM_DATA_PORT,
		  DEVICE_EMULATED,
		  DEVICE_READWRITE);

  return 0;
}

int nvram_deinit_device(struct vm_device *dev)
{

  nvram_reset_device(dev);
  
  dev_mgr_unhook_device(dev->vm,dev);

  return 0;
}






int nvram_start_device(struct vm_device *dev)
{
  return 0;
}


int nvram_stop_device(struct vm_device *dev)
{
  return 0;
}



int nvram_read_io_port(ushort_t port_read,
		       void   *address, 
		       uint_t length,
		       struct vm_device *dev)
{
  struct nvram_internal *data = (struct nvram_internal *) dev->private_data;

  switch (port_read) { 
  case NVRAM_REG_PORT:
    // nonsense
    memset(address,0,length);
    break;
  case NVRAM_DATA_PORT:
    memcpy(address,&(data->mem_state[data->thereg]),1);
    break;
  default:
    //bad
    return -1;
  }
  return 0;
}

int nvram_write_io_port(ushort_t port_written,
			void   *address, 
			uint_t length,
			struct vm_device *dev)
{
  struct nvram_internal *data = (struct nvram_internal *) dev->private_data;

  switch (port_written) { 
  case NVRAM_REG_PORT:
    memcpy(&(data->thereg),address,1);
    break;
  case NVRAM_DATA_PORT:
    memcpy(&(data->mem_state[data->thereg]),address,1);
    break;
  default:
    //bad
    return -1;
  }
  return 0;
}

int nvram_read_mapped_memory(void   *address_read,
			     void   *address, 
			     uint_t length,
			     struct vm_device *dev)
{
  return -1;

}

int nvram_write_mapped_memory(void   *address_written,
			      void   *address, 
			      uint_t length,
			      struct vm_device *dev)
{
  return -1;
}


static struct vm_device nvram_template = 
  { .init_device = nvram_init_device, 
    .deinit_device = nvram_deinit_device,
    .reset_device = nvram_reset_device,
    .start_device = nvram_start_device,
    .stop_device = nvram_stop_device,
    .read_io_port = nvram_read_io_port,
    .write_io_port = nvram_write_io_port,
    .read_mapped_memory = nvram_read_mapped_memory,
    .write_mapped_memory= nvram_write_mapped_memory,
  };



struct vm_device *nvram_create()
{
  struct vm_device *device = os_hooks->malloc(sizeof(struct vm_device));

  *device = nvram_template;

  device->private_data = os_hooks->malloc(sizeof(struct nvram_internal));

  return device;
}
