#include <devices/nvram.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>

extern struct vmm_os_hooks *os_hooks;

extern void SerialPrint(const char *format, ...);

#define NVRAM_REG_PORT  0x70
#define NVRAM_DATA_PORT 0x71



typedef enum {NVRAM_READY, NVRAM_REG_POSTED} nvram_state_t;


#define NVRAM_REG_MAX   256


// These are borrowed from Bochs, which borrowed from
// Ralf Brown's interupt list, and extended
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

#define NVRAM_IBM_HD_DATA                 0x12

#define NVRAM_REG_FLOPPY_TYPE             0x10
#define NVRAM_REG_EQUIPMENT_BYTE          0x14

#define NVRAM_REG_BASE_MEMORY_HIGH        0x16
#define NVRAM_REG_BASE_MEMORY_LOW         0x15

#define NVRAM_REG_EXT_MEMORY_HIGH         0x18
#define NVRAM_REG_EXT_MEMORY_LOW          0x17

#define NVRAM_REG_EXT_MEMORY_2ND_HIGH     0x31
#define NVRAM_REG_EXT_MEMORY_2ND_LOW      0x30

#define NVRAM_REG_BOOTSEQ_OLD             0x2d

#define NVRAM_REG_AMI_BIG_MEMORY_HIGH     0x35
#define NVRAM_REG_AMI_BIG_MEMORY_LOW      0x34


#define NVRAM_REG_CSUM_HIGH               0x2e
#define NVRAM_REG_CSUM_LOW                0x2f
#define NVRAM_REG_IBM_CENTURY_BYTE        0x32  
#define NVRAM_REG_IBM_PS2_CENTURY_BYTE    0x37  

#define NVRAM_REG_BOOTSEQ_NEW_FIRST       0x3D
#define NVRAM_REG_BOOTSEQ_NEW_SECOND      0x38


struct nvram_internal {
  nvram_state_t dev_state;
  uchar_t       thereg;
  uchar_t       mem_state[NVRAM_REG_MAX];
};



static int set_nvram_defaults(struct vm_device *dev)
{
  struct nvram_internal * nvram_state = (struct nvram_internal*) dev->private_data;

  //
  // 2 1.44 MB floppy drives
  //
#if 1
  nvram_state->mem_state[NVRAM_REG_FLOPPY_TYPE]= 0x44;
#else
  nvram_state->mem_state[NVRAM_REG_FLOPPY_TYPE] = 0x00;
#endif

  //
  // For old boot sequence style, do floppy first
  //
  nvram_state->mem_state[NVRAM_REG_BOOTSEQ_OLD]= 0x10;

#if 0
  // For new boot sequence style, do floppy, cd, then hd
  nvram_state->mem_state[NVRAM_REG_BOOTSEQ_NEW_FIRST]= 0x31;
  nvram_state->mem_state[NVRAM_REG_BOOTSEQ_NEW_SECOND]= 0x20;
#endif

  // For new boot sequence style, do cd, hd, floppy
  nvram_state->mem_state[NVRAM_REG_BOOTSEQ_NEW_FIRST]= 0x23;
  nvram_state->mem_state[NVRAM_REG_BOOTSEQ_NEW_SECOND]= 0x10;
 

  // Set equipment byte to note 2 floppies, vga display, keyboard,math,floppy
  nvram_state->mem_state[NVRAM_REG_EQUIPMENT_BYTE]= 0x4f;
  //nvram_state->mem_state[NVRAM_REG_EQUIPMENT_BYTE] = 0xf;

  // Set conventional memory to 640K
  nvram_state->mem_state[NVRAM_REG_BASE_MEMORY_HIGH]= 0x02;
  nvram_state->mem_state[NVRAM_REG_BASE_MEMORY_LOW]= 0x80;

  // Set extended memory to 15 MB
  nvram_state->mem_state[NVRAM_REG_EXT_MEMORY_HIGH]= 0x3C;
  nvram_state->mem_state[NVRAM_REG_EXT_MEMORY_LOW]= 0x00;
  nvram_state->mem_state[NVRAM_REG_EXT_MEMORY_2ND_HIGH]= 0x3C;
  nvram_state->mem_state[NVRAM_REG_EXT_MEMORY_2ND_LOW]= 0x00;

  // Set the extended memory beyond 16 MB to 128-16 MB
  nvram_state->mem_state[NVRAM_REG_AMI_BIG_MEMORY_HIGH] = 0x7;
  nvram_state->mem_state[NVRAM_REG_AMI_BIG_MEMORY_LOW] = 0x00;

  //nvram_state->mem_state[NVRAM_REG_AMI_BIG_MEMORY_HIGH]= 0x00;
  //nvram_state->mem_state[NVRAM_REG_AMI_BIG_MEMORY_LOW]= 0x00;

  
  // This is the harddisk type.... Set accordingly...
  nvram_state->mem_state[NVRAM_IBM_HD_DATA] = 0x20;

  // Set the shutdown status gently
  // soft reset
  nvram_state->mem_state[NVRAM_REG_SHUTDOWN_STATUS] = 0x0;


  // RTC status A
  // time update in progress, default timebase (32KHz, default interrupt rate 1KHz)
  // 10100110
  nvram_state->mem_state[NVRAM_REG_STAT_A] = 0xa6;

  // RTC status B
  // time updates, default timebase (32KHz, default interrupt rate 1KHz)
  // 10100110
  //nvram_state->mem_state[NVRAM_REG_STAT_B] = 0xa6;
  


  return 0;

}


