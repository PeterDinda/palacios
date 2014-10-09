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


#include <palacios/vmm_debug.h>
#include <palacios/vmm.h>
#include <palacios/vmm_host_events.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_config.h>

#define PRINT_TELEMETRY  1
#define PRINT_CORE_STATE 2
#define PRINT_ARCH_STATE 3
#define PRINT_STACK      4
#define PRINT_BACKTRACE  5


#define PRINT_ALL        100 // Absolutely everything
#define PRINT_STATE      101 // telemetry, core state, arch state




static int core_handler(struct guest_info * core, uint32_t cmd) {


    switch (cmd) {
#ifdef V3_CONFIG_TELEMETRY
	case PRINT_TELEMETRY: 
	    v3_print_core_telemetry(core);
	    break;
#endif
	
	case PRINT_CORE_STATE:
	    v3_raise_barrier(core->vm_info, NULL);

	    v3_print_guest_state(core);

	    v3_lower_barrier(core->vm_info);
	    break;
	case PRINT_ARCH_STATE:
	    v3_raise_barrier(core->vm_info, NULL);

	    v3_print_arch_state(core);

	    v3_lower_barrier(core->vm_info);
	    break;
	case PRINT_STACK:
	    v3_raise_barrier(core->vm_info, NULL);

	    v3_print_stack(core);

	    v3_lower_barrier(core->vm_info);
	    break;
	case PRINT_BACKTRACE:
	    v3_raise_barrier(core->vm_info, NULL);

	    v3_print_backtrace(core);
	    
	    v3_lower_barrier(core->vm_info);
	    break;

	case PRINT_STATE:
	    v3_raise_barrier(core->vm_info, NULL);

#ifdef V3_CONFIG_TELEMETRY
	    v3_print_core_telemetry(core);
#endif
	    v3_print_guest_state(core);
	    v3_print_arch_state(core);

	    v3_lower_barrier(core->vm_info);
	    break;

	case PRINT_ALL:
	    v3_raise_barrier(core->vm_info, NULL);

#ifdef V3_CONFIG_TELEMETRY
	    v3_print_core_telemetry(core);
#endif
	    v3_print_guest_state(core);
	    v3_print_arch_state(core);
        v3_print_stack(core);
        v3_print_backtrace(core);

	    v3_lower_barrier(core->vm_info);
	    break;

    }

    return 0;
}


static int evt_handler(struct v3_vm_info * vm, struct v3_debug_event * evt, void * priv_data) {

    V3_Print(vm, VCORE_NONE,"Debug Event Handler for core %d cmd=%x\n", evt->core_id, evt->cmd);

    if (evt->core_id == -1) {
	int i = 0;
	for (i = 0; i < vm->num_cores; i++) {
	    core_handler(&(vm->cores[i]), evt->cmd);
	}
    } else {
	return core_handler(&vm->cores[evt->core_id], evt->cmd);
    }

    
    return 0;
}


int v3_init_vm_debugging(struct v3_vm_info * vm) {
    v3_hook_host_event(vm, HOST_DEBUG_EVT, 
		       V3_HOST_EVENT_HANDLER(evt_handler), 
		       NULL);


    return 0;
}





void v3_print_segments(struct v3_segments * segs) {
    int i = 0;
    struct v3_segment * seg_ptr;

    seg_ptr=(struct v3_segment *)segs;
  
    char *seg_names[] = {"CS", "DS" , "ES", "FS", "GS", "SS" , "LDTR", "GDTR", "IDTR", "TR", NULL};
    V3_Print(VM_NONE, VCORE_NONE, "Segments\n");

    for (i = 0; seg_names[i] != NULL; i++) {

	V3_Print(VM_NONE, VCORE_NONE, "\t%s: Sel=%x, base=%p, limit=%x (long_mode=%d, db=%d)\n", seg_names[i], seg_ptr[i].selector, 
		   (void *)(addr_t)seg_ptr[i].base, seg_ptr[i].limit,
		   seg_ptr[i].long_mode, seg_ptr[i].db);

    }
}



