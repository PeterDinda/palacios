#include <geekos/malloc.h>
#include <geekos/pci.h>
#include <geekos/io.h>
#include <geekos/debug.h>

#define PCI_CONFIG_ADDRESS 0xcf8  // 32 bit, little endian
#define PCI_CONFIG_DATA    0xcfc  // 32 bit, little endian

#define PCI_MAX_NUM_BUSES  4


struct pci_device_config {
  ushort_t   device_id;
  ushort_t   vendor_id;
  ushort_t   status;
  ushort_t   command;
  uchar_t    class_code;
  uchar_t    subclass;
  ushort_t   revision;
  uchar_t    BIST;
  uchar_t    header_type;
  uchar_t    latency_time;
  uchar_t    cache_line_size;
  uint_t     BAR[6];
  uint_t     cardbus_cis_pointer;
  ushort_t   subsystem_id;
  ushort_t   subsystem_vendor_id;
  uint_t     expansion_rom_address;
  uint_t     reserved[2];
  uchar_t    max_latency;
  uchar_t    min_grant;
  uchar_t    intr_pin;
  uchar_t    intr_line;
}  __attribute__((__packed__)) ;

struct pci_bus {
  uint_t number;
  struct pci_bus *next;

  struct pci_device *device_list; //

};

struct pci_device {
  uint_t number;
  struct pci_bus    *bus;
  struct pci_device *next;
  
  struct pci_device_config config;
};

struct pci {
  uint_t          num_buses;
  struct pci_bus *bus_list;
};
    

static uint_t ReadPCIDWord(uint_t bus, uint_t dev, uint_t func, uint_t offset)
{
   uint_t address;
   uint_t data;
 
   address = ((bus << 16) | (dev << 11) |
	      (func << 8) | (offset & 0xfc) | ((uint_t)0x80000000));
 
   Out_DWord(PCI_CONFIG_ADDRESS,address);
   data=In_DWord(PCI_CONFIG_DATA);

   return data;
}

#if 0

static ushort_t ReadPCIWord(uint_t bus, uint_t dev, uint_t func, uint_t offset)
{
   uint_t address;
   uint_t data;
 
   address = ((bus << 16) | (dev << 11) |
             (func << 8) | (offset & 0xfc) | ((uint_t)0x80000000));
 
   Out_DWord(PCI_CONFIG_ADDRESS,address);
   data=In_DWord(PCI_CONFIG_DATA);

   //PrintBoth("PCI: [0x%x] = 0x%x\n",address,data);


   return (ushort_t) ((data>>((offset&0x2)*8)) & 0xffff);
}

#endif

static ushort_t ReadPCIWord(uint_t bus, uint_t dev, uint_t func, uint_t offset)
{
  return (ushort_t) (ReadPCIDWord(bus,dev,func,offset)>>((offset&0x2)*8));
}

#if 0  // not currently used
static uchar_t ReadPCIByte(uint_t bus, uint_t dev, uint_t func, uint_t offset)
{
  return (uchar_t) (ReadPCIDWord(bus,dev,func,offset)>>((offset&0x3)*8));
}
#endif

static struct pci *NewPCI()
{
  struct pci *p = (struct pci *)Malloc(sizeof(struct pci));
  p->bus_list=NULL;
  p->num_buses=0;
  return p;
}

static void AddPCIBus(struct pci *p, struct pci_bus *bus) 
{
  bus->next = p->bus_list;
  p->bus_list=bus;
}
    

static struct pci_bus *NewPCIBus(struct pci *p)
{
  struct pci_bus *pb = (struct pci_bus *)Malloc(sizeof(struct pci_bus));
  pb->device_list=NULL;
  pb->number = (p->num_buses);
  p->num_buses++;
  return pb;
}

static void AddPCIDevice(struct pci_bus *b, struct pci_device *d)
{
  d->bus=b;
  d->next=b->device_list;
  b->device_list=d;
}
 
static struct pci_device *NewPCIDevice(struct pci_bus *pb) 
{
  struct pci_device *pd = (struct pci_device *)Malloc(sizeof(struct pci_device));
  pd->number=0;
  pd->bus=NULL;
  pd->next=NULL;
  return pd;
}

static void GetPCIDeviceConfig(uint_t bus,
			       uint_t dev,
			       struct pci_device *d)
{
  uint_t numdwords=sizeof(struct pci_device_config) / 4;
  uint_t i;

  uint_t *p = (uint_t *) (&(d->config));

  for (i=0;i<numdwords;i++) {
    p[i]=ReadPCIDWord(bus,dev,0,i*4);
  }
}



static struct pci *ScanPCI()
{
  uint_t bus, dev;
  ushort_t vendor;

  struct pci *thepci = NewPCI();
  struct pci_bus *thebus;
 
