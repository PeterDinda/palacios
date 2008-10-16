/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmcb.h>
#include <palacios/vmm_ctrl_regs.h>

static const char VMMCALL[3] = {0x0f, 0x01, 0xd9};

#ifndef DEBUG_EMULATOR
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


int v3_init_emulator(struct guest_info * info) {
  struct emulation_state * emulator = &(info->emulator);

  emulator->num_emulated_pages = 0;
  INIT_LIST_HEAD(&(emulator->emulated_pages));


  emulator->num_saved_pages = 0;
  INIT_LIST_HEAD(&(emulator->saved_pages));
  
  emulator->num_write_regions = 0;
  INIT_LIST_HEAD(&(emulator->write_regions));

  emulator->running = 0;
  emulator->instr_length = 0;

  emulator->tf_enabled = 0;

  return 0;
}

static addr_t get_new_page() {
  void * page = V3_AllocPages(1);
  memset(page, 0, PAGE_SIZE);

  return (addr_t)page;
}

/*
static int setup_code_page(struct guest_info * info, char * instr, struct basic_instr_info * instr_info ) {
  addr_t code_page_offset = PT32_PAGE_OFFSET(info->rip);
  addr_t code_page = get_new_page();
  struct emulated_page * new_code_page = V3_Malloc(sizeof(struct emulated_page));
  struct saved_page * saved_code_page = V3_Malloc(sizeof(struct saved_page));


  saved_code_page->va = PT32_PAGE_ADDR(info->rip);

  new_code_page->page_addr = code_page; 
  new_code_page->va = PT32_PAGE_ADDR(info->rip);

  new_code_page->pte.present = 1;
  new_code_page->pte.writable = 0;
  new_code_page->pte.user_page = 1;
  new_code_page->pte.page_base_addr = PT32_BASE_ADDR(code_page);

  memcpy((void *)(code_page + code_page_offset), instr, instr_info->instr_length);
  memcpy((void *)(code_page + code_page_offset + instr_info->instr_length), VMMCALL, 3);

#ifdef DEBUG_EMULATOR
  PrintDebug("New Instr Stream:\n");
  PrintTraceMemDump((void *)(code_page + code_page_offset), 32);
  PrintDebug("rip =%x\n", info->rip);
#endif



  v3_replace_shdw_page32(info, new_code_page->va, &(new_code_page->pte), &(saved_code_page->pte));


  list_add(&(new_code_page->page_list), &(info->emulator.emulated_pages));
  info->emulator.num_emulated_pages++;

  list_add(&(saved_code_page->page_list), &(info->emulator.saved_pages));
  info->emulator.num_saved_pages++;

  return 0;
}
*/


static int set_stepping(struct guest_info * info) {
  vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));
  ctrl_area->exceptions.db = 1;

  info->emulator.tf_enabled = ((struct rflags *)&(info->ctrl_regs.rflags))->tf;

  ((struct rflags *)&(info->ctrl_regs.rflags))->tf = 1;

  return 0;
}


static int unset_stepping(struct guest_info * info) {
  vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));
  ctrl_area->exceptions.db = 0;

  ((struct rflags *)&(info->ctrl_regs.rflags))->tf = info->emulator.tf_enabled;

  if (info->emulator.tf_enabled) {
    // Inject breakpoint exception into guest
  }

  return 0;

}


// get the current instr
// check if rep + remove
// put into new page, vmexit after
// replace new page with current eip page
// 
int v3_emulate_memory_read(struct guest_info * info, addr_t read_gva, 
			   int (*read)(addr_t read_addr, void * dst, uint_t length, void * priv_data), 
			   addr_t read_gpa, void * private_data) {
  struct basic_instr_info instr_info;
  uchar_t instr[15];
  int ret;
  struct emulated_page * data_page = V3_Malloc(sizeof(struct emulated_page));
  addr_t data_addr_offset = PT32_PAGE_OFFSET(read_gva);
  pte32_t saved_pte;

  PrintDebug("Emulating Read\n");

  if (info->mem_mode == PHYSICAL_MEM) { 
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  } else { 
    ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  }

  if (ret == -1) {
    PrintError("Could not read guest memory\n");
    return -1;
  }

#ifdef DEBUG_EMULATOR
  PrintDebug("Instr (15 bytes) at %x:\n", instr);
  PrintTraceMemDump(instr, 15);
#endif  


  if (v3_basic_mem_decode(info, (addr_t)instr, &instr_info) == -1) {
    PrintError("Could not do a basic memory instruction decode\n");
    V3_Free(data_page);
    return -1;
  }

  /*
  if (instr_info.has_rep == 1) {
    PrintError("We currently don't handle rep* instructions\n");
    V3_Free(data_page);
    return -1;
  }
  */

  data_page->page_addr = get_new_page();
  data_page->va = PT32_PAGE_ADDR(read_gva);
  data_page->pte.present = 1;
  data_page->pte.writable = 0;
  data_page->pte.user_page = 1;
  data_page->pte.page_base_addr = PT32_BASE_ADDR(data_page->page_addr);


  // Read the data directly onto the emulated page
  ret = read(read_gpa, (void *)(data_page->page_addr + data_addr_offset), instr_info.op_size, private_data);
  if ((ret == -1) || ((uint_t)ret != instr_info.op_size)) {
    PrintError("Read error in emulator\n");
    V3_FreePage((void *)(data_page->page_addr));
    V3_Free(data_page);
    return -1;
  }

  v3_replace_shdw_page32(info, data_page->va, &(data_page->pte), &saved_pte);


  list_add(&(data_page->page_list), &(info->emulator.emulated_pages));
  info->emulator.num_emulated_pages++;

  if (saved_pte.present == 1) {
    struct saved_page * saved_data_page = V3_Malloc(sizeof(struct saved_page));
    saved_data_page->pte = saved_pte;
    saved_data_page->va = PT32_PAGE_ADDR(read_gva);

    list_add(&(saved_data_page->page_list), &(info->emulator.saved_pages));
    info->emulator.num_saved_pages++;
  }


  // setup_code_page(info, instr, &instr_info);
  set_stepping(info);

  info->emulator.running = 1;
  info->run_state = VM_EMULATING;
  info->emulator.instr_length = instr_info.instr_length;

  return 0;
}