void v3_print_ctrl_regs(struct guest_info * core) {
    struct v3_ctrl_regs * regs = &(core->ctrl_regs);
    int i = 0;
    v3_reg_t * reg_ptr;
    char * reg_names[] = {"CR0", "CR2", "CR3", "CR4", "CR8", "FLAGS", "EFER", NULL};
   

    reg_ptr = (v3_reg_t *)regs;

    V3_Print(core->vm_info, core,"Ctrl Regs:\n");

    for (i = 0; reg_names[i] != NULL; i++) {
        V3_Print(core->vm_info, core, "\t%s=0x%p (at %p)\n", reg_names[i], (void *)(addr_t)reg_ptr[i], &(reg_ptr[i]));  
    }


}

#if 0
static int safe_gva_to_hva(struct guest_info * core, addr_t linear_addr, addr_t * host_addr) {
    /* select the proper translation based on guest mode */
    if (core->mem_mode == PHYSICAL_MEM) {
    	if (v3_gpa_to_hva(core, linear_addr, host_addr) == -1) return -1;
    } else if (core->mem_mode == VIRTUAL_MEM) {
	if (v3_gva_to_hva(core, linear_addr, host_addr) == -1) return -1;
    }
    return 0;
}

static int v3_print_disassembly(struct guest_info * core) {
    int passed_rip = 0;
    addr_t rip, rip_linear, rip_host;

    /* we don't know where the instructions preceding RIP start, so we just take
     * a guess and hope the instruction stream synced up with our disassembly
     * some time before RIP; if it has not we correct RIP at that point
     */

    /* start disassembly 64 bytes before current RIP, continue 32 bytes after */
    rip = (addr_t) core->rip - 64;
    while ((int) (rip - core->rip) < 32) {
	V3_Print(info->vm_info, info, "disassembly step\n");

    	/* always print RIP, even if the instructions before were bad */
    	if (!passed_rip && rip >= core->rip) {
    	    if (rip != core->rip) {
    	    	V3_Print(info->vm_info, info, "***** bad disassembly up to this point *****\n");
    	    	rip = core->rip;
    	    }
    	    passed_rip = 1;
    	}

    	/* look up host virtual address for this instruction */
    	rip_linear = get_addr_linear(core, rip, &(core->segments.cs));
    	if (safe_gva_to_hva(core, rip_linear, &rip_host) < 0) {
    	    rip++;
    	    continue;
    	}

    	/* print disassembled instrcution (updates rip) */
    	if (v3_disasm(core, (void *) rip_host, &rip, rip == core->rip) < 0) {
    	    rip++;
    	    continue;
    	}

    }

    return 0;
}

#endif

void v3_print_guest_state(struct guest_info * core) {
    addr_t linear_addr = 0; 

    V3_Print(core->vm_info, core, "RIP: %p\n", (void *)(addr_t)(core->rip));
    linear_addr = get_addr_linear(core, core->rip, &(core->segments.cs));
    V3_Print(core->vm_info, core, "RIP Linear: %p\n", (void *)linear_addr);

    V3_Print(core->vm_info, core, "NumExits: %u\n", (uint32_t)core->num_exits);

    V3_Print(core->vm_info, core, "IRQ STATE: started=%d, pending=%d\n", 
	     core->intr_core_state.irq_started, 
	     core->intr_core_state.irq_pending);
    V3_Print(core->vm_info, core, "EXCP STATE: err_code_valid=%d, err_code=%x\n", 
	     core->excp_state.excp_error_code_valid, 
	     core->excp_state.excp_error_code);


    v3_print_segments(&(core->segments));
    v3_print_ctrl_regs(core);

    if (core->shdw_pg_mode == SHADOW_PAGING) {
	V3_Print(core->vm_info, core, "Shadow Paging Guest Registers:\n");
	V3_Print(core->vm_info, core, "\tGuest CR0=%p\n", (void *)(addr_t)(core->shdw_pg_state.guest_cr0));
	V3_Print(core->vm_info, core, "\tGuest CR3=%p\n", (void *)(addr_t)(core->shdw_pg_state.guest_cr3));
	V3_Print(core->vm_info, core, "\tGuest EFER=%p\n", (void *)(addr_t)(core->shdw_pg_state.guest_efer.value));
	// CR4
    }
    v3_print_GPRs(core);

    v3_print_mem_map(core->vm_info);

    v3_print_stack(core);

    //  v3_print_disassembly(core);
}


