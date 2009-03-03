/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2009, Chang Seok Bae <jhuell@gmail.com>
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author:  Lei Xia <lxia@northwestern.edu>
 *             Chang Seok Bae <jhuell@gmail.com>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */ 
 
/*
 * Virtual PCI
 */
 
#include <devices/vpci.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm_intr.h>

#define DEBUG_PCI


#ifndef DEBUG_PCI
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define NUM_DEVICES 255
#define NUM_BUS 1

struct pci_bus {
    int bus_num;
    struct pci_device *device_list[NUM_DEVICES];
    struct pci_bus *next;
    struct vm_device *vm_dev;
};

struct pci_internal {
  uint_t       num_buses;
  uint32_t   config_address;   //current value of corresponding to configure port
  struct pci_bus *bus_list[NUM_BUS];
};


struct port_ops_map {
	uint32_t port;
	int (*port_read)(ushort_t port, void * dst, uint_t length, struct vm_device *vdev);
	int (*port_write)(ushort_t port, void * src, uint_t length, struct vm_device *vdev);
};


//Lei
struct pci_device * get_device (struct vm_device *vmdev, uchar_t bus_no, uchar_t devfn_no)
{
	struct pci_device *dev = NULL;
	struct pci_bus *bus = NULL;
	struct pci_internal * pci_state;

	if (bus_no >= NUM_BUS || devfn_no >= NUM_DEVICES)
		return dev;
	
	pci_state = (struct pci_internal *)vmdev->private_data;
	bus = pci_state->bus_list[bus_no];
	if (bus)
		dev = bus->device_list[devfn_no];

	return dev;
}


//Lei
int pci_hook_ports(struct pci_device *dev, 
					int reg_num, 
					int num_ports, 
					port_read_fn *port_reads[], 
					port_write_fn *port_writes[])
{
       struct pci_ioregion *ioreg;

	ioreg = dev->ioregion[reg_num];

	if (!ioreg) return -1;

	if (ioreg->size != num_ports) return -1;

	ioreg->port_reads = port_reads;
	ioreg->port_writes = port_writes;

	return 0;
}

//Lei
static inline void hook_ioregion(struct pci_device *dev, struct pci_ioregion *ioreg)
{
	int i;

	if (ioreg->addr == -1) return;
	if (ioreg->type == PCI_ADDRESS_SPACE_IO){
		for (i = 0; i < ioreg->size; i++)
			if (ioreg->port_reads[i] || ioreg->port_writes[i])
				v3_dev_hook_io(dev->bus->vm_dev, 
								ioreg->addr + i, 
								ioreg->port_reads[i], 
								ioreg->port_writes[i]);
	}

}


//Chang
static uint32_t vpci_read_config(struct pci_device *pdev, uchar_t offset, int len) 
{
	uint32_t val = 0x0;

	switch(len) {
		case 4:
			if(offset <= 0xfc) {
				val = *(uint32_t *)(&(pdev->config)+offset);
				break;
			}
		case 2:
			if(offset <= 0xfe) {
				val = *(uint16_t *)(&(pdev->config)+offset);
				break;
			}
		case 1:
			val = *(uint8_t *)(&(pdev->config)+offset);
			break;
		default:
			break;			
	}
	
	return val;
}

//Lei
static void vpci_write_config(struct pci_device *dev, uchar_t offset, uint32_t val, int len)
{
    uchar_t *dev_config;

    dev_config = (uchar_t *)&(dev->config);
    dev_config += offset;

    switch(len){
	case 1:
		*dev_config = val & 0xff;
		break;
	case 2:
		*((uint16_t *)dev_config) = val & 0xffff;
		break;
	case 4:
		*((uint32_t *)dev_config) = val;
		break;
	default:
		PrintDebug("pci_write_config: wrong length %d\n", len);
		break;
    	}
}

//Lei
void vpci_raise_irq(struct pci_device *pdev, void *data)
{
	struct guest_info *vm;
	int irq_line;

	vm = pdev->bus->vm_dev->vm;
	irq_line = pdev->config.intr_line;
	v3_raise_irq(vm, irq_line);
}

