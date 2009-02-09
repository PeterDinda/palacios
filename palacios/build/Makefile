# Makefile for GeekOS kernel, userspace, and tools
#
#  Northwestern University 
# (c) 2008, Jack Lange <jarusl@cs.northwestern.edu>
# (c) 2008, Peter Dinda <pdinda@northwestern.edu> 
# (c) 2008, Lei Xia <lxia@northwestern.edu>
# (c) 2008, The V3VEE Project <http://www.v3vee.org> 
#
# Based on GeekOS Makefile:
# Copyright (c) 2004,2005 David H. Hovemeyer <daveho@cs.umd.edu>
# $Revision: 1.71 $


# This is free software.  You are permitted to use,
# redistribute, and modify it as specified in the file "COPYING".

# Required software to build GeekOS:
# - GNU Make (http://www.gnu.org/software/make)
# - gcc 2.95.2 generating code for target (i386/ELF) and host platforms
# - Perl5, AWK (any version), egrep
#
# Cygwin (http://cygwin.com) may be used to build GeekOS.
# Make sure that gcc, binutils, nasm, and perl are installed.

# NOTES:
# - This makefile has been written carefully to work correctly
#   with the -j (parallel make) option.  I regularly use "make -j 2"
#   to speed the build process on 2 processor systems.


# Base address of kernel
#
# Note: at top of memory minus three pages (GDT/TSS/IDT) 
# minus maximum size
#
#
# Note that the code will initially load at 0x10000
#
# The setup code needs to copy it up to this address and jump there
#




PROJECT_ROOT := ..
VPATH := $(PROJECT_ROOT)/src

#when -DNDEBUG is set the kassert functions are disabled
#JRLDEBUG=-DNDEBUG

# DEBUG=1 means VMM_DEBUG, VMM_INFO, and VMM_TRACE are enabled
# as are SERIAL_PRINT_DEBUG
# 

ifeq ($(LEAN_AND_MEAN),1) 
DEBUG := 0
DEBUG_SECTIONS :=
else
DEBUG := 1
DEBUG_SECTIONS := 
endif


ifeq ($(DEBUG_ALL),1)
  DEBUG_SECTIONS:= $(DEBUG_SECTIONS) -DDEBUG_SHADOW_PAGING -DDEBUG_CTRL_REGS -DDEBUG_INTERRUPTS -DDEBUG_KEYBOARD -DDEBUG_PIC -DDEBUG_PIT -DDEBUG_NVRAM -DDEBUG_EMULATOR  -DDEBUG_XED -DDEBUG_HALT -DDEBUG_DEV_MGR
# -DDEBUG_IO -DDEBUG_GENERIC -DDEBUG_RAMDISK
endif


