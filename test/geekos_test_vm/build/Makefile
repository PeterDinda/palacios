# Makefile for GeekOS kernel, userspace, and tools
# Copyright (c) 2004,2005 David H. Hovemeyer <daveho@cs.umd.edu>
# $Revision: 1.2 $

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
KERNEL_BASE_ADDR := 0x100000

# Kernel entry point function
KERNEL_ENTRY = $(SYM_PFX)Main


PROJECT_ROOT := ..
VPATH := $(PROJECT_ROOT)/src



ifeq ($(SERIAL_DEBUG), 1)
JRLDEBUG= -DDEBUG_SERIAL
else 
JRLDEBUG= 
endif


# Figure out if we're compiling with cygwin, http://cygwin.com
SYSTEM_NAME := $(shell uname -s)
ifeq ($(findstring CYGWIN,$(SYSTEM_NAME)),CYGWIN)
SYM_PFX            := _
EXTRA_C_OPTS       := -DNEED_UNDERSCORE -DGNU_WIN32
EXTRA_NASM_OPTS    := -DNEED_UNDERSCORE
NON_ELF_SYSTEM     := yes
EXTRA_CC_USER_OPTS := -Dmain=geekos_main
endif



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
ALL_TARGETS := geekos/kernel.bin fd.img


# Kernel source files
KERNEL_C_SRCS := idt.c int.c trap.c irq.c io.c \
        blockdev.c ide.c \
	keyboard.c screen.c timer.c \
	mem.c crc32.c \
	gdt.c tss.c segment.c \
	bget.c malloc.c \
	synch.c kthread.c \
	 debug.c \
	vm_cons.c \
	pci.c \
	serial.c  reboot.c \
        paging.c \
	main.c

 # 

# Kernel object files built from C source files
KERNEL_C_OBJS := $(KERNEL_C_SRCS:%.c=geekos/%.o)

# Kernel assembly files
KERNEL_ASM_SRCS := lowlevel.asm

KERNEL_GAS_SRCS :=

# Kernel object files build from assembler source files
KERNEL_ASM_OBJS := $(KERNEL_ASM_SRCS:%.asm=geekos/%.o) 

KERNEL_GAS_OBJS := $(KERNEL_GAS_SRCS:%.s=geekos/%.o)


# All kernel object files
KERNEL_OBJS := $(KERNEL_C_OBJS) \
  $(KERNEL_ASM_OBJS) $(KERNEL_GAS_OBJS)

# Common library source files.
# This library is linked into both the kernel and user programs.
# It provides string functions and generic printf()-style
# formatted output.
COMMON_C_SRCS := fmtout.c string.c memmove.c

# Common library object files.
COMMON_C_OBJS := $(COMMON_C_SRCS:%.c=common/%.o)




# ----------------------------------------------------------------------
# Tools -
#   This section defines programs that are used to build GeekOS.
# ----------------------------------------------------------------------

# Uncomment if cross compiling
#TARGET_CC_PREFIX := i386-elf-
#TARGET_CC_PREFIX :=  $(PROJECT_ROOT)/../../devtools/i386/bin/i386-elf-

# Target C compiler.  gcc 2.95.2 or later should work.
TARGET_CC := $(TARGET_CC_PREFIX)gcc -m32


# Host C compiler.  This is used to compile programs to execute on
# the host platform, not the target (x86) platform.  On x86/ELF
# systems, such as Linux and FreeBSD, it can generally be the same
# as the target C compiler.
HOST_CC := gcc 

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
#NASM := $(PROJECT_ROOT)/../../devtools/bin/nasm
NASM := nasm

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


# ----------------------------------------------------------------------
# Definitions -
#   Options passed to the tools.
# ----------------------------------------------------------------------

# Flags used for all C source files

GENERAL_OPTS := -O -Wall $(EXTRA_C_OPTS) $(JRLDEBUG)
CC_GENERAL_OPTS := $(GENERAL_OPTS) -Werror 

# Flags used for kernel C source files
CC_KERNEL_OPTS := -g -DGEEKOS -I$(PROJECT_ROOT)/include

# Flags user for kernel assembly files
NASM_KERNEL_OPTS := -I$(PROJECT_ROOT)/src/geekos/ -f elf $(EXTRA_NASM_OPTS)

# Flags used for common library and libc source files
CC_USER_OPTS := -I$(PROJECT_ROOT)/include -I$(PROJECT_ROOT)/include/libc \
	$(EXTRA_CC_USER_OPTS)

# Flags passed to objcopy program (strip unnecessary sections from kernel.exe)
OBJCOPY_FLAGS := -R .dynamic -R .note -R .comment