#if 0
//Chang
static void pci_write_config(struct pci_device *dev, uint32_t address, uint32_t val, int len) 
{
	int can_write, i, reg_num;
	uint32_t addr;

	if(len == 4 && ((address >= 0x10 && address < 0x10 + 4 * 6) || //base address registers
		(address >= 0x30 && address < 0x34))) { //expansion rom base address

		struct pci_ioregion * ioregion;
		if(address >= 0x30) {
				reg_num = PCI_ROM_SLOT;
		}else {
			reg_num = ((address - 0x10) >>2); 
		}

		ioregion = &dev->io_regions[reg_num];

		if(ioregion->size == 0) {//default config

		addr = address;
			for (i=0;i<len;i++) {
				switch(*(uint8_t *)(&(dev->config)+0x0e)) {
				case 0x00:
				case 0x80:
					switch(addr) {
					case 0x00:
					case 0x01:
					case 0x02:
					case 0x03:
					case 0x08:
					case 0x09:
					case 0x0a:
					case 0x0b:
					case 0x0e:
					case 0x10 ... 0x27:
					case 0x3d:
						can_write = 0;
						break;
					default:
						can_write = 1;
						break;
					}
					break;	
				default:
				case 0x01:
					switch(addr) {
					case 0x00:
					case 0x01:
					case 0x02:
					case 0x03:
					case 0x08:
					case 0x09:
					case 0x0a:
					case 0x0b:
					case 0x0e:
					case 0x38 ... 0x3b: 
					case 0x3d:
						can_write = 0;
						break;
					default:
						can_write = 1;
						break;
					}
					break;
				}
				if (can_write) {
					*(uint32_t *)(&(dev->config)+addr) = val;
				}
				if(++addr > 0xff) break;
				val >>= 8;
			}

			return;
	
		}else {
			if(reg_num== PCI_ROM_SLOT) {
				val &= (~(ioregion->size -1 )) | 1;
			} else {
				val &= ~(ioregion->size -1);
				val |= ioregion->type;
			}
		}
		//pci_update_mappings();
		return;
	}
}
#endif

/* -1 for dev_num means auto assign */
struct pci_device *
pci_register_device(struct pci_bus *bus, const char *name,
                               int instance_size, int dev_num,
                               uint32_t (*config_read)(struct pci_device *pci_dev, uchar_t addr, int len),
                               void (*config_write)(struct pci_device *pci_dev, uchar_t addr, uint32_t val, int len)) 
{

	struct pci_device * pci_dev;
	int found = 0;
	int i;
	
	if(dev_num < 0) {
		for(dev_num = 0; dev_num < 256; dev_num++) {
			if(!bus->device_list[dev_num]) { 
				found = 1;
				break;
			}
		}
	}
	if (found == 0) return NULL;

	pci_dev = (struct pci_device *)V3_Malloc(sizeof(struct pci_device));

	if(!pci_dev) return NULL;

	pci_dev->bus = bus;
	pci_dev->dev_num = dev_num;
	pci_dev->irqline = -1;

	strcpy(pci_dev->name,name);
	
	if(config_read) 
	       pci_dev->ops.config_read = config_read;
	else
		pci_dev->ops.config_read=&vpci_read_config;
	if(config_write) 
		pci_dev->ops.config_write = config_write;
	else
		pci_dev->ops.config_write=&vpci_write_config;

	pci_dev->ops.raise_irq = &vpci_raise_irq;

      for (i = 0; i < PCI_IO_REGIONS; i++)
		pci_dev->ioregion[i] = NULL;

      //config space initiate

      	bus->device_list[dev_num] = pci_dev;

	return pci_dev;
}

//Chang
static void init_fake_device(struct pci_internal *pci_state) 
{
	//maybe need table to map device, but just 
	//bus_num=0, dev_num=0
	
	//int i=0;
       struct pci_device *fake_device;

	//fake dev
	fake_device = pci_register_device(pci_state->bus_list[0],
				"fake ide", sizeof(struct pci_device),
				-1,
				NULL,NULL);
	
	if (!fake_device) return;

	/*
	intel, ide ctroller
	vendor id:0x8086
	device id: 0x1222
	*/
	fake_device->config.vendor_id = 0x8086;
	fake_device->config.device_id = 0x1222;
	fake_device->config.command = 0x0;
	fake_device->config.status = 0x0;
	fake_device->config.revision = 0x07;
	fake_device->config.class_code[0] = 0x1;
	fake_device->config.class_code[1] = 0x1;
	fake_device->config.class_code[2] = 0x1;
	fake_device->config.header_type = 0x0;
       //base address
	fake_device->config.BAR[0] = 0x1F0; 
	fake_device->config.BAR[1] = 0; 
	fake_device->config.BAR[2] = 0; 
	fake_device->config.BAR[3] = 0; 
	fake_device->config.BAR[4] = 0; 
	fake_device->config.BAR[5] = 0; 
	
	//fake dev end

       //need to register io regions

	pci_state->bus_list[0]->device_list[0] = fake_device;
	fake_device->bus = pci_state->bus_list[0];
	fake_device->next = NULL;
	
	return;
}



