#include <geekos/vmm_stubs.h>
#include <palacios/vmm.h>
#include <geekos/debug.h>
#include <geekos/serial.h>
#include <geekos/vm.h>
#include <geekos/screen.h>
#include <palacios/vmm_dev_mgr.h>
#include <devices/nvram.h>
#include <devices/timer.h>
#include <devices/simple_pic.h>
#include <devices/8259a.h>
#include <palacios/vmm_intr.h>


#define SPEAKER_PORT 0x61




inline void VM_Out_Byte(ushort_t port, uchar_t value)
{
    __asm__ __volatile__ (
	"outb %b0, %w1"
	:
	: "a" (value), "Nd" (port)
    );
}

/*
 * Read a byte from an I/O port.
 */
inline uchar_t VM_In_Byte(ushort_t port)
{
    uchar_t value;

    __asm__ __volatile__ (
	"inb %w1, %b0"
	: "=a" (value)
	: "Nd" (port)
    );

    return value;
}



int IO_Read(ushort_t port, void * dst, uint_t length, void * priv_data) {

  if (length != 1) {
    return 0;
  }

  *(uchar_t*)dst = VM_In_Byte(port);    
  return 1;
}



int IO_Write(ushort_t port, void * src, uint_t length, void * priv_data) {

  if (length != 1) {
    return 0;
  }

  VM_Out_Byte(port, *(uchar_t *)src);    

  return 1;
}


int IO_Read_to_Serial(ushort_t port, void * dst, uint_t length, void * priv_data) {
  PrintBoth("Input from Guest on port %d (0x%x) Length=%d\n", port, port, length);
  
  return 0;
}


char * bochs_debug_buf = NULL;
int bochs_debug_offset = 0;


int IO_BOCHS_debug(ushort_t port, void * src, uint_t length, void * priv_data) {
  if (!bochs_debug_buf) {
    bochs_debug_buf = (char*)Malloc(1024);
  }

  bochs_debug_buf[bochs_debug_offset++] = *(char*)src;

  if ((*(char*)src == 0xa) ||  (bochs_debug_offset == 1023)) {
    SerialPrint("BOCHS>%s", bochs_debug_buf);
    memset(bochs_debug_buf, 0, 1024);
    bochs_debug_offset = 0;
  }

  return length;
}


int IO_Write_to_Serial(ushort_t port, void * src, uint_t length, void * priv_data) {
 SerialPrint("Output from Guest on port %d (0x%x) Length=%d\n", port, port, length);
  switch (length) {

  case 1:
    SerialPrint(">0x%.2x\n", *(char*)src);
    break;
  case 2:
    SerialPrint(">0x%.4x\n", *(ushort_t*)src);
    break;
  case 4:
    SerialPrint(">0x%.8x\n", *(uint_t*)src);
    break;
  default:
    break;
  }

  //  SerialMemDump(src, length);
  return length;
}



void BuzzVM()
{
  int x;
  int j;
  unsigned char init;

#if 0  
  __asm__ __volatile__ (
    "popf"
    );
    
#endif
    
  PrintBoth("Starting To Buzz\n");

  init=VM_In_Byte(SPEAKER_PORT);

  while (1) {
    VM_Out_Byte(SPEAKER_PORT, init|0x2);
    for (j=0;j<1000000;j++) { 
      x+=j;
    }
    VM_Out_Byte(SPEAKER_PORT, init);
    for (j=0;j<1000000;j++) { 
      x+=j;
    }
  }
}