ifeq ($(DEBUG_SHADOW_PAGING),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_SHADOW_PAGING
else 
ifeq ($(DEBUG_SHADOW_PAGING),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_SHADOW_PAGING
endif
endif

ifeq ($(DEBUG_CTRL_REGS),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_CTRL_REGS
else 
ifeq ($(DEBUG_CTRL_REGS),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_CTRL_REGS
endif
endif

ifeq ($(DEBUG_INTERRUPTS),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_INTERRUPTS
else 
ifeq ($(DEBUG_INTERRUPTS),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_INTERRUPTS
endif
endif

ifeq ($(DEBUG_IO),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_IO
else 
ifeq ($(DEBUG_IO),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_IO
endif
endif

ifeq ($(DEBUG_KEYBOARD),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_KEYBOARD
else 
ifeq ($(DEBUG_KEYBOARD),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_KEYBOARD
endif
endif

ifeq ($(DEBUG_PIC),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_PIC
else 
ifeq ($(DEBUG_PIC),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_PIC
endif
endif

ifeq ($(DEBUG_PIT),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_PIT
else 
ifeq ($(DEBUG_PIT),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_PIT
endif
endif

ifeq ($(DEBUG_NVRAM),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_NVRAM
else 
ifeq ($(DEBUG_NVRAM),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_NVRAM
endif
endif

ifeq ($(DEBUG_GENERIC),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_GENERIC
else 
ifeq ($(DEBUG_GENERIC),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_GENERIC
endif
endif

ifeq ($(DEBUG_EMULATOR),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_EMULATOR
else 
ifeq ($(DEBUG_EMULATOR),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_EMULATOR
endif
endif

ifeq ($(DEBUG_RAMDISK),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_RAMDISK
else 
ifeq ($(DEBUG_RAMDISK),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_RAMDISK
endif
endif

ifeq ($(TRACE_RAMDISK),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DTRACE_RAMDISK
else
ifeq ($(TRACE_RAMDSK),0)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UTRACE_RAMDISK
endif
endif

ifeq ($(DEBUG_XED),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_XED
else 
ifeq ($(DEBUG_XED),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_XED
endif
endif

ifeq ($(DEBUG_HALT),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_HALT
else 
ifeq ($(DEBUG_HALT),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_HALT
endif
endif

ifeq ($(DEBUG_DEV_MGR),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DDEBUG_DEV_MGR
else 
ifeq ($(DEBUG_DEV_MGR),0)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) -UDEBUG_DEV_MGR
endif
endif

#DEBUG_SECTIONS := $(DEBUG_SECTIONS) -DTEST_NE2K

ifeq ($(DEBUG),1)
  JRLDEBUG=  -DVMM_DEBUG -DVMM_INFO -DVMM_TRACE $(DEBUG_SECTIONS)
else

endif






#
# This is wrong for current cygwin - no changes needed
#
# Figure out if we're compiling with cygwin, http://cygwin.com
#
# 
#SYSTEM_NAME := $(shell uname -s)
#ifeq ($(findstring CYGWIN,$(SYSTEM_NAME)),CYGWIN)
#SYM_PFX            := _
#EXTRA_C_OPTS       := -DNEED_UNDERSCORE -DGNU_WIN32
#EXTRA_NASM_OPTS    := -DNEED_UNDERSCORE
#NON_ELF_SYSTEM     := yes
#EXTRA_CC_USER_OPTS := -Dmain=geekos_main
#endif




# ----------------------------------------------------------------------
# Configuration -
#   Various options specifying how GeekOS should be built,
#   what source files to build, which user programs to build,
#   etc.  This is generally the only section of the makefile
#   that will need to be modified.
# ----------------------------------------------------------------------

# List of targets to build by default.
# These targets encompass everything needed to boot
# and run GeekOS.
ALL_TARGETS := vmm force_payload


VMM_OBJS := \
	palacios/vm_guest.o \
	palacios/svm.o \
	palacios/svm_handler.o \
	palacios/vmm.o \
	palacios/vmm_util.o \
	palacios/vmm_ctrl_regs.o \
	palacios/vmcb.o \
	palacios/vmm_mem.o \
	palacios/vmm_paging.o \
	palacios/vmm_io.o \
	palacios/vmm_debug.o \
	palacios/svm_io.o \
	palacios/vmm_intr.o \
	palacios/vmm_time.o \
	palacios/vmm_shadow_paging.o \
	palacios/vm_guest_mem.o \
	palacios/vm_dev.o \
	palacios/vmm_dev_mgr.o \
	palacios/vmm_decoder.o \
	palacios/svm_halt.o \
	palacios/svm_pause.o \
	palacios/svm_wbinvd.o \
	palacios/vmm_config.o \
	palacios/vmm_hashtable.o \
	palacios/vmm_string.o \
	palacios/vmm_emulator.o \
	palacios/vmm_queue.o \
	palacios/vmm_host_events.o \
	palacios/svm_lowlevel.o \
	palacios/vmm_msr.o \
	palacios/svm_msr.o \
	palacios/vmm_socket.o \
	palacios/vmm_xed.o \
	palacios/vmm_rbtree.o \
	palacios/vmm_profiler.o \
	palacios/vmm_direct_paging.o \
	palacios/vmm_ringbuffer.o \

#		vmx.c vmcs_gen.c vmcs.c

# Extra C flags for the VMM objects
$(VMM_OBJS) :: EXTRA_CFLAGS = \
	$(JRLDEBUG) \



XED_OBJS := \
	xed/v3-xed-compat.o \
	xed/v3-udiv-compat.o \

$(XED_OBJS) :: EXTRA_CFLAGS = \
	$(JRLDEBUG) \

DEVICES_OBJS := \
	devices/generic.o \
	devices/keyboard.o \
	devices/nvram.o \
	devices/timer.o \
	devices/simple_pic.o \
	devices/8259a.o \
	devices/8254.o \
	devices/serial.o \
	devices/ramdisk.o \
	devices/cdrom.o \
	devices/bochs_debug.o \
	devices/os_debug.o \
	devices/apic.o	\

$(DEVICES_OBJS) :: EXTRA_CFLAGS = \
	$(JRLDEBUG) \



# ----------------------------------------------------------------------
# Tools -
#   This section defines programs that are used to build GeekOS.
# ----------------------------------------------------------------------

ifeq ($(ARCH),64)
V3_ARCH := __V3_64BIT__
else 
V3_ARCH := __V3_32BIT__
endif


# Uncomment if cross compiling
#TARGET_CC_PREFIX :=  $(PROJECT_ROOT)/../devtools/i386/bin/i386-elf-
#TARGET_CC_PREFIX :=  i386-elf-

# Target C compiler.  gcc 2.95.2 or later should work.
ifeq ($(ARCH),64)
TARGET_CC := $(TARGET_CC_PREFIX)gcc -m64
else 
TARGET_CC := $(TARGET_CC_PREFIX)gcc -m32
endif
#TARGET_CC := $(TARGET_CC_PREFIX)gcc34 -m32


# Target linker.  GNU ld is probably to only one that will work.
TARGET_LD := $(TARGET_CC_PREFIX)ld -melf_i386

# Target archiver
TARGET_AR := $(TARGET_CC_PREFIX)ar

# Target ranlib
TARGET_RANLIB := $(TARGET_CC_PREFIX)ranlib

# Target nm
TARGET_NM := $(TARGET_CC_PREFIX)nm

# Target objcopy
TARGET_OBJCOPY := $(TARGET_CC_PREFIX)objcopy

# Nasm (http://nasm.sourceforge.net)
NASM := $(PROJECT_ROOT)/../devtools/bin/nasm
#NASM := /opt/vmm-tools/bin/nasm

CPP := cpp


ifeq ($(ARCH),64)
AS = as --64
else 
AS = as --32
endif


# Tool to build PFAT filesystem images.
BUILDFAT := tools/builtFat.exe

# Perl5 or later
PERL := perl

# Pad a file so its size is a multiple of some unit (i.e., sector size)
PAD := $(PERL) $(PROJECT_ROOT)/scripts/pad

# Create a file filled with zeroes.
ZEROFILE := $(PERL) $(PROJECT_ROOT)/scripts/zerofile

# Calculate size of file in sectors
NUMSECS := $(PERL) $(PROJECT_ROOT)/scripts/numsecs


FD_SECTORS_PER_TRACK := $(PERL) $(PROJECT_ROOT)/scripts/numsecs_per_track


# ----------------------------------------------------------------------
# Definitions -
#   Options passed to the tools.
# ----------------------------------------------------------------------

# Flags used for all C source files

CC_GENERAL_OPTS = \
	-O \
	-Wall \
	-g \
	-D__V3VEE__ \
	-D$(V3_ARCH) \
	$(EXTRA_C_OPTS) \
	$(VMM_FLAGS) \
	-I$(PROJECT_ROOT)/include \
	-Werror \
	-fPIC \
	-Wp,-MD,$(@D)/.$(@F).d \
	-Wp,-MT,$@ \

#-fPIC \
#-fvisibility=hidden



# Flags passed to objcopy program (strip unnecessary sections from kernel.exe)
OBJCOPY_FLAGS := -R .dynamic -R .note -R .comment

# ----------------------------------------------------------------------
# Rules -
#   Describes how to compile the source files.
# ----------------------------------------------------------------------

# Compilation of kernel C source files
# Usage:
#	$(call build,TAG,commandline)
#
# If V=1 on the command line or the environment, then the actual
# command executed will be echoed to the terminal.  Otherwise
# only the tag and the output file will be printed to make
# any warnings stand out from the compile messages.
#
build = \
	@if [ -z "$V" ]; then \
		echo '    [$1]	$@'; \
		$2; \
	else \
		echo '$2'; \
		$2; \
	fi



CC_COMPILE = \
	$(call build,CC,$(TARGET_CC) \
		$(CC_GENERAL_OPTS) \
		$(EXTRA_CFLAGS) \
		-c \
		$< \
		-o $@ \
	)

AS_COMPILE = \
	$(call build,AS,$(TARGET_CC) \
		$(CC_GENERAL_OPTS) \
		$(EXTRA_CFLAGS) \
		-c \
		$< \
		-o $@ \
	)


%.o: %.c
	$(CC_COMPILE)
%.o: %.S
	$(AS_COMPILE)
%.o: %.s
	$(AS_COMPILE)


# ----------------------------------------------------------------------
# Targets -
#   Specifies files to be built
# ----------------------------------------------------------------------

# Default target - see definition of ALL_TARGETS in Configuration section
all : $(ALL_TARGETS)


#geekos/vmx_lowlevel.o: $(PROJECT_ROOT)/src/geekos/vmx_lowlevel.asm
#	$(NASM) -O99 \
#	-f elf \
#		-I$(PROJECT_ROOT)/src/geekos/ \
#		$(PROJECT_ROOT)/src/geekos/vmx_lowlevel.asm \
#	-o $@


#geekos/test: geekos/test.o geekos/vmcs.o geekos/vmx_lowlevel.o 
#	$(CC) geekos/test.o geekos/vmcs.o geekos/vmx_lowlevel.o  -o geekos/test



rombios_link:
	ln -s -f ../src/vmboot/rombios/BIOS-bochs-latest rombios

vgabios_link:
	ln -s -f ../src/vmboot/vgabios/VGABIOS-lgpl-latest.bin vgabios


force_rombios: rombios_link
	(cd ../src/vmboot/rombios; make clean; make)

force_vgabios: vgabios_link
	(cd ../src/vmboot/vgabios; make clean; make)

force_payload: force_rombios force_vgabios
	../scripts/make_payload.pl payload_layout.txt vm_kernel

inter1: force_payload
	-make clean

world: inter1 vmm

vmm: palacios/vmm.lib 




# Loadable (flat) kernel image.
palacios/vmm.bin : palacios/vmm.lib
	$(TARGET_OBJCOPY) $(OBJCOPY_FLAGS) -S -O binary palacios/vmm.lib palacios/vmm.bin


# The kernel executable and symbol map.
palacios/vmm.lib: libv3vee.a
libv3vee.a: \
	$(VMM_OBJS) \
	$(DEVICES_OBJS) \
	$(XED_OBJS) \

	$(call build,AR,$(TARGET_AR) rcs $@ $^)

force:





# Clean build directories of generated files
clean :
	for d in palacios devices xed; do \
		(cd $$d && rm -f * .*.d); \
	done


# Include all of the generated dependency files if they exist
-include */.*.o.d
