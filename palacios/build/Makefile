# Makefile for GeekOS kernel, userspace, and tools
#
#  Northwestern University 
# (c) 2008, Jack Lange <jarusl@cs.northwestern.edu>
# (c) 2008, Peter Dinda <pdinda@northwestern.edu> 
# (c) 2008, Lei Xia <xiaxlei@gmail.com>
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
# - nasm (http://nasm.sourceforge.net)
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
DEBUG=0
DEBUG_SECTIONS=
else
DEBUG=1
DEBUG_SECTIONS= 
endif


ifeq ($(DEBUG_ALL),1)
  DEBUG_SECTIONS:= $(DEBUG_SECTIONS) -DDEBUG_SHADOW_PAGING -DDEBUG_CTRL_REGS -DDEBUG_INTERRUPTS -DDEBUG_IO -DDEBUG_KEYBOARD -DDEBUG_PIC -DDEBUG_PIT -DDEBUG_NVRAM -DDEBUG_EMULATOR -DDEBUG_GENERIC -DDEBUG_RAMDISK -DDEBUG_XED -DDEBUG_HALT -DDEBUG_DEV_MGR
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
# DECODER is the decoder that will be used 
# currently we only support xed
#
DECODER=XED

DECODER_FLAGS=
DECODER_SRCS=
DECODER_LIBS=

ifeq ($(DECODER),XED)
DECODER_SRCS := vmm_xed.c
DECODER_FLAGS :=  -L../lib/xed
DECODER_LIBS := $(PROJECT_ROOT)/lib/xed/libxed.a
else
# This is an error
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
ALL_TARGETS := vmm vm_kernel






VMM_ASM_SRCS :=  svm_lowlevel.asm\
#			vmx_lowlevel.asm

VMM_ASM_OBJS := $(VMM_ASM_SRCS:%.asm=palacios/%.o)


VMM_C_SRCS :=   vm_guest.c \
	 	svm.c svm_handler.c vmm.c vmm_util.c vmm_ctrl_regs.c \
		vmcb.c vmm_mem.c vmm_paging.c vmm_io.c vmm_debug.c svm_io.c \
		vmm_intr.c vmm_time.c \
		vmm_shadow_paging.c vm_guest_mem.c  \
                vm_dev.c vmm_dev_mgr.c vmm_decoder.c \
                svm_halt.c svm_pause.c svm_wbinvd.c \
	        vmm_config.c vmm_hashtable.c \
		vmm_string.c vmm_emulator.c vmm_queue.c\
		vmm_host_events.c \
		 $(DECODER_SRCS)
#		vmx.c vmcs_gen.c vmcs.c

VMM_C_OBJS := $(VMM_C_SRCS:%.c=palacios/%.o)

VMM_OBJS := $(VMM_C_OBJS) $(VMM_ASM_OBJS)



XED_C_SRCS := v3-xed-compat.c

XED_C_OBJS := $(XED_C_SRCS:%.c=xed/%.o)

XED_GAS_SRCS := v3-udiv-compat.s

XED_GAS_OBJS := $(XED_GAS_SRCS:%.s=xed/%.o)

XED_OBJS := $(XED_C_OBJS) $(XED_GAS_OBJS)



DEVICE_C_SRCS := generic.c keyboard.c nvram.c timer.c simple_pic.c 8259a.c 8254.c serial.c ramdisk.c cdrom.c bochs_debug.c

DEVICE_C_OBJS := $(DEVICE_C_SRCS:%.c=devices/%.o)

DEVICE_OBJS := $(DEVICE_C_OBJS)

V3LIBS := $(DECODER_LIBS)





# ----------------------------------------------------------------------
# Tools -
#   This section defines programs that are used to build GeekOS.
# ----------------------------------------------------------------------


V3_ARCH := __V3_32BIT__
#V3_ARCH := __V3_64BIT__

