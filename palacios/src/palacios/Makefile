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
	vmm_halt.o \
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
	vmm_binaries.o \
	vmm_cpuid.o \
	vmm_xml.o \
	vmm_muxer.o \
	vmm_mem_hook.o \
	vmm_mptable.o \

obj-$(CONFIG_SVM) +=    svm.o \
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
			vmcs.o \
			vmx_ctrl_regs.o \
			vmx_assist.o




obj-$(CONFIG_INSTRUMENT_VMM) += vmm_instrument.o
obj-$(CONFIG_TELEMETRY) += vmm_telemetry.o 
obj-$(CONFIG_SOCKET) +=  vmm_socket.o
obj-$(CONFIG_VNET) += vmm_vnet.o
obj-$(CONFIG_FILE) += vmm_file.o
obj-$(CONFIG_CONSOLE) += vmm_console.o


obj-$(CONFIG_SYMBIOTIC) += vmm_symbiotic.o vmm_symspy.o
obj-$(CONFIG_SYMCALL) += vmm_symcall.o
obj-$(CONFIG_SYMMOD) += vmm_symmod.o


obj-y += mmu/