// Lei
/* if region_num == -1, assign automatically
 */
int 
pci_register_io_region(struct pci_device *pci_dev, int region_num,
                            			uint32_t size, int type,
                            			pci_mapioregion_fn *map_func)
{
	int found = 0;
      struct pci_ioregion *region;

	if(region_num < 0) {
		for(region_num = 0; region_num < 256; region_num++) {
			if(!pci_dev->ioregion[region_num]) { 
				found = 1;
				break;
			}
		}
	}
	if (found == 0) return -1;
	if (pci_dev->ioregion[region_num] != NULL)
		return -1;

	region = (struct pci_ioregion *)V3_Malloc(sizeof(struct pci_ioregion));
	if (!region) return -1;

	region->addr = -1;
	region->reg_num = region_num;
	region->size = size;
	region->mapped_size = -1;
	region->type = type;
	region->map_func = map_func;
	region->port_reads = NULL;
	region->port_writes = NULL;

	pci_dev->ioregion[region_num] = region;

	return region_num;
}




//Chang
static int 
vpci_addrport_read(ushort_t port,
				 	void * dst,
				 	uint_t length,
				 	struct vm_device *dev) 
{

  struct pci_internal *pci_state = (struct pci_internal *)dev->private_data;
  int start;
  uchar_t *addr;
  int i;

  start = port - PCI_CONFIG_ADDRESS;
  if (length + start > 4){
  	return length;   //cross port boundary, is memory mapped IO style
  }
  addr = (uchar_t *)&(pci_state->config_address);
  addr += start;
  memcpy(dst, addr, length);    //be careful, PCI is little endian

  PrintDebug("PCI Address: reading %d bytes from port %x: 0x", length, port);

  for (i = length - 1; i >= 0; i--) { 
    PrintDebug("%.2x", ((uchar_t*)dst)[i]);
  }
   PrintDebug("\n");
  return length;
}

//Lei
static int 
vpci_addrport_write(ushort_t port,
						void *src,
						uint_t length,
						struct vm_device *dev)
{
  struct pci_internal *pci_state = (struct pci_internal *)dev->private_data;
  int start;
  uchar_t *addr;
  int i;

  start = port - PCI_CONFIG_ADDRESS;
  if (length + start > 4){
  	return length;   //cross port boundary, is memory mapped IO style
  }
  addr = (uchar_t *)&(pci_state->config_address);
  addr += start;
  memcpy(addr, src, length);    //be careful, PCI is little endian

  PrintDebug("PCI Address: writing %d bytes to port %x: 0x", length, port);

  for (i = length - 1; i >= 0; i--) { 
    PrintDebug("%.2x", ((uchar_t*)src)[i]);
  }
   PrintDebug("\n");
  return length;
}

//Chang
static int 
vpci_dataport_read(ushort_t port,
				 	void * dst,
				 	uint_t length,
				 	struct vm_device *vmdev) 
{
	/*
	decode address of config_address
	bus num 	= 	config_address[23:16]
	device num = config_address[15:11]
	func num = 	config_address[10:08]
	reg num	= 	config_address[07:02]
	*/
	
	struct pci_internal * pci_state;
	struct pci_device * pci_dev = NULL;
	int bus_num, devfn, offset;
	uint32_t address;
	uint32_t val;
	int i;

       if (length > 4){
	   	PrintDebug("Read more than 4 bytes from port 0x%x\n", (int)port);
		return length;
       }

	pci_state = (struct pci_internal *)vmdev->private_data;
	address = pci_state->config_address;
	offset = address & 0xff;
	devfn = (address >> 8) & 0xff;
	bus_num = (address >> 16) & 0xff; 

       pci_dev = get_device(vmdev, bus_num, devfn);

	if(!pci_dev) {
		val = 0xffffffff;
	}else {
       	val = 0x0;
		val = pci_dev->ops.config_read(pci_dev, offset, length);
	}
	memcpy(dst,&val,length);

	PrintDebug("PCI Data: reading %d bytes from port %x: 0x", length, port);