int RunVMM(struct Boot_Info * bootInfo) {

    struct vmm_os_hooks os_hooks;
    struct vmm_ctrl_ops vmm_ops;
    struct guest_info vm_info;
    addr_t rsp;
    addr_t rip;



    memset(&os_hooks, 0, sizeof(struct vmm_os_hooks));
    memset(&vmm_ops, 0, sizeof(struct vmm_ctrl_ops));
    memset(&vm_info, 0, sizeof(struct guest_info));

    os_hooks.print_debug = &SerialPrint;
    os_hooks.print_info = &Print;
    os_hooks.print_trace = &SerialPrint;
    os_hooks.allocate_pages = &Allocate_VMM_Pages;
    os_hooks.free_page = &Free_VMM_Page;
    os_hooks.malloc = &VMM_Malloc;
    os_hooks.free = &VMM_Free;
    os_hooks.vaddr_to_paddr = &Identity;
    os_hooks.paddr_to_vaddr = &Identity;
    os_hooks.hook_interrupt = &hook_irq_stub;
    os_hooks.ack_irq = &ack_irq;
 
    //   DumpGDT();
    Init_VMM(&os_hooks, &vmm_ops);
  
    init_shadow_map(&(vm_info.mem_map));
    init_shadow_page_state(&(vm_info.shdw_pg_state));
    vm_info.page_mode = SHADOW_PAGING;

    vm_info.cpu_mode = REAL;

    //init_irq_map(&(vm_info.irq_map));
    init_vmm_io_map(&(vm_info.io_map));
    init_interrupt_state(&vm_info);

    dev_mgr_init(&(vm_info.dev_mgr));
    
    if (0) {
      
      //    add_shared_mem_range(&(vm_info.mem_layout), 0, 0x800000, 0x10000);    
      //    add_shared_mem_range(&(vm_info.mem_layout), 0, 0x1000000, 0);
      
      rip = (ulong_t)(void*)&BuzzVM;
      //  rip -= 0x10000;
      //    rip = (addr_t)(void*)&exit_test;
      //  rip -= 0x2000;
      vm_info.rip = rip;
      rsp = (addr_t)Alloc_Page();
      
      vm_info.vm_regs.rsp = (rsp +4092 );// - 0x2000;
      
            
    } else if (0) {
      //add_shared_mem_range(&(vm_info.mem_layout), 0x0, 0x1000, 0x100000);
      //      add_shared_mem_range(&(vm_info.mem_layout), 0x0, 0x100000, 0x0);
      
      /*
	shadow_region_t *ent = Malloc(sizeof(shadow_region_t));;
	init_shadow_region_physical(ent,0,0x100000,GUEST_REGION_PHYSICAL_MEMORY,
	0x100000, HOST_REGION_PHYSICAL_MEMORY);
	add_shadow_region(&(vm_info.mem_map),ent);
      */

      add_shadow_region_passthrough(&vm_info, 0x0, 0x100000, 0x100000);

      hook_io_port(&(vm_info.io_map), 0x61, &IO_Read, &IO_Write, NULL);
      hook_io_port(&(vm_info.io_map), 0x05, &IO_Read, &IO_Write_to_Serial, NULL);
      
      /*
	vm_info.cr0 = 0;
	vm_info.cs.base=0xf000;
	vm_info.cs.limit=0xffff;
      */
      //vm_info.rip = 0xfff0;

      vm_info.rip = 0;
      vm_info.vm_regs.rsp = 0x0;
    } else {
      int i;
      void * region_start;

 
      PrintDebug("Guest Size: %lu\n", bootInfo->guest_size);

      struct guest_mem_layout * layout = (struct guest_mem_layout *)0x100000;

      if (layout->magic != MAGIC_CODE) {
	PrintDebug("Layout Magic Mismatch (0x%x)\n", layout->magic);
      }

      PrintDebug("%d layout regions\n", layout->num_regions);

      region_start = (void *)&(layout->regions[layout->num_regions]);

      PrintDebug("region start = 0x%x\n", region_start);

      for (i = 0; i < layout->num_regions; i++) {
	struct layout_region * reg = &(layout->regions[i]);
	uint_t num_pages = (reg->length / PAGE_SIZE) + ((reg->length % PAGE_SIZE) ? 1 : 0);
	void * guest_mem = Allocate_VMM_Pages(num_pages);

	PrintDebug("Layout Region %d bytes\n", reg->length);
	memcpy(guest_mem, region_start, reg->length);
	
	SerialMemDump((unsigned char *)(guest_mem), 16);

	add_shadow_region_passthrough(&vm_info, reg->final_addr, reg->final_addr + (num_pages * PAGE_SIZE), (addr_t)guest_mem);

	PrintDebug("Adding Shadow Region (0x%x-0x%x) -> 0x%x\n", reg->final_addr, reg->final_addr + (num_pages * PAGE_SIZE), guest_mem);

	region_start += reg->length;
      }
      
      //     
      add_shadow_region_passthrough(&vm_info, 0x0, 0xa0000, (addr_t)Allocate_VMM_Pages(160));
      add_shadow_region_passthrough(&vm_info, 0xa0000, 0xc0000, 0xa0000); 
      

      if (1) {
	add_shadow_region_passthrough(&vm_info, 0xc7000, 0xc8000, (addr_t)Allocate_VMM_Pages(1));
	if (add_shadow_region_passthrough(&vm_info, 0xc8000, 0xf0000, (addr_t)Allocate_VMM_Pages(40)) == -1) {
	  PrintDebug("Error adding shadow region\n");
	}
      } else {
	add_shadow_region_passthrough(&vm_info, 0xc0000, 0xc8000, 0xc0000);
	add_shadow_region_passthrough(&vm_info, 0xc8000, 0xf0000, 0xc8000);
      }


      print_shadow_map(&(vm_info.mem_map));

      hook_io_port(&(vm_info.io_map), 0x61, &IO_Read, &IO_Write, NULL);
      hook_io_port(&(vm_info.io_map), 0x05, &IO_Read, &IO_Write_to_Serial, NULL);

      /*
	hook_io_port(&(vm_info.io_map), 0x20, &IO_Read, &IO_Write_to_Serial, NULL);
	hook_io_port(&(vm_info.io_map), 0x21, &IO_Read, &IO_Write_to_Serial, NULL);
	hook_io_port(&(vm_info.io_map), 0xa0, &IO_Read, &IO_Write_to_Serial, NULL);
	hook_io_port(&(vm_info.io_map), 0xa1, &IO_Read, &IO_Write_to_Serial, NULL);
      */
      hook_io_port(&(vm_info.io_map), 0x400, &IO_Read, &IO_Write_to_Serial, NULL);
      hook_io_port(&(vm_info.io_map), 0x401, &IO_Read, &IO_Write_to_Serial, NULL);
      hook_io_port(&(vm_info.io_map), 0x402, &IO_Read, &IO_BOCHS_debug, NULL);
      hook_io_port(&(vm_info.io_map), 0x403, &IO_Read, &IO_Write_to_Serial, NULL);

      {
	struct vm_device * nvram = create_nvram();
	//struct vm_device * timer = create_timer();
	struct vm_device * pic = create_pic();

	attach_device(&(vm_info), nvram);
	//attach_device(&(vm_info), timer);
	attach_device(&(vm_info), pic);

	PrintDebugDevMgr(&(vm_info.dev_mgr));
      }

      hook_irq(&vm_info, 6);
      hook_irq(&vm_info, 14);
      hook_irq(&vm_info, 15);

      vm_info.rip = 0xfff0;
      vm_info.vm_regs.rsp = 0x0;
    }

    PrintBoth("Initializing Guest (eip=0x%.8x) (esp=0x%.8x)\n", (uint_t)vm_info.rip,(uint_t)vm_info.vm_regs.rsp);
    (vmm_ops).init_guest(&vm_info);
    PrintBoth("Starting Guest\n");
    //Clear_Screen();
    (vmm_ops).start_guest(&vm_info);

    return 0;
}