void v3_print_arch_state(struct guest_info * core) {


}


void v3_print_guest_state_all(struct v3_vm_info * vm) {
    int i = 0;

    V3_Print(vm, VCORE_NONE,"VM Core states for %s\n", vm->name);

    for (i = 0; i < 80; i++) {
      V3_Print(vm, VCORE_NONE, "-");
    }

    for (i = 0; i < vm->num_cores; i++) {
	v3_print_guest_state(&vm->cores[i]);  
    }
    
    for (i = 0; i < 80; i++) {
	V3_Print(vm, VCORE_NONE, "-");
    }

    V3_Print(vm, VCORE_NONE, "\n");    
}



void v3_print_stack(struct guest_info * core) {
    addr_t linear_addr = 0;
    addr_t host_addr = 0;
    int i = 0;
    v3_cpu_mode_t cpu_mode = v3_get_vm_cpu_mode(core);

    linear_addr = get_addr_linear(core, core->vm_regs.rsp, &(core->segments.ss));
 
    V3_Print(core->vm_info, core, "Stack at %p:\n", (void *)linear_addr);
   
    if (core->mem_mode == PHYSICAL_MEM) {
	if (v3_gpa_to_hva(core, linear_addr, &host_addr) == -1) {
	    PrintError(core->vm_info, core, "Could not translate Stack address\n");
	    return;
	}
    } else if (core->mem_mode == VIRTUAL_MEM) {
	if (v3_gva_to_hva(core, linear_addr, &host_addr) == -1) {
	    PrintError(core->vm_info, core, "Could not translate Virtual Stack address\n");
	    return;
	}
    }
    
    V3_Print(core->vm_info, core, "Host Address of rsp = 0x%p\n", (void *)host_addr);
 
    // We start i at one because the current stack pointer points to an unused stack element
    for (i = 0; i <= 24; i++) {

	if (cpu_mode == REAL) {
	    V3_Print(core->vm_info, core, "\t0x%.4x\n", *((uint16_t *)host_addr + (i * 2)));
	} else if (cpu_mode == LONG) {
	    V3_Print(core->vm_info, core, "\t%p\n", (void *)*(addr_t *)(host_addr + (i * 8)));
	} else {
	    // 32 bit stacks...
	    V3_Print(core->vm_info, core, "\t0x%.8x\n", *(uint32_t *)(host_addr + (i * 4)));
	}
    }

}    


