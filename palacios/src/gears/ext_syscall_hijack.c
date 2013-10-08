/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Kyle C. Hale <kh@u.norhtwestern.edu>
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmcb.h>
#include <palacios/vmm_hypercall.h>


#include <gears/syscall_hijack.h>
#include <gears/sw_intr.h>
#include <gears/syscall_ref.h>

#ifdef V3_CONFIG_EXT_CODE_INJECT
#include <gears/code_inject.h>
#include <palacios/vmm_list.h>
extern struct v3_code_injects code_injects;
#endif

#ifndef V3_CONFIG_DEBUG_EXT_SYSCALL_HIJACK
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


struct v3_syscall_hook {
    int (*handler)(struct guest_info * core, uint_t syscall_nr, void * priv_data);
    void * priv_data;
};

static struct v3_syscall_hook * syscall_hooks[512];

#if defined(V3_CONFIG_EXT_SELECTIVE_SYSCALL_EXIT) || defined(V3_CONFIG_EXT_SYSCALL_INSTR)
static struct v3_syscall_info syscall_info;
#endif

static void print_arg (struct  guest_info * core, v3_reg_t reg, uint8_t argnum) {

    addr_t hva;
    int ret = 0;
    
    PrintDebug(core->vm_info, core, "\t ARG%d: INT - %ld\n", argnum, (long) reg);

    if (core->mem_mode == PHYSICAL_MEM) {
        ret = v3_gpa_to_hva(core, get_addr_linear(core, reg, &(core->segments.ds)), &hva);
    }
    else { 
        ret = v3_gva_to_hva(core, get_addr_linear(core, reg, &(core->segments.ds)), &hva);
    }

    PrintDebug(core->vm_info, core, "\t       STR - ");
    if (ret == -1) {
        PrintDebug(core->vm_info, core, "\n");
        return;
    }
        
    uint32_t c = max(MAX_CHARS, 4096 - (hva % 4096));
    int i = 0;
    for (; i < c && *((char*)(hva + i)) != 0; i++) {
        PrintDebug(core->vm_info, core, "%c", *((char*)(hva + i)));
    }
    PrintDebug(core->vm_info, core, "\n");
}


static void print_syscall (uint8_t is64, struct guest_info * core) {

    if (is64) {
        PrintDebug(core->vm_info, core, "Syscall #%ld: \"%s\"\n", (long)core->vm_regs.rax, get_linux_syscall_name64(core->vm_regs.rax));
    } else {
        PrintDebug(core->vm_info, core, "Syscall #%ld: \"%s\"\n", (long)core->vm_regs.rax, get_linux_syscall_name32(core->vm_regs.rax));
    }

    print_arg(core, core->vm_regs.rbx, 1);
    print_arg(core, core->vm_regs.rcx, 2);
    print_arg(core, core->vm_regs.rdx, 3);
}


int v3_syscall_handler (struct guest_info * core, uint8_t vector, void * priv_data) {
 
    uint_t syscall_nr = (uint_t) core->vm_regs.rax;
    int err = 0, ret = 0;

    struct v3_syscall_hook * hook = syscall_hooks[syscall_nr];

#ifdef V3_CONFIG_EXT_SYSCALL_INSTR
    // address originally written to LSTAR
    if (!vector) 
        core->rip = syscall_info.target_addr;
#endif

    if (hook == NULL) {
#ifdef V3_CONFIG_EXT_SYSCALL_PASSTHROUGH
        if (v3_hook_passthrough_syscall(core, syscall_nr) == -1) {
            PrintDebug(core->vm_info, core, "Error hooking passthrough syscall\n");
            return -1;
        }
        hook = syscall_hooks[syscall_nr];
#endif

/* 
 * if this syscall isn't hooked, pop off a pending inject
 * and run it
 */
#ifdef V3_CONFIG_EXT_CODE_INJECT
        struct v3_code_injects * injects = &code_injects;
        struct v3_code_inject_info * inject = NULL;

        if (list_empty(&(injects->code_inject_list))) {
            return 0;
        } else {

            inject = (struct v3_code_inject_info*) list_first_entry(
                        &(injects->code_inject_list), 
                        struct v3_code_inject_info, 
                        inject_node);

            if (inject == NULL) {
                PrintError(core->vm_info, core, "Problem getting inject from inject list\n");
                return -1;
            }

            if (inject->in_progress) 
                return 0;

            // do the inject and don't fall over if there's an inject already in
            // progress
            if ((ret = v3_handle_guest_inject(core, (void*)inject)) == -1) {
                PrintError(core->vm_info, core, "Could not run code injection: v3_syscall_handler\n");
                return 0;
            } else {
                return ret; 
            }
        }
#else
        return 0;
#endif
    }
    
    err = hook->handler(core, syscall_nr, hook->priv_data);
    if (err == -1) {
        PrintDebug(core->vm_info, core, "V3 Syscall Handler: Error in syscall hook\n");
        return -1;
    }

#ifdef V3_CONFIG_EXT_CODE_INJECT
    if (err == E_NEED_PF) 
        return E_NEED_PF;
#endif
    return 0;
}


