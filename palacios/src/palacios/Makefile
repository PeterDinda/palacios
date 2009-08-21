obj-y := \
	vm_guest.o \
	vm_guest_mem.o \
	vmm.o \
	vmm_config.o \
	vmm_ctrl_regs.o \
	vmm_debug.o \
	vmm_decoder.o \
	vmm_dev_mgr.o \
	vmm_direct_paging.o \
	vmm_emulator.o \
	vmm_excp.o \
	vmm_hashtable.o \
	vmm_host_events.o \
	vmm_hypercall.o \
	vmm_intr.o \
	vmm_io.o \
	vmm_lock.o \
	vmm_mem.o \
	vmm_msr.o \
	vmm_paging.o \
	vmm_queue.o \
	vmm_rbtree.o \
	vmm_ringbuffer.o \
	vmm_shadow_paging.o \
	vmm_sprintf.o \
	vmm_string.o \
	vmm_time.o \
	vmm_util.o \
	vmm_xed.o \
	vmm_binaries.o 


obj-$(CONFIG_SVM) +=    svm.o \
			svm_halt.o \
			svm_io.o \
			svm_lowlevel.o \
			svm_msr.o \
			svm_pause.o \
			svm_wbinvd.o \
			svm_handler.o \
			vmcb.o

obj-$(CONFIG_VMX) +=  	vmx.o \
			vmx_handler.o \
			vmx_io.o \
			vmx_lowlevel.o \
			vmx_msr.o \
			vmcs.o



obj-$(CONFIG_INSTRUMENT_VMM) += vmm_instrument.o
obj-$(CONFIG_TELEMETRY) += vmm_telemetry.o 
obj-$(CONFIG_SOCKET) +=  vmm_socket.o

obj-$(CONFIG_SYMBIOTIC_SWAP) += vmm_sym_swap.o