void v3_print_backtrace(struct guest_info * core) {
    addr_t gla_rbp = 0;
    int i = 0;
    v3_cpu_mode_t cpu_mode = v3_get_vm_cpu_mode(core);
    struct v3_cfg_file * system_map = v3_cfg_get_file(core->vm_info, "System.map");

    V3_Print(core->vm_info, core, "Performing Backtrace for Core %d\n", core->vcpu_id);
    V3_Print(core->vm_info, core, "\tRSP=%p, RBP=%p\n", (void *)core->vm_regs.rsp, (void *)core->vm_regs.rbp);

    gla_rbp = get_addr_linear(core, core->vm_regs.rbp, &(core->segments.ss));


    for (i = 0; i < 30; i++) {
	addr_t hva_rbp = 0; 
	addr_t hva_rip = 0; 
	char * sym_name = NULL;
	addr_t rip_val = 0;

	if (core->mem_mode == PHYSICAL_MEM) {
	    if (v3_gpa_to_hva(core, gla_rbp, &hva_rbp) == -1) {
		PrintError(core->vm_info, core, "Could not translate Stack address\n");
		return;
	    }
	} else if (core->mem_mode == VIRTUAL_MEM) {
	    if (v3_gva_to_hva(core, gla_rbp, &hva_rbp) == -1) {
		PrintError(core->vm_info, core, "Could not translate Virtual Stack address\n");
		return;
	    }
	}


	hva_rip = hva_rbp + v3_get_addr_width(core);
	
	if (cpu_mode == REAL) {
	    rip_val = (addr_t)*(uint16_t *)hva_rip;
	} else if (cpu_mode == LONG) {
	    rip_val = (addr_t)*(uint64_t *)hva_rip;
	} else {
	    rip_val = (addr_t)*(uint32_t *)hva_rip;
	}

	if (system_map) {
	    char * tmp_ptr = system_map->data;
	    char * sym_ptr = NULL;
	    uint64_t file_offset = 0; 
	    uint64_t sym_offset = 0;

	    while (file_offset < system_map->size) {
		sym_offset = strtox(tmp_ptr, &tmp_ptr);

		tmp_ptr += 3; // pass over symbol type

		if (sym_offset > rip_val) {
		    char * end_ptr = strchr(sym_ptr, '\n');

		    if (end_ptr) {
			*end_ptr = 0; // null terminate symbol...
		    }

		    sym_name = sym_ptr;
		    break;
		}

		sym_ptr = tmp_ptr;
		{ 
		    char * end_ptr2 = strchr(tmp_ptr, '\n');

		    if (!end_ptr2) {
			tmp_ptr += strlen(tmp_ptr) + 1;
		    } else {
			tmp_ptr = end_ptr2 + 1;
		    }
		}
	    }
	}

	if (!sym_name) {
	    sym_name = "?";
	}

	if (cpu_mode == REAL) {
	    V3_Print(core->vm_info, core, "Next RBP=0x%.4x, RIP=0x%.4x (%s)\n", 
		     *(uint16_t *)hva_rbp,*(uint16_t *)hva_rip, 
		     sym_name);
	    
	    gla_rbp = *(uint16_t *)hva_rbp;
	} else if (cpu_mode == LONG) {
	    V3_Print(core->vm_info, core, "Next RBP=%p, RIP=%p (%s)\n", 
		     (void *)*(uint64_t *)hva_rbp, (void *)*(uint64_t *)hva_rip,
		     sym_name);
	    gla_rbp = *(uint64_t *)hva_rbp;
	} else {
	    V3_Print(core->vm_info, core, "Next RBP=0x%.8x, RIP=0x%.8x (%s)\n", 
		     *(uint32_t *)hva_rbp, *(uint32_t *)hva_rip,
		     sym_name);
	    gla_rbp = *(uint32_t *)hva_rbp;
	}

    }
}


#ifdef __V3_32BIT__

void v3_print_GPRs(struct guest_info * core) {
    struct v3_gprs * regs = &(core->vm_regs);
    int i = 0;
    v3_reg_t * reg_ptr;
    char * reg_names[] = { "RDI", "RSI", "RBP", "RSP", "RBX", "RDX", "RCX", "RAX", NULL};

    reg_ptr = (v3_reg_t *)regs;

    V3_Print(info->vm_info, info, "32 bit GPRs:\n");

    for (i = 0; reg_names[i] != NULL; i++) {
	V3_Print(info->vm_info, info, "\t%s=0x%p (at %p)\n", reg_names[i], (void *)(addr_t)reg_ptr[i], &(reg_ptr[i]));  
    }
}