int v3_emulate_memory_write(struct guest_info * info, addr_t write_gva,
			    int (*write)(addr_t write_addr, void * src, uint_t length, void * priv_data), 
			    addr_t write_gpa, void * private_data) {

  struct basic_instr_info instr_info;
  uchar_t instr[15];
  int ret;
  struct write_region * write_op = V3_Malloc(sizeof(struct write_region ));
  struct emulated_page * data_page = V3_Malloc(sizeof(struct emulated_page));
  addr_t data_addr_offset = PT32_PAGE_OFFSET(write_gva);
  pte32_t saved_pte;
  int i;

  PrintDebug("Emulating Write for instruction at 0x%x\n",info->rip);

  if (info->mem_mode == PHYSICAL_MEM) { 
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  } else { 
    ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  }


  PrintDebug("Instruction is");
  for (i=0;i<15;i++) { PrintDebug(" 0x%x",instr[i]); } 
  PrintDebug("\n");
  
  if (v3_basic_mem_decode(info, (addr_t)instr, &instr_info) == -1) {
    PrintError("Could not do a basic memory instruction decode\n");
    V3_Free(write_op);
    V3_Free(data_page);
    return -1;
  }

  if (instr_info.has_rep==1) { 
    PrintDebug("Emulated instruction has rep\n");
  }

  /*
  if (instr_info.has_rep == 1) {
    PrintError("We currently don't handle rep* instructions\n");
    V3_Free(write_op);
    V3_Free(data_page);
    return -1;
  }
  */

  data_page->page_addr = get_new_page();
  data_page->va = PT32_PAGE_ADDR(write_gva);
  data_page->pte.present = 1;
  data_page->pte.writable = 1;
  data_page->pte.user_page = 1;
  data_page->pte.page_base_addr = PT32_BASE_ADDR(data_page->page_addr);



  write_op->write = write;
  write_op->write_addr = write_gpa;
  write_op->length = instr_info.op_size;
  write_op->private_data = private_data;

  write_op->write_data = (void *)(data_page->page_addr + data_addr_offset);

  list_add(&(write_op->write_list), &(info->emulator.write_regions));
  info->emulator.num_write_regions--;

  v3_replace_shdw_page32(info, data_page->va, &(data_page->pte), &saved_pte);


  list_add(&(data_page->page_list), &(info->emulator.emulated_pages));
  info->emulator.num_emulated_pages++;

  if (saved_pte.present == 1) {
    struct saved_page * saved_data_page = V3_Malloc(sizeof(struct saved_page));
    saved_data_page->pte = saved_pte;
    saved_data_page->va = PT32_PAGE_ADDR(write_gva);

    list_add(&(saved_data_page->page_list), &(info->emulator.saved_pages));
    info->emulator.num_saved_pages++;
  }


  if (info->emulator.running == 0) {
    //    setup_code_page(info, instr, &instr_info);
    set_stepping(info);
    info->emulator.running = 1;
    info->run_state = VM_EMULATING;
    info->emulator.instr_length = instr_info.instr_length;
  }

  return 0;
}


// end emulation
int v3_emulation_exit_handler(struct guest_info * info) {
  struct saved_page * svpg, * p_svpg;
  struct emulated_page * empg, * p_empg;
  struct write_region * wr_reg, * p_wr_reg;
  pte32_t dummy_pte;

  // Complete the writes
  // delete writes
  // swap out emulated pages with blank dummies
  // swap in saved pages
  // increment rip
  
  PrintDebug("V3 Emulation Exit Handler\n");

  list_for_each_entry_safe(wr_reg, p_wr_reg, &(info->emulator.write_regions), write_list) {
    wr_reg->write(wr_reg->write_addr, wr_reg->write_data, wr_reg->length, wr_reg->private_data);
    PrintDebug("Writing \n");
    
    list_del(&(wr_reg->write_list));
    V3_Free(wr_reg);

  }
  info->emulator.num_write_regions = 0;


  *(uint_t *)&dummy_pte = 0;
  
  list_for_each_entry_safe(empg, p_empg, &(info->emulator.emulated_pages), page_list) {
    pte32_t empte32_t;

    PrintDebug("wiping page %x\n", empg->va); 

    v3_replace_shdw_page32(info, empg->va, &dummy_pte, &empte32_t);
    V3_FreePage((void *)(empg->page_addr));

    list_del(&(empg->page_list));
    V3_Free(empg);
  }
  info->emulator.num_emulated_pages = 0;

  list_for_each_entry_safe(svpg, p_svpg, &(info->emulator.saved_pages), page_list) {

    PrintDebug("Setting Saved page %x back\n", svpg->va); 
    v3_replace_shdw_page32(info, empg->va, &(svpg->pte), &dummy_pte);
    
    list_del(&(svpg->page_list));
    V3_Free(svpg);
  }
  info->emulator.num_saved_pages = 0;

  info->run_state = VM_RUNNING;
  info->emulator.running = 0;
  //info->rip += info->emulator.instr_length;


  PrintDebug("Returning to rip: 0x%x\n", info->rip);

  info->emulator.instr_length = 0;
  
  
  unset_stepping(info);


  PrintDebug("returning from emulation\n");

  return 0;
}