# ----------------------------------------------------------------------
# Rules -
#   Describes how to compile the source files.
# ----------------------------------------------------------------------

# Compilation of kernel C source files

geekos/%.o : geekos/%.c
	$(TARGET_CC) -c $(CC_GENERAL_OPTS) $(CC_KERNEL_OPTS) $< -o geekos/$*.o


# Compilation of kernel assembly source files
geekos/%.o : geekos/%.asm
	$(NASM) $(NASM_KERNEL_OPTS) $< -o geekos/$*.o

# Compilation of test VM
geekos/%.o : geekos/%.s
	$(AS) $< -o geekos/$*.o

geekos/%.o : geekos/%.S
	$(TARGET_CC) -c $(CC_GENERAL_OPTS) $(CC_KERNEL_OPTS) $< -o geekos/$*.o

# Compilation of common library C source files
common/%.o : common/%.c
	$(TARGET_CC) -c $(CC_GENERAL_OPTS) $(CC_USER_OPTS) $< -o common/$*.o

# ----------------------------------------------------------------------
# Targets -
#   Specifies files to be built
# ----------------------------------------------------------------------

# Default target - see definition of ALL_TARGETS in Configuration section
all : $(ALL_TARGETS)

# Standard floppy image - just boots the kernel
fd.img : geekos/fd_boot.bin geekos/setup.bin geekos/kernel.bin
	cat geekos/fd_boot.bin geekos/setup.bin geekos/kernel.bin > $@



guest-img: fd.img
	cp fd.img guest.img
	$(PAD) guest.img 1474560

guest-iso: guest-img
	mkisofs -pad -b guest.img -R -o guest.iso guest.img



# Floppy boot sector (first stage boot loader).
geekos/fd_boot.bin : geekos/setup.bin geekos/kernel.bin $(PROJECT_ROOT)/src/geekos/fd_boot.asm
	$(NASM) -f bin \
		-I$(PROJECT_ROOT)/src/geekos/ \
		-DNUM_SETUP_SECTORS=`$(NUMSECS) geekos/setup.bin` \
		-DNUM_KERN_SECTORS=`$(NUMSECS) geekos/kernel.bin` \
		$(PROJECT_ROOT)/src/geekos/fd_boot.asm \
		-o $@

# Setup program (second stage boot loader).
geekos/setup.bin : geekos/kernel.exe $(PROJECT_ROOT)/src/geekos/setup.asm
	$(NASM) -f bin \
		-I$(PROJECT_ROOT)/src/geekos/ \
		-DENTRY_POINT=0x`egrep 'Main$$' geekos/kernel.syms |awk '{print $$1}'` \
		$(PROJECT_ROOT)/src/geekos/setup.asm \
		-o $@
	$(PAD) $@ 512


# Loadable (flat) kernel image.
geekos/kernel.bin : geekos/kernel.exe
	$(TARGET_OBJCOPY) $(OBJCOPY_FLAGS) -S -O binary geekos/kernel.exe geekos/kernel.bin
	$(PAD) $@ 512

# The kernel executable and symbol map.
geekos/kernel.exe : $(KERNEL_OBJS) $(COMMON_C_OBJS)
	$(TARGET_LD) -o geekos/kernel.exe -Ttext $(KERNEL_BASE_ADDR) -e $(KERNEL_ENTRY) \
		$(KERNEL_OBJS) $(COMMON_C_OBJS)
	$(TARGET_NM) geekos/kernel.exe > geekos/kernel.syms





force:

# Clean build directories of generated files
clean :
	for d in geekos common libc user tools; do \
		(cd $$d && rm -f *); \
	done


# Build header file dependencies, so source files are recompiled when
# header files they depend on are modified.
depend : $(GENERATED_LIBC_SRCS)
	$(TARGET_CC) -M $(CC_GENERAL_OPTS) $(CC_KERNEL_OPTS) \
		$(KERNEL_C_SRCS:%.c=$(PROJECT_ROOT)/src/geekos/%.c) \
		| $(PERL) -n -e 's,^(\S),geekos/$$1,;print' \
		> depend.mak
	$(TARGET_CC) -M $(CC_GENERAL_OPTS) $(CC_USER_OPTS) \
		$(COMMON_C_SRCS:%.c=$(PROJECT_ROOT)/src/common/%.c) \
		| $(PERL) -n -e 's,^(\S),common/$$1,;print' \
		>> depend.mak

# By default, there are no header file dependencies.
depend.mak :
	touch $@

include depend.mak