#ifdef V3_CONFIG_EXT_SELECTIVE_SYSCALL_EXIT
static int v3_handle_lstar_write (struct guest_info * core, uint_t msr, struct v3_msr src, void * priv_data) {
    syscall_info.target_addr = (uint64_t) ((((uint64_t)src.hi) << 32) | src.lo);
    
    PrintDebug(core->vm_info, core, "LSTAR Write: %p\n", (void*)syscall_info.target_addr); 
    core->msrs.lstar = syscall_info.target_addr;
    return 0;
}


// virtualize the lstar
static int v3_handle_lstar_read (struct guest_info * core, uint_t msr, struct v3_msr * dst, void * priv_data) {
    PrintDebug(core->vm_info, core, "LSTAR Read\n");
    dst->value = syscall_info.target_addr;
    return 0;
}


static int syscall_setup (struct guest_info * core, unsigned int hcall_id, void * priv_data) {
	addr_t syscall_stub, syscall_map_gva, syscall_map_hva, ssa_gva, ssa_hva;

	syscall_stub = (addr_t)core->vm_regs.rbx;
	syscall_map_gva = (addr_t)core->vm_regs.rcx;
	ssa_gva = (addr_t)core->vm_regs.rdx;

	PrintDebug(core->vm_info, core, "syscall setup hypercall:\n");
	PrintDebug(core->vm_info, core, "\t&syscall_stub (rbx): %p\n\t&syscall_map (rcx): %p\n", (void*)syscall_stub, (void*)syscall_map_gva);
	PrintDebug(core->vm_info, core, "\t&ssa (rdx): %p\n", (void*)ssa_gva);

	// the guest vitual address of the asm syscall handling routine
	syscall_info.syscall_stub = syscall_stub;

	
	// now get the hva of the system call map so we can manipulate it in the VMM
	if (v3_gva_to_hva(core, get_addr_linear(core, syscall_map_gva, &(core->segments.ds)), &syscall_map_hva) == 1) {
		PrintError(core->vm_info, core, "Problem translating gva to hva for syscall map\n");
		return -1;
	}
	
	if (v3_gva_to_hva(core, get_addr_linear(core, ssa_gva, &(core->segments.ds)), &ssa_hva) == 1) {
		PrintError(core->vm_info, core, "Problem translating gva to hva for syscall map\n");
		return -1;
	}
	
	PrintDebug(core->vm_info, core, "\t&syscall_map (hva): %p\n", (void*) syscall_map_hva);
	PrintDebug(core->vm_info, core, "\t&ssa (hva): %p\n", (void*) ssa_hva);

	syscall_info.syscall_map = (uint8_t*)syscall_map_hva;
	syscall_info.ssa = ssa_hva;

	/* return the original syscall entry point */
	core->vm_regs.rax = syscall_info.target_addr;

	/* redirect syscalls henceforth */
	core->msrs.lstar = syscall_stub;
	return 0;
}


static int syscall_cleanup (struct guest_info * core, unsigned int hcall_id, void * priv_data) {

    core->msrs.lstar = syscall_info.target_addr;
    PrintDebug(core->vm_info, core, "original syscall entry point restored\n");
    return 0;
}


static int sel_syscall_handle (struct guest_info * core, unsigned int hcall_id, void * priv_data) {
	struct v3_gprs regs;
	
	PrintDebug(core->vm_info, core, "caught a selectively exited syscall\n");
	
    /* setup registers for handler routines. They should be in the same state
     * as when the system call was originally invoked */
	memcpy((void*)&regs, (void*)&core->vm_regs, sizeof(struct v3_gprs));
	memcpy((void*)&core->vm_regs, (void*)syscall_info.ssa, sizeof(struct v3_gprs));
	
	//v3_print_guest_state(core);
	v3_syscall_handler(core, 0, NULL);
	
	memcpy((void*)syscall_info.ssa, (void*)&core->vm_regs, sizeof(struct v3_gprs));
	memcpy((void*)&core->vm_regs, (void*)&regs,sizeof(struct v3_gprs));
	return 0;
}


// TODO: make these three functions guest-dependent
int v3_syscall_on (void * ginfo, uint8_t syscall_nr) {
    PrintDebug(VM_NONE, VCORE_NONE, "Enabling exiting for syscall #%d\n", syscall_nr);
    syscall_info.syscall_map[syscall_nr] = 1;
    return 0;
}


int v3_syscall_off (void * ginfo, uint8_t syscall_nr) {
    PrintDebug(VM_NONE, VCORE_NONE, "Disabling exiting for syscall #%d\n", syscall_nr);
    syscall_info.syscall_map[syscall_nr] = 0;
    return 0;
}


int v3_syscall_stat (void * ginfo, uint8_t syscall_nr) {
    return syscall_info.syscall_map[syscall_nr];
}

#endif