  	for (i = length - 1; i >= 0; i--) { 
    		PrintDebug("%.2x", ((uchar_t*)dst)[i]);
 	}
      PrintDebug("\n");
		
	return length;

}

static int 
vpci_dataport_write(ushort_t port,
           					void * src,
     						uint_t length,
     						struct vm_device *vmdev)
{
  struct pci_internal *pci_state;
  uint32_t val;
  uint32_t address;
  struct pci_device *pdev;
  char bus, devfn, offset;
  int i;

  if (length > 4){
	   	PrintDebug("Write more than 4 bytes to port 0x%x\n", (int)port);
		return length;
  }

  pci_state = (struct pci_internal *)vmdev->private_data;
  address = pci_state->config_address;
  offset = address & 0xff;
  devfn = (address >> 8) & 0xff;
  bus = (address >> 16) & 0xff; 

  pdev = get_device(vmdev, bus, devfn);
  if (pdev == NULL){
  	// not sure what to do here, just ignore it
  	return length;
  }
  
  val = 0x0;
  memcpy(&val, src, length);
  
  pdev->ops.config_write(pdev, offset, val, length);

  PrintDebug("PCI Data: writing %d bytes to port %x: 0x", length, port);

  for (i = length - 1; i >= 0; i--) { 
    		PrintDebug("%.2x", ((uchar_t*)src)[i]);
 }
  PrintDebug("\n");
  
  return length;
}
	

//Lei
static void init_pci_bus(struct pci_internal *pci_state) 
{
  int i;
  struct pci_bus *first_bus;

  first_bus = (struct pci_bus *)V3_Malloc(sizeof(struct pci_bus));

  first_bus->bus_num = 0;  //?? not sure
  for (i = 0; i < NUM_DEVICES; i++)
	first_bus->device_list[i] = NULL;
  first_bus->next = NULL;

  pci_state->num_buses = 1;
  pci_state->bus_list[0] = first_bus;
  for (i=1; i<NUM_BUS; i++)
  	pci_state->bus_list[i] = NULL;
}

//Lei
static void init_pci_internal(struct pci_internal *pci_state) 
{

  pci_state->config_address = 0x00;  //Not sure????
  init_pci_bus(pci_state);

}


static int vpci_set_defaults(struct vm_device *dev)
{
  PrintDebug("vpci: set defaults\n");
  return 0;
}


static int vpci_reset_device(struct vm_device * dev)
{
  
  PrintDebug("vpci: reset device\n");

  vpci_set_defaults(dev);
  
  return 0;
}


static int vpci_start_device(struct vm_device *dev)
{
  PrintDebug("vpci: start device\n");
  
  return 0;
}


int vpci_stop_device(struct vm_device *dev)
{
  PrintDebug("vpci: stop device\n");
  
  return 0;
}


int vpci_init_device(struct vm_device * dev) 
{
  struct pci_internal *pci_state;
  int i;

  PrintDebug("vpci: init_device\n");
  
  pci_state = (struct pci_internal *)dev->private_data;

  init_pci_internal(pci_state);

  init_fake_device(pci_state); //Chang

  for (i = 0; i<4; i++){
  	v3_dev_hook_io(dev, PCI_CONFIG_ADDRESS + i, &vpci_addrport_read, &vpci_addrport_write);
  	v3_dev_hook_io(dev, PCI_CONFIG_DATA + i, &vpci_dataport_read, &vpci_dataport_write);
 }


  return 0;
}

int vpci_deinit_device(struct vm_device *dev)
{
   int i;
   
   for (i = 0; i<4; i++){
  	v3_dev_unhook_io(dev, PCI_CONFIG_ADDRESS + i);
  	v3_dev_unhook_io(dev, PCI_CONFIG_DATA + i);
  }

  vpci_reset_device(dev);
  return 0;
}

static struct vm_device_ops dev_ops = { 
  .init = vpci_init_device, 
  .deinit = vpci_deinit_device,
  .reset = vpci_reset_device,
  .start = vpci_start_device,
  .stop = vpci_stop_device,
};

struct vm_device *v3_create_vpci() {
  struct pci_internal * pci_state = V3_Malloc(sizeof(struct pci_internal));

  PrintDebug("PCI internal at %x\n",(int)(long)pci_state);
  struct vm_device *device = v3_create_device("PCI", &dev_ops, pci_state);

  return device;
}