int nvram_reset_device(struct vm_device * dev)
{
  struct nvram_internal *data = (struct nvram_internal *) dev->private_data;
  
  SerialPrint("nvram: reset device\n");

 

  data->dev_state = NVRAM_READY;
  data->thereg=0;

  
  return 0;

}





int nvram_start_device(struct vm_device *dev)
{
  SerialPrint("nvram: start device\n");
  return 0;
}


int nvram_stop_device(struct vm_device *dev)
{
  SerialPrint("nvram: stop device\n");
  return 0;
}




int nvram_write_reg_port(ushort_t port,
			void * src, 
			uint_t length,
			struct vm_device * dev)
{
  struct nvram_internal *data = (struct nvram_internal *) dev->private_data;

  memcpy(&(data->thereg), src, 1);
  PrintDebug("Writing To NVRAM reg: 0x%x\n", data->thereg);


  return 1;
}

int nvram_read_data_port(ushort_t port,
		       void   * dst, 
		       uint_t length,
		       struct vm_device * dev)
{
  struct nvram_internal *data = (struct nvram_internal *) dev->private_data;



  memcpy(dst, &(data->mem_state[data->thereg]), 1);

  PrintDebug("nvram_read_data_port(0x%x)=0x%x\n", data->thereg, data->mem_state[data->thereg]);

  // hack
  if (data->thereg==NVRAM_REG_STAT_A) { 
    data->mem_state[data->thereg] ^= 0x80;  // toggle Update in progess
  }


  return 1;
}

int nvram_write_data_port(ushort_t port,
			void * src, 
			uint_t length,
			struct vm_device * dev)
{
  struct nvram_internal *data = (struct nvram_internal *) dev->private_data;

  memcpy(&(data->mem_state[data->thereg]), src, 1);

  PrintDebug("nvram_write_data_port(0x%x)=0x%x\n", data->thereg, data->mem_state[data->thereg]);

  return 1;
}



int nvram_init_device(struct vm_device * dev) {
 
  struct nvram_internal *data = (struct nvram_internal *) dev->private_data;

  SerialPrint("nvram: init_device\n");

  memset(data->mem_state, 0, NVRAM_REG_MAX);

  // Would read state here
  set_nvram_defaults(dev);

  nvram_reset_device(dev);

  // hook ports
  dev_hook_io(dev, NVRAM_REG_PORT, NULL, &nvram_write_reg_port);
  dev_hook_io(dev, NVRAM_DATA_PORT, &nvram_read_data_port, &nvram_write_data_port);
  
  return 0;
}

int nvram_deinit_device(struct vm_device *dev)
{


  dev_unhook_io(dev, NVRAM_REG_PORT);
  dev_unhook_io(dev, NVRAM_DATA_PORT);

  nvram_reset_device(dev);
  return 0;
}





static struct vm_device_ops dev_ops = { 
  .init = nvram_init_device, 
  .deinit = nvram_deinit_device,
  .reset = nvram_reset_device,
  .start = nvram_start_device,
  .stop = nvram_stop_device,
};




struct vm_device *create_nvram() {
  struct nvram_internal * nvram_state = os_hooks->malloc(sizeof(struct nvram_internal)+1000);

  SerialPrint("internal at %x\n",nvram_state);

  struct vm_device *device = create_device("NVRAM", &dev_ops, nvram_state);


  return device;
}