void v3_print_idt(struct guest_info * core, addr_t idtr_base) {
    addr_t base_hva;

    if (core->mem_mode == PHYSICAL_MEM) {
        v3_gpa_to_hva(core, 
                      get_addr_linear(core, idtr_base, &(core->segments.cs)),
                      &base_hva);
        PrintError(core->vm_info, core, "Kind of weird that we got here.... physical mem?\n");
    } else if (core->mem_mode == VIRTUAL_MEM) {
        v3_gva_to_hva(core, 
                      get_addr_linear(core, idtr_base, &(core->segments.cs)),
                      &base_hva);
    }

    // SANITY CHECK
    if (idtr_base != get_addr_linear(core, idtr_base, &(core->segments.cs))) {
        PrintError(core->vm_info, core, "idtr base address != linear translation, might be something funky with cs\n");
    }

    int i;
    char *types[16] = {"  ILGL","aTSS16","   LDT","bTSS16","call16","  task","intr16","trap16",
        "  ILGL","aTSS32","  ILGL","bTSS32","call32","  ILGL","intr32","trap32"};

    struct int_trap_gate_lgcy * entry;
    entry = (struct int_trap_gate_lgcy *)base_hva;
    PrintDebug(core->vm_info, core, "= IDT ========\n");
    PrintDebug(core->vm_info, core, "  # | hex | selector | si:ti:rpl |   offset | type | dpl | s | p\n");
    for (i = 0; i < NUM_IDT_ENTRIES; i++) {
        uint32_t tmp = entry->selector;
        struct segment_selector * seg = (struct segment_selector *)(&tmp);
        PrintDebug(core->vm_info, core, "%3d | %3x |     %04x |   %03x:%x:%x | %04x%04x | %s |   %x | %x | %x | %x\n", i, i,
                entry->selector,
                seg->index, seg->ti, seg->rpl,
                entry->offset_hi, entry->offset_lo,
                types[entry->type], entry->dpl, entry->s, entry->p);
        entry++;
    }
}

void v3_print_gdt(struct guest_info * core, addr_t gdtr_base) {
    addr_t base_hva;

    if (core->mem_mode == PHYSICAL_MEM) {
        v3_gpa_to_hva(core, 
                      get_addr_linear(core, gdtr_base, &(core->segments.cs)),
                      &base_hva);
        PrintError(core->vm_info, core, "Kind of weird that we got here.... physical mem?\n");
    } else if (core->mem_mode == VIRTUAL_MEM) {
        v3_gva_to_hva(core, 
                      get_addr_linear(core, gdtr_base, &(core->segments.cs)),
                      &base_hva);
    }

    // SANITY CHECK
    if (gdtr_base != get_addr_linear(core, gdtr_base, &(core->segments.cs))) {
        PrintError(core->vm_info, core, "gdtr base address != linear translation, might be something funky with cs\n");
    }

    int i;
    char* cd[2] = {"code","data"};
    // TODO: handle possibility of gate/segment descriptor

    struct code_desc_lgcy * entry;
    entry = (struct code_desc_long *)base_hva;
    PrintDebug(core->vm_info, core, "= GDT ========\n");
    PrintDebug(core->vm_info, core, "  # | hex | limit |     base |  c/d | dpl | p\n");
    for (i = 0; i < NUM_GDT_ENTRIES; i++) {
        PrintDebug(core->vm_info, core, "%3d | %3x | %x%04x | %02x%02x%04x | %s |   %x | %x\n", i, i,
                entry->limit_hi, entry->limit_lo,
                entry->base_hi, entry->base_mid, entry->base_lo,
                cd[entry->one1], entry->dpl, entry->p);
        entry++;
    }
}

void v3_print_gp_error(struct guest_info * core, addr_t exit_info1) {
    struct selector_error_code * error = (struct selector_error_code *)(&exit_info1);

    PrintDebug(core->vm_info, core, "      selector index: %x, TI: %x, IDT: %x, EXT: %x (error=%llx)\n",
            error->index, error->ti, error->idt, error->ext,
            (unsigned long long)exit_info1);
}

#elif __V3_64BIT__

void v3_print_GPRs(struct guest_info * core) {
    struct v3_gprs * regs = &(core->vm_regs);
    int i = 0;
    v3_reg_t * reg_ptr;
    char * reg_names[] = { "RDI", "RSI", "RBP", "RSP", "RBX", "RDX", "RCX", "RAX", \
			   "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15", NULL};

    reg_ptr = (v3_reg_t *)regs;

    V3_Print(core->vm_info, core, "64 bit GPRs:\n");

    for (i = 0; reg_names[i] != NULL; i++) {
	V3_Print(core->vm_info, core, "\t%s=0x%p (at %p)\n", reg_names[i], (void *)(addr_t)reg_ptr[i], &(reg_ptr[i]));  
    }
}

