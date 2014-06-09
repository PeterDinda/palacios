#ifndef V3_CONFIG_SVM


static int handle_svm_nested_pagefault(struct guest_info * info, addr_t fault_addr, void *pfinfo,
				       addr_t *actual_start, addr_t *actual_end) 
{
    PrintError(info->vm_info, info, "Cannot do nested page fault as SVM is not enabled.\n");
    return -1;
}
static int handle_svm_invalidate_nested_addr(struct guest_info * info, addr_t inv_addr,
					     addr_t *actual_start, addr_t *actual_end) 
{
    PrintError(info->vm_info, info, "Cannot do invalidate nested addr as SVM is not enabled.\n");
    return -1;
}
static int handle_svm_invalidate_nested_addr_range(struct guest_info * info, 
						   addr_t inv_addr_start, addr_t inv_addr_end,
						   addr_t *actual_start, addr_t *actual_end) 
{
    PrintError(info->vm_info, info, "Cannot do invalidate nested addr range as SVM is not enabled.\n");
    return -1;
}

#else

static int handle_svm_nested_pagefault(struct guest_info * info, addr_t fault_addr, void *pfinfo,
				       addr_t *actual_start, addr_t *actual_end) 
{
    pf_error_t error_code = *((pf_error_t *) pfinfo);
    v3_cpu_mode_t mode = v3_get_host_cpu_mode();


    PrintDebug(info->vm_info, info, "Nested PageFault: fault_addr=%p, error_code=%u\n", (void *)fault_addr, *(uint_t *)&error_code);

    switch(mode) {
	case REAL:
	case PROTECTED:
	  return handle_passthrough_pagefault_32(info, fault_addr, error_code, actual_start, actual_end);
	    
	case PROTECTED_PAE:
	  return handle_passthrough_pagefault_32pae(info, fault_addr, error_code, actual_start, actual_end);
	    
	case LONG:
	case LONG_32_COMPAT:
	  return handle_passthrough_pagefault_64(info, fault_addr, error_code, actual_start, actual_end);   
	    
	default:
	    PrintError(info->vm_info, info, "Unknown CPU Mode\n");
	    break;
    }
    return -1;
}


static int handle_svm_invalidate_nested_addr(struct guest_info * info, addr_t inv_addr,
					     addr_t *actual_start, addr_t *actual_end) {

#ifdef __V3_64BIT__
    v3_cpu_mode_t mode = LONG;
#else 
#error Compilation for 32 bit target detected
    v3_cpu_mode_t mode = PROTECTED;
#endif

    switch(mode) {
	case REAL:
	case PROTECTED:
	  return invalidate_addr_32(info, inv_addr, actual_start, actual_end);

	case PROTECTED_PAE:
	  return invalidate_addr_32pae(info, inv_addr, actual_start, actual_end);

	case LONG:
	case LONG_32_COMPAT:
	  return invalidate_addr_64(info, inv_addr, actual_start, actual_end);	    
	
	default:
	    PrintError(info->vm_info, info, "Unknown CPU Mode\n");
	    break;
    }

    return -1;
}

static int handle_svm_invalidate_nested_addr_range(struct guest_info * info, 
						   addr_t inv_addr_start, addr_t inv_addr_end,
						   addr_t *actual_start, addr_t *actual_end) 
{
    
#ifdef __V3_64BIT__
    v3_cpu_mode_t mode = LONG;
#else 
#error Compilation for 32 bit target detected
    v3_cpu_mode_t mode = PROTECTED;
#endif

    switch(mode) {
	case REAL:
	case PROTECTED:
	  return invalidate_addr_32_range(info, inv_addr_start, inv_addr_end, actual_start, actual_end);

	case PROTECTED_PAE:
	  return invalidate_addr_32pae_range(info, inv_addr_start, inv_addr_end, actual_start, actual_end);

	case LONG:
	case LONG_32_COMPAT:
	  return invalidate_addr_64_range(info, inv_addr_start, inv_addr_end, actual_start, actual_end);  
	
	default:
	    PrintError(info->vm_info, info, "Unknown CPU Mode\n");
	    break;
    }

    return -1;
}

#endif
