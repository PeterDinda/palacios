obj-y := \
	vm_guest.o \
	vm_guest_mem.o \
	vmm.o \
	vmm_config.o \
	vmm_cpu_mapper.o \
	vmm_ctrl_regs.o \
	vmm_debug.o \
	vmm_decoder.o \
	vmm_dev_mgr.o \
	vmm_direct_paging.o \
	vmm_emulator.o \
	vmm_excp.o \
	vmm_halt.o \
	vmm_mwait.o \
	vmm_hashtable.o \
	vmm_host_events.o \
	vmm_hypercall.o \
	vmm_intr.o \
	vmm_io.o \
	vmm_lock.o \
	vmm_mem.o \
        vmm_fp.o \
	vmm_msr.o \
	vmm_paging.o \
	vmm_options.o \
	vmm_queue.o \
	vmm_rbtree.o \
	vmm_ringbuffer.o \
	vmm_scheduler.o \
	vmm_shadow_paging.o \
	vmm_sprintf.o \
	vmm_string.o \
	vmm_time.o \
	vmm_util.o \
	vmm_binaries.o \
	vmm_cpuid.o \
	vmm_xml.o \
	vmm_mem_hook.o \
	vmm_extensions.o \
	vmm_multitree.o \
	vmm_bitmap.o \
	vmm_barrier.o \
	vmm_timeout.o \
	vmm_exits.o \
	vmm_events.o \
	vmm_perftune.o \
	vmm_fw_cfg.o 


obj-$(V3_CONFIG_SWAPPING) += vmm_swapping.o

obj-$(V3_CONFIG_XED) +=	vmm_xed.o
obj-$(V3_CONFIG_V3_DECODER) += vmm_v3dec.o
obj-$(V3_CONFIG_QUIX86) += vmm_quix86.o
obj-$(V3_CONFIG_QUIX86_DEBUG) += vmm_quix86.o


obj-$(V3_CONFIG_SVM) +=    svm.o \
			   svm_io.o \
			   svm_lowlevel.o \
			   svm_msr.o \
			   svm_pause.o \
			   svm_wbinvd.o \
			   svm_handler.o \
			   svm_exits.o \
			   vmcb.o

obj-$(V3_CONFIG_VMX) +=  	vmx.o \
				vmx_handler.o \
				vmx_io.o \
				vmx_lowlevel.o \
				vmx_msr.o \
				vmx_hw_info.o \
				vmcs.o \
				vmx_ctrl_regs.o \
				vmx_assist.o \
				vmx_exits.o



obj-$(V3_CONFIG_CHECKPOINT) += vmm_checkpoint.o

obj-$(V3_CONFIG_TELEMETRY) += vmm_telemetry.o 

obj-$(V3_CONFIG_PMU_TELEMETRY) += vmm_pmu_telemetry.o 
obj-$(V3_CONFIG_PWRSTAT_TELEMETRY) += vmm_pwrstat_telemetry.o

obj-$(V3_CONFIG_SYMBIOTIC) += vmm_symbiotic.o vmm_symspy.o
obj-$(V3_CONFIG_SYMCALL) += vmm_symcall.o
obj-$(V3_CONFIG_SYMMOD) += vmm_symmod.o


obj-$(V3_CONFIG_MEM_TRACK) += vmm_mem_track.o

obj-$(V3_CONFIG_MULTIBOOT) += vmm_multiboot.o
obj-$(V3_CONFIG_HVM) += vmm_hvm.o vmm_hvm_lowlevel.o

obj-y += mmu/