# Uncomment if cross compiling
#TARGET_CC_PREFIX :=  $(PROJECT_ROOT)/../devtools/i386/bin/i386-elf-
#TARGET_CC_PREFIX :=  i386-elf-

# Target C compiler.  gcc 2.95.2 or later should work.
TARGET_CC := $(TARGET_CC_PREFIX)gcc -m32
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

AS = as --32

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
#GENERAL_OPTS := -O -Wall $(EXTRA_C_OPTS) $(VMM_FLAGS) -fPIC #-fvisibility=hidden
GENERAL_OPTS :=  -O -Wall  $(EXTRA_C_OPTS) $(VMM_FLAGS) -fPIC -Werror#-fvisibility=hidden
CC_GENERAL_OPTS := $(GENERAL_OPTS) 

# Flags used for VMM C source files
CC_VMM_OPTS := -g -I$(PROJECT_ROOT)/include -D__V3VEE__ -D$(V3_ARCH) $(DECODER_FLAGS) $(JRLDEBUG)

# Flags used for VMM C ASM files
NASM_VMM_OPTS := -I$(PROJECT_ROOT)/src/palacios/ -f elf $(EXTRA_NASM_OPTS)





# Flags passed to objcopy program (strip unnecessary sections from kernel.exe)
OBJCOPY_FLAGS := -R .dynamic -R .note -R .comment

# ----------------------------------------------------------------------
# Rules -
#   Describes how to compile the source files.
# ----------------------------------------------------------------------



palacios/%.o : palacios/%.c
	$(TARGET_CC) -c $(CC_GENERAL_OPTS) $(CC_VMM_OPTS) $< -o palacios/$*.o

palacios/%.o : palacios/%.asm
	$(NASM) $(NASM_VMM_OPTS) $< -o palacios/$*.o

devices/%.o : devices/%.c
	$(TARGET_CC) -c $(CC_GENERAL_OPTS) $(CC_VMM_OPTS) $< -o devices/$*.o

devices/%.o : devices/%.asm
	$(NASM) $(NASM_VMM_OPTS) $< -o devices/$*.o


xed/%.o : xed/%.c
	$(TARGET_CC) -c $(CC_GENERAL_OPTS) $(CC_VMM_OPTS) $< -o xed/$*.o

xed/%.o : xed/%.s
	$(AS) $< -o xed/$*.o


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
palacios/vmm.lib: $(VMM_OBJS) $(DEVICE_OBJS) $(XED_OBJS) vm_kernel
#	$(TARGET_LD) -o palacios/vmm.lib  \
#		$(DECODER_FLAGS) \
#		 $(VMM_OBJS) $(DEVICE_OBJS) $(XED_OBJS) $(V3LIBS) -b binary vm_kernel
#	$(TARGET_NM) palacios/vmm.lib > palacios/vmm.syms
	$(TARGET_AR) rcs libv3vee.a  \
		 $(VMM_OBJS) $(DEVICE_OBJS) $(XED_OBJS)



force:





# Clean build directories of generated files
clean :
	for d in palacios devices; do \
		(cd $$d && rm -f *); \
	done


# Build header file dependencies, so source files are recompiled when
# header files they depend on are modified.
depend : $(GENERATED_LIBC_SRCS)

	$(TARGET_CC) -M $(CC_GENERAL_OPTS) $(CC_KERNEL_OPTS) \
		$(VMM_C_SRCS:%.c=$(PROJECT_ROOT)/src/palacios/%.c) \
		| $(PERL) -n -e 's,^(\S),palacios/$$1,;print' \
		>> depend.mak
	$(TARGET_CC) -M $(CC_GENERAL_OPTS) $(CC_KERNEL_OPTS) \
		$(DEVICE_C_SRCS:%.c=$(PROJECT_ROOT)/src/devices/%.c) \
		| $(PERL) -n -e 's,^(\S),devices/$$1,;print' \
		>> depend.mak

# By default, there are no header file dependencies.
depend.mak :
	touch $@

include depend.mak
