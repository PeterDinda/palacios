/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */


#include <palacios/vmm_config.h>
#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>


#include <devices/serial.h>
#include <devices/keyboard.h>
#include <devices/8259a.h>
#include <devices/8254.h>
#include <devices/nvram.h>
#include <devices/generic.h>
#include <devices/ramdisk.h>


#define USE_GENERIC 1
#define USE_RAMDISK 0



static int mem_test_read(addr_t guest_addr, void * dst, uint_t length, void * priv_data) {
  int foo = 20;


  memcpy(dst, &foo, length);

  PrintDebug("Passthrough mem read returning: %d (length=%d)\n", foo + (guest_addr & 0xfff), length);
  return length;
}

static int passthrough_mem_read(addr_t guest_addr, void * dst, uint_t length, void * priv_data) {
    memcpy(dst, (void*)guest_addr, length);
    return length;
}

static int passthrough_mem_write(addr_t guest_addr, void * src, uint_t length, void * priv_data) {
  memcpy((void*)guest_addr, src, length);
  return length;
}


/*static int IO_Read(ushort_t port, void * dst, uint_t length, void * priv_data) {

  struct guest_info * info = priv_data;
  ulong_t tsc_spread = 0;
  ullong_t exit_tsc = 0;

  
  *(ulong_t *)(&exit_tsc) = info->vm_regs.rbx;
  *(ulong_t *)((&exit_tsc) + 4) = info->vm_regs.rcx; 
  tsc_spread = info->exit_tsc - exit_tsc;
  
  PrintError("IOREAD tsc diff = %lu\n",tsc_spread); 
  info->rip += 3;


  return 1;
}
*/