static int init_syscall_hijack (struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data) {

#ifdef V3_CONFIG_EXT_SELECTIVE_SYSCALL_EXIT
	v3_register_hypercall(vm, SYSCALL_HANDLE_HCALL, sel_syscall_handle, NULL);
	v3_register_hypercall(vm, SYSCALL_SETUP_HCALL, syscall_setup, NULL);
	v3_register_hypercall(vm, SYSCALL_CLEANUP_HCALL, syscall_cleanup, NULL);
#endif

    return 0;
}



#ifdef V3_CONFIG_EXT_SYSCALL_INSTR
static int v3_handle_lstar_write (struct guest_info * core, uint_t msr, struct v3_msr src, void * priv_data) {
    PrintDebug(core->vm_info, core, "KCH: LSTAR Write\n");
    //PrintDebug(core->vm_info, core, "\tvalue: 0x%x%x\n", src.hi, src.lo);
    syscall_info.target_addr = (uint64_t) ((((uint64_t)src.hi) << 32) | src.lo);
    
    // Set LSTAR value seen by hardware while the guest is running
    PrintDebug(core->vm_info, core, "replacing with %lx\n", SYSCALL_MAGIC_ADDR);
    core->msrs.lstar = SYSCALL_MAGIC_ADDR;
    return 0;
}

static int v3_handle_lstar_read (struct guest_info * core, uint_t msr, struct v3_msr * dst, void * priv_data) {
    PrintDebug(core->vm_info, core, "KCH: LSTAR Read\n");
    dst->value = syscall_info.target_addr;
    return 0;
}
#endif


static int init_syscall_hijack_core (struct guest_info * core, void * priv_data, void ** core_data) {

#ifdef V3_CONFIG_EXT_SW_INTERRUPTS
    v3_hook_swintr(core, SYSCALL_INT_VECTOR, v3_syscall_handler, NULL);
#endif

#if defined(V3_CONFIG_EXT_SYSCALL_INSTR) || defined(V3_CONFIG_EXT_SELECTIVE_SYSCALL_EXIT)
    v3_hook_msr(core->vm_info, LSTAR_MSR,
        &v3_handle_lstar_read,
        &v3_handle_lstar_write,
        core);
#endif

    return 0;
}

static int deinit_syscall_hijack (struct v3_vm_info * vm, void * priv_data) {
    return 0;
}


static struct v3_extension_impl syscall_impl = {
    .name = "syscall_intercept",
    .vm_init = init_syscall_hijack,
    .vm_deinit = deinit_syscall_hijack,
    .core_init = init_syscall_hijack_core,
    .core_deinit = NULL,
    .on_entry = NULL,  
    .on_exit = NULL 
};

register_extension(&syscall_impl);


static inline struct v3_syscall_hook * get_syscall_hook (struct guest_info * core, uint_t syscall_nr) {
    return syscall_hooks[syscall_nr];
} 


int v3_hook_syscall (struct guest_info * core,
    uint_t syscall_nr,
    int (*handler)(struct guest_info * core, uint_t syscall_nr, void * priv_data),
    void * priv_data) 
{
    struct v3_syscall_hook * hook = (struct v3_syscall_hook *)V3_Malloc(sizeof(struct v3_syscall_hook));

    
    if (hook == NULL) {
	PrintError(core->vm_info, core, "Cannot allocate for syscall hook\n");
        return -1;
    }

    if (get_syscall_hook(core, syscall_nr) != NULL) {
        PrintError(core->vm_info, core, "System Call #%d already hooked\n", syscall_nr);
        return -1;
    }

    hook->handler = handler;
    hook->priv_data = priv_data;

    syscall_hooks[syscall_nr] = hook;

    PrintDebug(core->vm_info, core, "Hooked Syscall #%d\n", syscall_nr);

    return 0;
}


static int passthrough_syscall_handler (struct guest_info * core, uint_t syscall_nr, void * priv_data) {
    print_syscall(core->cpu_mode == LONG, core);
    return 0;
}


int v3_hook_passthrough_syscall (struct guest_info * core, uint_t syscall_nr) {
    
    int rc = v3_hook_syscall(core, syscall_nr, passthrough_syscall_handler, NULL);

    if (rc) {
        PrintError(core->vm_info, core, "failed to hook syscall 0x%x for passthrough (guest=0x%p)\n", syscall_nr, (void *)core);
        return -1;
    } else {
        PrintDebug(core->vm_info, core, "hooked syscall 0x%x for passthrough (guest=0x%p)\n", syscall_nr, (void *)core);
        return 0;
    }

    /* shouldn't get here */
    return 0;
}