void v3_print_idt(struct guest_info * core, addr_t idtr_base) {
    addr_t base_hva;

    if (core->mem_mode == PHYSICAL_MEM) {
        v3_gpa_to_hva(core, 
                      get_addr_linear(core, idtr_base, &(core->segments.cs)),
                      &base_hva);
    } else if (core->mem_mode == VIRTUAL_MEM) {
        v3_gva_to_hva(core, 
                      get_addr_linear(core, idtr_base, &(core->segments.cs)),
                      &base_hva);
    }

    // SANITY CHECK
    if (idtr_base != get_addr_linear(core, idtr_base, &(core->segments.cs))) {
        PrintError(core->vm_info, core, "idtr base address != linear translation, might be something funky with cs\n");
    }

    int i;
    char *types[16] = {"ILGL","ILGL"," LDT","ILGL","ILGL","ILGL","ILGL","ILGL","ILGL",
        "aTSS","ILGL","bTSS","call","ILGL","intr","trap"};

    struct int_trap_gate_long * entry;
    entry = (struct int_trap_gate_long *)base_hva;
    PrintDebug(core->vm_info, core, "= IDT ========\n");
    PrintDebug(core->vm_info, core, "  # | hex | selector | si:ti:rpl |           offset | type | dpl | s | r | p\n");
    for (i = 0; i < NUM_IDT_ENTRIES; i++) {
        uint32_t tmp = entry->selector;
        struct segment_selector * seg = (struct segment_selector *)(&tmp);
        PrintDebug(core->vm_info, core, "%3d | %3x |     %04x |   %03x:%x:%x | %08x%04x%04x | %s |   %x | %x | %x | %x\n", i, i,
                entry->selector,
                seg->index, seg->ti, seg->rpl,
                entry->offset_hi, entry->offset_mid, entry->offset_lo,
                types[entry->type], entry->dpl, entry->s,
                entry->s, entry->p);
        entry++;
    }
}

void v3_print_gdt(struct guest_info * core, addr_t gdtr_base) {
    addr_t base_hva;

    if (core->mem_mode == PHYSICAL_MEM) {
        v3_gpa_to_hva(core, 
                      get_addr_linear(core, gdtr_base, &(core->segments.cs)),
                      &base_hva);
    } else if (core->mem_mode == VIRTUAL_MEM) {
        v3_gva_to_hva(core, 
                      get_addr_linear(core, gdtr_base, &(core->segments.cs)),
                      &base_hva);
    }

    // SANITY CHECK
    if (gdtr_base != get_addr_linear(core, gdtr_base, &(core->segments.cs))) {
        PrintError(core->vm_info, core, "gdtr base address != linear translation, might be something funky with cs\n");
    }

    int i;
    char* cd[2] = {"code","data"};
    // TODO: handle possibility of gate/segment descriptor

    struct code_desc_long * entry;
    entry = (struct code_desc_long *)base_hva;
    PrintDebug(core->vm_info, core, "= GDT ========\n");
    PrintDebug(core->vm_info, core, "  # | hex | limit |     base |  c/d | dpl | p\n");
    for (i = 0; i < NUM_GDT_ENTRIES; i++) {
        PrintDebug(core->vm_info, core, "%3d | %3x | %x%04x | %02x%02x%04x | %s |   %x | %x\n", i, i,
                entry->limit_hi, entry->limit_lo,
                entry->base_hi, entry->base_mid, entry->base_lo,
                cd[entry->one1], entry->dpl, entry->p);
        entry++;
    }
}

void v3_print_gp_error(struct guest_info * core, addr_t exit_info1) {
    struct selector_error_code * error = (struct selector_error_code *)(&exit_info1);

    PrintDebug(core->vm_info, core, "      selector index: %x, TI: %x, IDT: %x, EXT: %x (error=%llx)\n",
            error->index, error->ti, error->idt, error->ext,
            (unsigned long long)exit_info1);
}

#endif