int config_guest(struct guest_info * info, void * config_ptr) {

  struct guest_mem_layout * layout = (struct guest_mem_layout *)config_ptr;
  extern v3_cpu_arch_t v3_cpu_type;
  void * region_start;
  int i;

  int use_ramdisk = USE_RAMDISK;
  int use_generic = USE_GENERIC;


  v3_init_time(info);
  init_shadow_map(info);
  
  if (v3_cpu_type == V3_SVM_REV3_CPU) {
    info->shdw_pg_mode = NESTED_PAGING;
  } else {
    init_shadow_page_state(info);
    info->shdw_pg_mode = SHADOW_PAGING;
  }
  
  info->cpu_mode = REAL;
  info->mem_mode = PHYSICAL_MEM;
  
 
  init_vmm_io_map(info);
  init_interrupt_state(info);
  
  dev_mgr_init(info);

  init_emulator(info);
  
 
  //     SerialPrint("Guest Mem Dump at 0x%x\n", 0x100000);
  //PrintDebugMemDump((unsigned char *)(0x100000), 261 * 1024);
  if (layout->magic != MAGIC_CODE) {
    
    PrintDebug("Layout Magic Mismatch (0x%x)\n", layout->magic);
    return -1;
  }
  
  PrintDebug("%d layout regions\n", layout->num_regions);
  
  region_start = (void *)&(layout->regions[layout->num_regions]);
  
  PrintDebug("region start = 0x%x\n", region_start);
  
  for (i = 0; i < layout->num_regions; i++) {
    struct layout_region * reg = &(layout->regions[i]);
    uint_t num_pages = (reg->length / PAGE_SIZE) + ((reg->length % PAGE_SIZE) ? 1 : 0);
    void * guest_mem = V3_AllocPages(num_pages);
    
    PrintDebug("Layout Region %d bytes\n", reg->length);
    memcpy(guest_mem, region_start, reg->length);
    
    PrintDebugMemDump((unsigned char *)(guest_mem), 16);
    
    add_shadow_region_passthrough(info, reg->final_addr, reg->final_addr + (num_pages * PAGE_SIZE), (addr_t)guest_mem);
    
    PrintDebug("Adding Shadow Region (0x%x-0x%x) -> 0x%x\n", reg->final_addr, reg->final_addr + (num_pages * PAGE_SIZE), guest_mem);
    
    region_start += reg->length;
  }
  
      //     
  add_shadow_region_passthrough(info, 0x0, 0xa0000, (addr_t)V3_AllocPages(160));
  
  if (1) {
    add_shadow_region_passthrough(info, 0xa0000, 0xc0000, 0xa0000); 
  } else {
    hook_guest_mem(info, 0xa0000, 0xc0000, passthrough_mem_read, passthrough_mem_write, NULL);
  }  
  
  // TEMP
  //add_shadow_region_passthrough(info, 0xc0000, 0xc8000, 0xc0000);
  
  if (1) {
    add_shadow_region_passthrough(info, 0xc7000, 0xc8000, (addr_t)V3_AllocPages(1));
    if (add_shadow_region_passthrough(info, 0xc8000, 0xf0000, (addr_t)V3_AllocPages(40)) == -1) {
      PrintDebug("Error adding shadow region\n");
    }
  } else {
    add_shadow_region_passthrough(info, 0xc0000, 0xc8000, 0xc0000);
    add_shadow_region_passthrough(info, 0xc8000, 0xf0000, 0xc8000);
  }
  
  
  if (1) {
  add_shadow_region_passthrough(info, 0x100000, 0x1000000, (addr_t)V3_AllocPages(4096));
  } else {
    /* MEMORY HOOK TEST */
    add_shadow_region_passthrough(info, 0x100000, 0xa00000, (addr_t)V3_AllocPages(2304));
    hook_guest_mem(info, 0xa00000, 0xa01000, mem_test_read, passthrough_mem_write, NULL); 
    add_shadow_region_passthrough(info, 0xa01000, 0x1000000, (addr_t)V3_AllocPages(1791));
  }

    add_shadow_region_passthrough(info, 0x1000000, 0x8000000, (addr_t)V3_AllocPages(32768));
 
  // test - give linux accesss to PCI space - PAD
  add_shadow_region_passthrough(info, 0xc0000000,0xffffffff,0xc0000000);
  
  
  print_shadow_map(&(info->mem_map));

  
  {
    struct vm_device * ramdisk = NULL;
    struct vm_device * nvram = create_nvram();
    //struct vm_device * timer = create_timer();
    struct vm_device * pic = create_pic();
    struct vm_device * keyboard = create_keyboard();
    struct vm_device * pit = create_pit(); 
    //struct vm_device * serial = create_serial();
    struct vm_device * generic = NULL;
    //Zheng 09/29/2008


    if (use_ramdisk) {
      PrintDebug("Creating Ramdisk\n");
      ramdisk = create_ramdisk();
    }
    
    
    if (use_generic) {
      PrintDebug("Creating Generic Device\n");
      generic = create_generic();
      
      // Make the DMA controller invisible
      v3_generic_add_port_range(generic, 0x00, 0x07, GENERIC_PRINT_AND_IGNORE);   // DMA 1 channels 0,1,2,3 (address, counter)
      v3_generic_add_port_range(generic, 0xc0, 0xc7, GENERIC_PRINT_AND_IGNORE);   // DMA 2 channels 4,5,6,7 (address, counter)
      v3_generic_add_port_range(generic, 0x87, 0x87, GENERIC_PRINT_AND_IGNORE);   // DMA 1 channel 0 page register
      v3_generic_add_port_range(generic, 0x83, 0x83, GENERIC_PRINT_AND_IGNORE);   // DMA 1 channel 1 page register
      v3_generic_add_port_range(generic, 0x81, 0x81, GENERIC_PRINT_AND_IGNORE);   // DMA 1 channel 2 page register
      v3_generic_add_port_range(generic, 0x82, 0x82, GENERIC_PRINT_AND_IGNORE);   // DMA 1 channel 3 page register
      v3_generic_add_port_range(generic, 0x8f, 0x8f, GENERIC_PRINT_AND_IGNORE);   // DMA 2 channel 4 page register
      v3_generic_add_port_range(generic, 0x8b, 0x8b, GENERIC_PRINT_AND_IGNORE);   // DMA 2 channel 5 page register
      v3_generic_add_port_range(generic, 0x89, 0x89, GENERIC_PRINT_AND_IGNORE);   // DMA 2 channel 6 page register
      v3_generic_add_port_range(generic, 0x8a, 0x8a, GENERIC_PRINT_AND_IGNORE);   // DMA 2 channel 7 page register
      v3_generic_add_port_range(generic, 0x08, 0x0f, GENERIC_PRINT_AND_IGNORE);   // DMA 1 misc registers (csr, req, smask,mode,clearff,reset,enable,mmask)
      v3_generic_add_port_range(generic, 0xd0, 0xde, GENERIC_PRINT_AND_IGNORE);   // DMA 2 misc registers
      
      
      
      
      // Make the Serial ports invisible 
      
      v3_generic_add_port_range(generic, 0x3f8, 0x3f8+7, GENERIC_PRINT_AND_IGNORE);      // COM 1
      v3_generic_add_port_range(generic, 0x2f8, 0x2f8+7, GENERIC_PRINT_AND_IGNORE);      // COM 2
      

      
#ifndef DEBUG_RAMDISK
      
      v3_generic_add_port_range(generic, 0x3e8, 0x3e8+7, GENERIC_PRINT_AND_IGNORE);      // COM 3
      v3_generic_add_port_range(generic, 0x2e8, 0x2e8+7, GENERIC_PRINT_AND_IGNORE);      // COM 4
#endif
      
      

      // Make the PCI bus invisible (at least it's configuration)
      
      v3_generic_add_port_range(generic, 0xcf8, 0xcf8, GENERIC_PRINT_AND_IGNORE); // PCI Config Address
      v3_generic_add_port_range(generic, 0xcfc, 0xcfc, GENERIC_PRINT_AND_IGNORE); // PCI Config Data

      
      
      if (!use_ramdisk) {
	// Monitor the IDE controllers (very slow)
	v3_generic_add_port_range(generic, 0x170, 0x178, GENERIC_PRINT_AND_PASSTHROUGH); // IDE 1
	v3_generic_add_port_range(generic, 0x376, 0x377, GENERIC_PRINT_AND_PASSTHROUGH); // IDE 1
      }
      
#if 1
      v3_generic_add_port_range(generic, 0x1f0, 0x1f8, GENERIC_PRINT_AND_PASSTHROUGH); // IDE 0
      v3_generic_add_port_range(generic, 0x3f6, 0x3f7, GENERIC_PRINT_AND_PASSTHROUGH); // IDE 0
#endif
      
      
#if 0
      
      // Make the floppy controllers invisible
      
      v3_generic_add_port_range(generic, 0x3f0, 0x3f2, GENERIC_PRINT_AND_IGNORE); // Primary floppy controller (base,statusa/statusb,DOR)
      v3_generic_add_port_range(generic, 0x3f4, 0x3f5, GENERIC_PRINT_AND_IGNORE); // Primary floppy controller (mainstat/datarate,data)
      v3_generic_add_port_range(generic, 0x3f7, 0x3f7, GENERIC_PRINT_AND_IGNORE); // Primary floppy controller (DIR)
      v3_generic_add_port_range(generic, 0x370, 0x372, GENERIC_PRINT_AND_IGNORE); // Secondary floppy controller (base,statusa/statusb,DOR)
      v3_generic_add_port_range(generic, 0x374, 0x375, GENERIC_PRINT_AND_IGNORE); // Secondary floppy controller (mainstat/datarate,data)
      v3_generic_add_port_range(generic, 0x377, 0x377, GENERIC_PRINT_AND_IGNORE); // Secondary floppy controller (DIR)
      
#endif

#if 1

      // Make the parallel port invisible
      
      v3_generic_add_port_range(generic, 0x378, 0x37f, GENERIC_PRINT_AND_IGNORE);

#endif

#if 1

      // Monitor graphics card operations

      v3_generic_add_port_range(generic, 0x3b0, 0x3bb, GENERIC_PRINT_AND_PASSTHROUGH);
      v3_generic_add_port_range(generic, 0x3c0, 0x3df, GENERIC_PRINT_AND_PASSTHROUGH);
      
#endif


#if 1
      // Make the ISA PNP features invisible

      v3_generic_add_port_range(generic, 0x274, 0x277, GENERIC_PRINT_AND_IGNORE);
      v3_generic_add_port_range(generic, 0x279, 0x279, GENERIC_PRINT_AND_IGNORE);
      v3_generic_add_port_range(generic, 0xa79, 0xa79, GENERIC_PRINT_AND_IGNORE);
#endif


#if 1
      // Monitor any network card (realtek ne2000) operations 
      v3_generic_add_port_range(generic, 0xc100, 0xc1ff, GENERIC_PRINT_AND_PASSTHROUGH);
#endif


#if 1
      // Make any Bus master ide controller invisible
      
      v3_generic_add_port_range(generic, 0xc000, 0xc00f, GENERIC_PRINT_AND_IGNORE);
#endif
      
    }
      //  v3_generic_add_port_range(generic, 0x378, 0x400, GENERIC_PRINT_AND_IGNORE);
      


    

    v3_attach_device(info, nvram);
    //v3_attach_device(info, timer);
    v3_attach_device(info, pic);
    v3_attach_device(info, pit);
    v3_attach_device(info, keyboard);
    // v3_attach_device(info, serial);

    if (use_ramdisk) {
      v3_attach_device(info, ramdisk);
    }

    if (use_generic) {
      // Important that this be attached last!
      v3_attach_device(info, generic);
    }
    
    PrintDebugDevMgr(info);
  }
  
  // give keyboard interrupts to vm
  // no longer needed since we have a keyboard device
  //hook_irq(&vm_info, 1);
  
#if 0
  // give floppy controller to vm
  v3_hook_irq_for_guest_injection(info, 6);
#endif
  

  if (!use_ramdisk) {
    //primary ide
    v3_hook_irq_for_guest_injection(info, 14);
  
    // secondary ide
    v3_hook_irq_for_guest_injection(info, 15);    
  }  

  //v3_hook_io_port(info, 1234, &IO_Read, NULL, info);

  info->rip = 0xfff0;
  info->vm_regs.rsp = 0x0;
  

  return 0;
}