char * get_linux_syscall_name32 (uint_t syscall_nr) { 

    switch (syscall_nr) { 

        case 0: return "restart_syscall"; break;
        case 1: return "exit"; break;
        case 2: return "fork"; break;
        case 3: return "read"; break;
        case 4: return "write"; break;
        case 5: return "open"; break;
        case 6: return "close"; break;
        case 7: return "waitpid"; break;
        case 8: return "creat"; break;
        case 9: return "link"; break;
        case 10: return "unlink"; break;
        case 11: return "execve"; break;
        case 12: return "chdir"; break;
        case 13: return "time"; break;
        case 14: return "mknod"; break;
        case 15: return "chmod"; break;
        case 16: return "lchown"; break;
        case 17: return "break"; break;
        case 18: return "oldstat"; break;
        case 19: return "lseek"; break;
        case 20: return "getpid"; break;
        case 21: return "mount"; break;
        case 22: return "umount"; break;
        case 23: return "setuid"; break;
        case 24: return "getuid"; break;
        case 25: return "stime"; break;
        case 26: return "ptrace"; break;
        case 27: return "alarm"; break;
        case 28: return "oldfstat"; break;
        case 29: return "pause"; break;
        case 30: return "utime"; break;
        case 31: return "stty"; break;
        case 32: return "gtty"; break;
        case 33: return "access"; break;
        case 34: return "nice"; break;
        case 35: return "ftime"; break;
        case 36: return "sync"; break;
        case 37: return "kill"; break;
        case 38: return "rename"; break;
        case 39: return "mkdir"; break;
        case 40: return "rmdir"; break;
        case 41: return "dup"; break;
        case 42: return "pipe"; break;
        case 43: return "times"; break;
        case 44: return "prof"; break;
        case 45: return "brk"; break;
        case 46: return "setgid"; break;
        case 47: return "getgid"; break;
        case 48: return "signal"; break;
        case 49: return "geteuid"; break;
        case 50: return "getegid"; break;
        case 51: return "acct"; break;
        case 52: return "umount2"; break;
        case 53: return "lock"; break;
        case 54: return "ioctl"; break;
        case 55: return "fcntl"; break;
        case 56: return "mpx"; break;
        case 57: return "setpgid"; break;
        case 58: return "ulimit"; break;
        case 59: return "oldolduname"; break;
        case 60: return "umask"; break;
        case 61: return "chroot"; break;
        case 62: return "ustat"; break;
        case 63: return "dup2"; break;
        case 64: return "getppid"; break;
        case 65: return "getpgrp"; break;
        case 66: return "setsid"; break;
        case 67: return "sigaction"; break;
        case 68: return "sgetmask"; break;
        case 69: return "ssetmask"; break;
        case 70: return "setreuid"; break;
        case 71: return "setregid"; break;
        case 72: return "sigsuspend"; break;
        case 73: return "sigpending"; break;
        case 74: return "sethostname"; break;
        case 75: return "setrlimit"; break;
        case 76: return "getrlimit"; break;
        case 77: return "getrusage"; break;
        case 78: return "gettimeofday"; break;
        case 79: return "settimeofday"; break;
        case 80: return "getgroups"; break;
        case 81: return "setgroups"; break;
        case 82: return "select"; break;
        case 83: return "symlink"; break;
        case 84: return "oldlstat"; break;
        case 85: return "readlink"; break;
        case 86: return "uselib"; break;
        case 87: return "swapon"; break;
        case 88: return "reboot"; break;
        case 89: return "readdir"; break;
        case 90: return "mmap"; break;
        case 91: return "munmap"; break;
        case 92: return "truncate"; break;
        case 93: return "ftruncate"; break;
        case 94: return "fchmod"; break;
        case 95: return "fchown"; break;
        case 96: return "getpriority"; break;
        case 97: return "setpriority"; break;
        case 98: return "profil"; break;
        case 99: return "statfs"; break;
        case 100: return "fstatfs"; break;
        case 101: return "ioperm"; break;
        case 102: return "socketcall"; break;
        case 103: return "syslog"; break;
        case 104: return "setitimer"; break;
        case 105: return "getitimer"; break;
        case 106: return "stat"; break;
        case 107: return "lstat"; break;
        case 108: return "fstat"; break;
        case 109: return "olduname"; break;
        case 110: return "iopl"; break;
        case 111: return "vhangup"; break;
        case 112: return "idle"; break;
        case 113: return "vm86old"; break;
        case 114: return "wait4"; break;
        case 115: return "swapoff"; break;
        case 116: return "sysinfo"; break;
        case 117: return "ipc"; break;
        case 118: return "fsync"; break;
        case 119: return "sigreturn"; break;
        case 120: return "clone"; break;
        case 121: return "setdomainname"; break;
        case 122: return "uname"; break;
        case 123: return "modify_ldt"; break;
        case 124: return "adjtimex"; break;
        case 125: return "mprotect"; break;
        case 126: return "sigprocmask"; break;
        case 127: return "create_module"; break;
        case 128: return "init_module"; break;
        case 129: return "delete_module"; break;
        case 130: return "get_kernel_syms"; break;
        case 131: return "quotactl"; break;
        case 132: return "getpgid"; break;
        case 133: return "fchdir"; break;
        case 134: return "bdflush"; break;
        case 135: return "sysfs"; break;
        case 136: return "personality"; break;
        case 137: return "afs_syscall"; break;
        case 138: return "setfsuid"; break;
        case 139: return "setfsgid"; break;
        case 140: return "_llseek"; break;
        case 141: return "getdents"; break;
        case 142: return "_newselect"; break;
        case 143: return "flock"; break;
        case 144: return "msync"; break;
        case 145: return "readv"; break;
        case 146: return "writev"; break;
        case 147: return "getsid"; break;
        case 148: return "fdatasync"; break;
        case 149: return "_sysctl"; break;
        case 150: return "mlock"; break;
        case 151: return "munlock"; break;
        case 152: return "mlockall"; break;
        case 153: return "munlockall"; break;
        case 154: return "sched_setparam"; break;
        case 155: return "sched_getparam"; break;
        case 156: return "sched_setscheduler"; break;
        case 157: return "sched_getscheduler"; break;
        case 158: return "sched_yield"; break;
        case 159: return "sched_get_priority_max"; break;
        case 160: return "sched_get_priority_min"; break;
        case 161: return "sched_rr_get_interval"; break;
        case 162: return "nanosleep"; break;
        case 163: return "mremap"; break;
        case 164: return "setresuid"; break;
        case 165: return "getresuid"; break;
        case 166: return "vm86"; break;
        case 167: return "query_module"; break;
        case 168: return "poll"; break;
        case 169: return "nfsservctl"; break;
        case 170: return "setresgid"; break;
        case 171: return "getresgid"; break;
        case 172: return "prctl"; break;
        case 173: return "rt_sigreturn"; break;
        case 174: return "rt_sigaction"; break;
        case 175: return "rt_sigprocmask"; break;
        case 176: return "rt_sigpending"; break;
        case 177: return "rt_sigtimedwait"; break;
        case 178: return "rt_sigqueueinfo"; break;
        case 179: return "rt_sigsuspend"; break;
        case 180: return "pread64"; break;
        case 181: return "pwrite64"; break;
        case 182: return "chown"; break;
        case 183: return "getcwd"; break;
        case 184: return "capget"; break;
        case 185: return "capset"; break;
        case 186: return "sigaltstack"; break;
        case 187: return "sendfile"; break;
        case 188: return "getpmsg"; break;
        case 189: return "putpmsg"; break;
        case 190: return "vfork"; break;
        case 191: return "ugetrlimit"; break;
        case 192: return "mmap2"; break;
        case 193: return "truncate64"; break;
        case 194: return "ftruncate64"; break;
        case 195: return "stat64"; break;
        case 196: return "lstat64"; break;
        case 197: return "fstat64"; break;
        case 198: return "lchown32"; break;
        case 199: return "getuid32"; break;
        case 200: return "getgid32"; break;
        case 201: return "geteuid32"; break;
        case 202: return "getegid32"; break;
        case 203: return "setreuid32"; break;
        case 204: return "setregid32"; break;
        case 205: return "getgroups32"; break;
        case 206: return "setgroups32"; break;
        case 207: return "fchown32"; break;
        case 208: return "setresuid32"; break;
        case 209: return "getresuid32"; break;
        case 210: return "setresgid32"; break;
        case 211: return "getresgid32"; break;
        case 212: return "chown32"; break;
        case 213: return "setuid32"; break;
        case 214: return "setgid32"; break;
        case 215: return "setfsuid32"; break;
        case 216: return "setfsgid32"; break;
        case 217: return "pivot_root"; break;
        case 218: return "mincore"; break;
        case 219: return "madvise1"; break;
        case 220: return "getdents64"; break;
        case 221: return "fcntl64"; break;
        case 224: return "gettid"; break;
        case 225: return "readahead"; break;
        case 226: return "setxattr"; break;
        case 227: return "lsetxattr"; break;
        case 228: return "fsetxattr"; break;
        case 229: return "getxattr"; break;
        case 230: return "lgetxattr"; break;
        case 231: return "fgetxattr"; break;
        case 232: return "listxattr"; break;
        case 233: return "llistxattr"; break;
        case 234: return "flistxattr"; break;
        case 235: return "removexattr"; break;
        case 236: return "lremovexattr"; break;
        case 237: return "fremovexattr"; break;
        case 238: return "tkill"; break;
        case 239: return "sendfile64"; break;
        case 240: return "futex"; break;
        case 241: return "sched_setaffinity"; break;
        case 242: return "sched_getaffinity"; break;
        case 243: return "set_thread_area"; break;
        case 244: return "get_thread_area"; break;
        case 245: return "io_setup"; break;
        case 246: return "io_destroy"; break;
        case 247: return "io_getevents"; break;
        case 248: return "io_submit"; break;
        case 249: return "io_cancel"; break;
        case 250: return "fadvise64"; break;
        case 252: return "exit_group"; break;
        case 253: return "lookup_dcookie"; break;
        case 254: return "epoll_create"; break;
        case 255: return "epoll_ctl"; break;
        case 256: return "epoll_wait"; break;
        case 257: return "remap_file_pages"; break;
        case 258: return "set_tid_address"; break;
        case 259: return "timer_create"; break;
        case 260: return "timer_settime"; break;
        case 261: return "timer_gettime"; break;
        case 262: return "timer_getoverrun"; break;
        case 263: return "timer_delete"; break;
        case 264: return "clock_settime"; break;
        case 265: return "clock_gettime"; break;
        case 266: return "clock_getres"; break;
        case 267: return "clock_nanosleep"; break;
        case 268: return "statfs64"; break;
        case 269: return "fstatfs64"; break;
        case 270: return "tgkill"; break;
        case 271: return "utimes"; break;
        case 272: return "fadvise64_64"; break;
        case 273: return "vserver"; break;
        case 274: return "mbind"; break;
        case 275: return "get_mempolicy"; break;
        case 276: return "set_mempolicy"; break;
        case 277: return "mq_open"; break;
        case 278: return "mq_unlink"; break;
        case 279: return "mq_timedsend"; break;
        case 280: return "mq_timedreceive"; break;
        case 281: return "mq_notify"; break;
        case 282: return "mq_getsetattr"; break;
        case 283: return "kexec_load"; break;
        case 284: return "waitid"; break;
        case 285: return "sys_setaltroot"; break;
        case 286: return "add_key"; break;
        case 287: return "request_key"; break;
        case 288: return "keyctl"; break;
        case 289: return "ioprio_set"; break;
        case 290: return "ioprio_get"; break;
        case 291: return "inotify_init"; break;
        case 292: return "inotify_add_watch"; break;
        case 293: return "inotify_rm_watch"; break;
        case 294: return "migrate_pages"; break;
        case 295: return "openat"; break;
        case 296: return "mkdirat"; break;
        case 297: return "mknodat"; break;
        case 298: return "fchownat"; break;
        case 299: return "futimesat"; break;
        case 300: return "fstatat64"; break;
        case 301: return "unlinkat"; break;
        case 302: return "renameat"; break;
        case 303: return "linkat"; break;
        case 304: return "symlinkat"; break;
        case 305: return "readlinkat"; break;
        case 306: return "fchmodat"; break;
        case 307: return "faccessat"; break;
        case 308: return "pselect6"; break;
        case 309: return "ppoll"; break;
        case 310: return "unshare"; break;
        case 311: return "set_robust_list"; break;
        case 312: return "get_robust_list"; break;
        case 313: return "splice"; break;
        case 314: return "sync_file_range"; break;
        case 315: return "tee"; break;
        case 316: return "vmsplice"; break;
        case 317: return "move_pages"; break;
        case 318: return "getcpu"; break;
        case 319: return "epoll_pwait"; break;
        case 320: return "utimensat"; break;
        case 321: return "signalfd"; break;
        case 322: return "timerfd_create"; break;
        case 323: return "eventfd"; break;
        case 324: return "fallocate"; break;
        case 325: return "timerfd_settime"; break;
        case 326: return "timerfd_gettime"; break;
        case 327: return "signalfd4"; break;
        case 328: return "eventfd2"; break;
        case 329: return "epoll_create1"; break;
        case 330: return "dup3"; break;
        case 331: return "pipe2"; break;
        case 332: return "inotify_init1"; break;
        case 333: return "preadv"; break;
        case 334: return "pwritev"; break;
        case 335: return "rt_tgsigqueueinfo"; break;
        case 336: return "perf_event_open"; break;
        default: return "UNKNOWN";  break; 
    }
}