  for (bus=0;bus<PCI_MAX_NUM_BUSES;bus++) {
    // Are there any devices on the bus?
    for (dev=0;dev<32;dev++) { 
      vendor=ReadPCIWord(bus,dev,0,0);
      if (vendor!=0xffff) { 
	break;
      }
    }
    if (dev==32) { 
      continue;
    }
    // There are devices.  Create a bus.
    thebus = NewPCIBus(thepci);
    thebus->number=bus;
    // Add the devices to the bus
    for (dev=0;dev<32;dev++) { 
      vendor=ReadPCIWord(bus,dev,0,0);
      if (vendor!=0xffff) { 
	struct pci_device *thedev=NewPCIDevice(thebus);
	thedev->number=dev;
	GetPCIDeviceConfig(bus,dev,thedev);
	AddPCIDevice(thebus,thedev);
      }
    }
    AddPCIBus(thepci,thebus);
  }
  return thepci;
}

static void PrintPCIDevice(struct pci_device *thedev)
{
  PrintBoth("    PCI Device: \n");
  PrintBoth("     Slot: %u\n",thedev->number);
  PrintBoth("      device_id:        0x%x\n", (uint_t) thedev->config.device_id);
  PrintBoth("      vendor_id:        0x%x\n", (uint_t) thedev->config.vendor_id);
  PrintBoth("      status:           0x%x\n", (uint_t) thedev->config.status);
  PrintBoth("      command:          0x%x\n", (uint_t) thedev->config.command);
  PrintBoth("      class_code:       0x%x\n", (uint_t) thedev->config.class_code);
  PrintBoth("      subclass:         0x%x\n", (uint_t) thedev->config.subclass);
  PrintBoth("      revision:         0x%x\n", (uint_t) thedev->config.revision);
  PrintBoth("      BIST:             0x%x\n", (uint_t) thedev->config.BIST);
  PrintBoth("      header_type:      0x%x\n", (uint_t) thedev->config.header_type);
  PrintBoth("      latency_time:     0x%x\n", (uint_t) thedev->config.latency_time);
  PrintBoth("      cache_line_size:  0x%x\n", (uint_t) thedev->config.cache_line_size);
  PrintBoth("      BAR[0]:           0x%x\n", (uint_t) thedev->config.BAR[0]);
  PrintBoth("      BAR[1]:           0x%x\n", (uint_t) thedev->config.BAR[1]);
  PrintBoth("      BAR[2]:           0x%x\n", (uint_t) thedev->config.BAR[2]);
  PrintBoth("      BAR[3]:           0x%x\n", (uint_t) thedev->config.BAR[3]);
  PrintBoth("      BAR[4]:           0x%x\n", (uint_t) thedev->config.BAR[4]);
  PrintBoth("      BAR[5]:           0x%x\n", (uint_t) thedev->config.BAR[5]);
  PrintBoth("      cardbus_cis_ptr:  0x%x\n", (uint_t) thedev->config.cardbus_cis_pointer);
  PrintBoth("      subsystem_id:     0x%x\n", (uint_t) thedev->config.subsystem_id);
  PrintBoth("      subsystem_vendor: 0x%x\n", (uint_t) thedev->config.subsystem_vendor_id);
  PrintBoth("      exp_rom_address:  0x%x\n", (uint_t) thedev->config.expansion_rom_address);
  PrintBoth("      reserved[0]:      0x%x\n", (uint_t) thedev->config.reserved[0]);
  PrintBoth("      reserved[1]:      0x%x\n", (uint_t) thedev->config.reserved[1]);
  PrintBoth("      max_latency:      0x%x\n", (uint_t) thedev->config.max_latency);
  PrintBoth("      min_grant:        0x%x\n", (uint_t) thedev->config.min_grant);
  PrintBoth("      intr_pin:         0x%x\n", (uint_t) thedev->config.intr_pin);
  PrintBoth("      intr_line:        0x%x\n", (uint_t) thedev->config.intr_line);
}

static void PrintPCIBus(struct pci_bus *thebus)
{
  struct pci_device *thedev;

  PrintBoth("  PCI Bus:\n");
  PrintBoth("   Number: %u\n",thebus->number);

  thedev=thebus->device_list;

  while (thedev) { 
    PrintPCIDevice(thedev);
    thedev=thedev->next;
  }
}

static void PrintPCI(struct pci *thepci)
{
  struct pci_bus *thebus;

  PrintBoth("PCI Configuration:\n");
  PrintBoth(" Number of Buses: %u\n",thepci->num_buses);
  thebus=thepci->bus_list;
  while (thebus) { 
    PrintPCIBus(thebus);
    thebus=thebus->next;
  }

}

int Init_PCI()
{
  PrintPCI(ScanPCI());

  return 0;

}