char * get_linux_syscall_name64 (uint_t syscall_nr) { 

    switch (syscall_nr) { 

        case 0: return "read"; break;
        case 1: return "write"; break;
        case 2: return "open"; break;
        case 3: return "close"; break;
        case 4: return "stat"; break;
        case 5: return "fstat"; break;
        case 6: return "lstat"; break;
        case 7: return "poll"; break;
        case 8: return "lseek"; break;
        case 9: return "mmap"; break;
        case 10: return "mprotect"; break;
        case 11: return "munmap"; break;
        case 12: return "brk"; break;
        case 13: return "rt_sigaction"; break;
        case 14: return "rt_sigprocmask"; break;
        case 15: return "rt_sigreturn"; break;
        case 16: return "ioctl"; break;
        case 17: return "pread64"; break;
        case 18: return "pwrite64"; break;
        case 19: return "readv"; break;
        case 20: return "writev"; break;
        case 21: return "access"; break;
        case 22: return "pipe"; break;
        case 23: return "select"; break;
        case 24: return "sched_yield"; break;
        case 25: return "mremap"; break;
        case 26: return "msync"; break;
        case 27: return "mincore"; break;
        case 28: return "madvise"; break;
        case 29: return "shmget"; break;
        case 30: return "shmat"; break;
        case 31: return "shmctl"; break;
        case 32: return "dup"; break;
        case 33: return "dup2"; break;
        case 34: return "pause"; break;
        case 35: return "nanosleep"; break;
        case 36: return "getitimer"; break;
        case 37: return "alarm"; break;
        case 38: return "setitimer"; break;
        case 39: return "getpid"; break;
        case 40: return "sendfile"; break;
        case 41: return "socket"; break;
        case 42: return "connect"; break;
        case 43: return "accept"; break;
        case 44: return "sendto"; break;
        case 45: return "recvfrom"; break;
        case 46: return "sendmsg"; break;
        case 47: return "recvmsg"; break;
        case 48: return "shutdown"; break;
        case 49: return "bind"; break;
        case 50: return "listen"; break;
        case 51: return "getsockname"; break;
        case 52: return "getpeername"; break;
        case 53: return "socketpair"; break;
        case 54: return "setsockopt"; break;
        case 55: return "getsockopt"; break;
        case 56: return "clone"; break;
        case 57: return "fork"; break;
        case 58: return "vfork"; break;
        case 59: return "execve"; break;
        case 60: return "exit"; break;
        case 61: return "wait4"; break;
        case 62: return "kill"; break;
        case 63: return "uname"; break;
        case 64: return "semget"; break;
        case 65: return "semop"; break;
        case 66: return "semctl"; break;
        case 67: return "shmdt"; break;
        case 68: return "msgget"; break;
        case 69: return "msgsnd"; break;
        case 70: return "msgrcv"; break;
        case 71: return "msgctl"; break;
        case 72: return "fcntl"; break;
        case 73: return "flock"; break;
        case 74: return "fsync"; break;
        case 75: return "fdatasync"; break;
        case 76: return "truncate"; break;
        case 77: return "ftruncate"; break;
        case 78: return "getdents"; break;
        case 79: return "getcwd"; break;
        case 80: return "chdir"; break;
        case 81: return "fchdir"; break;
        case 82: return "rename"; break;
        case 83: return "mkdir"; break;
        case 84: return "rmdir"; break;
        case 85: return "creat"; break;
        case 86: return "link"; break;
        case 87: return "unlink"; break;
        case 88: return "symlink"; break;
        case 89: return "readlink"; break;
        case 90: return "chmod"; break;
        case 91: return "fchmod"; break;
        case 92: return "chown"; break;
        case 93: return "fchown"; break;
        case 94: return "lchown"; break;
        case 95: return "umask"; break;
        case 96: return "gettimeofday"; break;
        case 97: return "getrlimit"; break;
        case 98: return "getrusage"; break;
        case 99: return "sysinfo"; break;
        case 100: return "times"; break;
        case 101: return "ptrace"; break;
        case 102: return "getuid"; break;
        case 103: return "syslog"; break;
        case 104: return "getgid"; break;
        case 105: return "setuid"; break;
        case 106: return "setgid"; break;
        case 107: return "geteuid"; break;
        case 108: return "getegid"; break;
        case 109: return "setpgid"; break;
        case 110: return "getppid"; break;
        case 111: return "getpgrp"; break;
        case 112: return "setsid"; break;
        case 113: return "setreuid"; break;
        case 114: return "setregid"; break;
        case 115: return "getgroups"; break;
        case 116: return "setgroups"; break;
        case 117: return "setresuid"; break;
        case 118: return "getresuid"; break;
        case 119: return "setresgid"; break;
        case 120: return "getresgid"; break;
        case 121: return "getpgid"; break;
        case 122: return "setfsuid"; break;
        case 123: return "setfsgid"; break;
        case 124: return "getsid"; break;
        case 125: return "capget"; break;
        case 126: return "capset"; break;
        case 127: return "rt_sigpending"; break;
        case 128: return "rt_sigtimedwait"; break;
        case 129: return "rt_sigqueueinfo"; break;
        case 130: return "rt_sigsuspend"; break;
        case 131: return "sigaltstack"; break;
        case 132: return "utime"; break;
        case 133: return "mknod"; break;
        case 134: return "uselib"; break;
        case 135: return "personality"; break;
        case 136: return "ustat"; break;
        case 137: return "statfs"; break;
        case 138: return "fstatfs"; break;
        case 139: return "sysfs"; break;
        case 140: return "getpriority"; break;
        case 141: return "setpriority"; break;
        case 142: return "sched_setparam"; break;
        case 143: return "sched_getparam"; break;
        case 144: return "sched_setscheduler"; break;
        case 145: return "sched_getscheduler"; break;
        case 146: return "sched_get_priority_max"; break;
        case 147: return "sched_get_priority_min"; break;
        case 148: return "sched_rr_get_interval"; break;
        case 149: return "mlock"; break;
        case 150: return "munlock"; break;
        case 151: return "mlockall"; break;
        case 152: return "munlockall"; break;
        case 153: return "vhangup"; break;
        case 154: return "modify_ldt"; break;
        case 155: return "pivot_root"; break;
        case 156: return "_sysctl"; break;
        case 157: return "prctl"; break;
        case 158: return "arch_prctl"; break;
        case 159: return "adjtimex"; break;
        case 160: return "setrlimit"; break;
        case 161: return "chroot"; break;
        case 162: return "sync"; break;
        case 163: return "acct"; break;
        case 164: return "settimeofday"; break;
        case 165: return "mount"; break;
        case 166: return "umount2"; break;
        case 167: return "swapon"; break;
        case 168: return "swapoff"; break;
        case 169: return "reboot"; break;
        case 170: return "sethostname"; break;
        case 171: return "setdomainname"; break;
        case 172: return "iopl"; break;
        case 173: return "ioperm"; break;
        case 174: return "create_module"; break;
        case 175: return "init_module"; break;
        case 176: return "delete_module"; break;
        case 177: return "get_kernel_syms"; break;
        case 178: return "query_module"; break;
        case 179: return "quotactl"; break;
        case 180: return "nfsservctl"; break;
        case 181: return "getpmsg"; break;
        case 182: return "putpmsg"; break;
        case 183: return "afs_syscall"; break;
        case 184: return "tuxcall"; break;
        case 185: return "security"; break;
        case 186: return "gettid"; break;
        case 187: return "readahead"; break;
        case 188: return "setxattr"; break;
        case 189: return "lsetxattr"; break;
        case 190: return "fsetxattr"; break;
        case 191: return "getxattr"; break;
        case 192: return "lgetxattr"; break;
        case 193: return "fgetxattr"; break;
        case 194: return "listxattr"; break;
        case 195: return "llistxattr"; break;
        case 196: return "flistxattr"; break;
        case 197: return "removexattr"; break;
        case 198: return "lremovexattr"; break;
        case 199: return "fremovexattr"; break;
        case 200: return "tkill"; break;
        case 201: return "time"; break;
        case 202: return "futex"; break;
        case 203: return "sched_setaffinity"; break;
        case 204: return "sched_getaffinity"; break;
        case 205: return "set_thread_area"; break;
        case 206: return "io_setup"; break;
        case 207: return "io_destroy"; break;
        case 208: return "io_getevents"; break;
        case 209: return "io_submit"; break;
        case 210: return "io_cancel"; break;
        case 211: return "get_thread_area"; break;
        case 212: return "lookup_dcookie"; break;
        case 213: return "epoll_create"; break;
        case 214: return "epoll_ctl_old"; break;
        case 215: return "epoll_wait_old"; break;
        case 216: return "remap_file_pages"; break;
        case 217: return "getdents64"; break;
        case 218: return "set_tid_address"; break;
        case 219: return "restart_syscall"; break;
        case 220: return "semtimedop"; break;
        case 221: return "fadvise64"; break;
        case 222: return "timer_create"; break;
        case 223: return "timer_settime"; break;
        case 224: return "timer_gettime"; break;
        case 225: return "timer_getoverrun"; break;
        case 226: return "timer_delete"; break;
        case 227: return "clock_settime"; break;
        case 228: return "clock_gettime"; break;
        case 229: return "clock_getres"; break;
        case 230: return "clock_nanosleep"; break;
        case 231: return "exit_group"; break;
        case 232: return "epoll_wait"; break;
        case 233: return "epoll_ctl"; break;
        case 234: return "tgkill"; break;
        case 235: return "utimes"; break;
        case 236: return "vserver"; break;
        case 237: return "mbind"; break;
        case 238: return "set_mempolicy"; break;
        case 239: return "get_mempolicy"; break;
        case 240: return "mq_open"; break;
        case 241: return "mq_unlink"; break;
        case 242: return "mq_timedsend"; break;
        case 243: return "mq_timedreceive"; break;
        case 244: return "mq_notify"; break;
        case 246: return "kexec_load"; break;
        case 247: return "waitid"; break;
        case 248: return "add_key"; break;
        case 249: return "request_key"; break;
        case 250: return "keyctl"; break;
        case 251: return "ioprio_set"; break;
        case 252: return "ioprio_get"; break;
        case 253: return "inotify_init"; break;
        case 254: return "inotify_add_watch"; break;
        case 255: return "inotify_rm_watch"; break;
        case 256: return "migrate_pages"; break;
        case 257: return "openat"; break;
        case 258: return "mkdirat"; break;
        case 259: return "mknodat"; break;
        case 260: return "fchownat"; break;
        case 261: return "futimesat"; break;
        case 262: return "newfstatat"; break;
        case 263: return "unlinkat"; break;
        case 264: return "renameat"; break;
        case 265: return "linkat"; break;
        case 266: return "symlinkat"; break;
        case 267: return "readlinkat"; break;
        case 268: return "fchmodat"; break;
        case 269: return "faccessat"; break;
        case 270: return "pselect6"; break;
        case 271: return "ppoll"; break;
        case 272: return "unshare"; break;
        case 273: return "set_robust_list"; break;
        case 274: return "get_robust_list"; break;
        case 275: return "splice"; break;
        case 276: return "tee"; break;
        case 277: return "sync_file_range"; break;
        case 278: return "vmsplice"; break;
        case 279: return "move_pages"; break;
        case 280: return "utimensat"; break;
        case 281: return "epoll_pwait"; break;
        case 282: return "signalfd"; break;
        case 283: return "timerfd_create"; break;
        case 284: return "eventfd"; break;
        case 285: return "fallocate"; break;
        case 286: return "timerfd_settime"; break;
        case 287: return "timerfd_gettime"; break;
        case 288: return "accept4"; break;
        case 289: return "signalfd4"; break;
        case 290: return "eventfd2"; break;
        case 291: return "epoll_create1"; break;
        case 292: return "dup3"; break;
        case 293: return "pipe2"; break;
        case 294: return "inotify_init1"; break;
        case 295: return "preadv"; break;
        case 296: return "pwritev"; break;
        case 297: return "rt_tgsigqueueinfo"; break;
        case 298: return "perf_event_open"; break;
        default: return "UNKNOWN";  break; 
    }
}
