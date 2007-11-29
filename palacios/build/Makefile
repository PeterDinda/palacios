# Makefile for GeekOS kernel, userspace, and tools
# Copyright (c) 2004,2005 David H. Hovemeyer <daveho@cs.umd.edu>
# $Revision: 1.1.1.1 $

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



# THESE MUST MATCH WHAT IS DEFINED IN defs.h and defs.asm exactly
# MUST BE INTEGRAL NUMBER OF PAGES
TOP_OF_MEM := 0x40000000
VM_SIZE    := 0x20000000
VM_START   := 0x0

#
# THE KERNEL, SETUP, BOOTPACKAGE MUST BE SMALLER THAN THIS
# MUST BE INTEGRAL NUMBER OF PAGES
# define 
MAX_VMM := 0x60000

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
KERNEL_BASE_ADDR := $(shell perl -e 'print sprintf("0x%x",$(TOP_OF_MEM)-4096*3-$(MAX_VMM));')

# Kernel entry point function
KERNEL_ENTRY = $(SYM_PFX)Main


PROJECT_ROOT := ..
VPATH := $(PROJECT_ROOT)/src

#when -DNDEBUG is set the kassert functions are disabled
#JRLDEBUG=-DNDEBUG
JRLDEBUG= -DSERIAL_PRINT_DEBUG=1 -DSERIAL_PRINT_DEBUG_LEVEL=1000 -DSERIAL_PRINT=1

#
#
#Peter's compile flags
PADFLAGS = -DMAX_VMM=$(MAX_VMM)

# Figure out if we're compiling with cygwin, http://cygwin.com
SYSTEM_NAME := $(shell uname -s)
ifeq ($(findstring CYGWIN,$(SYSTEM_NAME)),CYGWIN)
SYM_PFX            := _
EXTRA_C_OPTS       := -DNEED_UNDERSCORE -DGNU_WIN32
EXTRA_NASM_OPTS    := -DNEED_UNDERSCORE
NON_ELF_SYSTEM     := yes
EXTRA_CC_USER_OPTS := -Dmain=geekos_main
endif

VMM_SIZES = ../include/geekos/vmm_sizes.h


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
ALL_TARGETS := fd.img  rombios vgabios vmxassist


# Kernel source files
KERNEL_C_SRCS := idt.c int.c trap.c irq.c io.c \
	keyboard.c screen.c timer.c \
	mem.c crc32.c \
	gdt.c tss.c segment.c \
	bget.c malloc.c \
	synch.c kthread.c \
	serial.c  reboot.c \
        paging.c vmx.c vmcs_gen.c vmcs.c\
	main.c

# Kernel object files built from C source files
KERNEL_C_OBJS := $(KERNEL_C_SRCS:%.c=geekos/%.o)

# Kernel assembly files
KERNEL_ASM_SRCS := lowlevel.asm vmx_lowlevel.asm

KERNEL_GAS_SRCS := testvm.s

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

# Target C compiler.  gcc 2.95.2 or later should work.
TARGET_CC := $(TARGET_CC_PREFIX)gcc

# Host C compiler.  This is used to compile programs to execute on
# the host platform, not the target (x86) platform.  On x86/ELF
# systems, such as Linux and FreeBSD, it can generally be the same
# as the target C compiler.
HOST_CC := gcc

# Target linker.  GNU ld is probably to only one that will work.
TARGET_LD := $(TARGET_CC_PREFIX)ld

# Target archiver
TARGET_AR := $(TARGET_CC_PREFIX)ar

# Target ranlib
TARGET_RANLIB := $(TARGET_CC_PREFIX)ranlib

# Target nm
TARGET_NM := $(TARGET_CC_PREFIX)nm

# Target objcopy
TARGET_OBJCOPY := $(TARGET_CC_PREFIX)objcopy

# Nasm (http://nasm.sourceforge.net)
NASM := /usr/local/nasm-vmx/bin/nasm

AS = as

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
GENERAL_OPTS := -O -Wall $(EXTRA_C_OPTS) $(JRLDEBUG) $(PADFLAGS)
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


geekos/vmx_lowlevel.o: $(PROJECT_ROOT)/src/geekos/vmx_lowlevel.asm
	$(NASM) -O99 \
	-f elf \
		-I$(PROJECT_ROOT)/src/geekos/ \
		$(PROJECT_ROOT)/src/geekos/vmx_lowlevel.asm \
	-o $@


geekos/test: geekos/test.o geekos/vmcs.o geekos/vmx_lowlevel.o 
	$(CC) geekos/test.o geekos/vmcs.o geekos/vmx_lowlevel.o  -o geekos/test

# Standard floppy image - just boots the kernel
fd.img : geekos/fd_boot.bin geekos/setup.bin geekos/kernel.bin rombios vgabios vmxassist
	cat geekos/fd_boot.bin geekos/setup.bin geekos/kernel.bin > _temp
	$(PAD) _temp 512
# Note - second copy of rombios is intentional 
	cat _temp rombios vgabios vmxassist rombios > $@

# make ready to boot over PXE
pxe:	fd.img
	cp fd.img /tftpboot/geekos.img
	$(PAD) /tftpboot/geekos.img 1474560


pxe-discovery-pdinda:	fd.img
	cp fd.img geekos.img
	$(PAD) geekos.img 1474560
	/usr/local/vmm-util/pxe_cp geekos.img
	/usr/local/vmm-util/tty_perm pdinda
	echo "Copied file to PXE boot area and set serial permissions for pdinda"


pxe-discovery-bjp600:	fd.img
	cp fd.img geekos.img
	$(PAD) geekos.img 1474560
	/usr/local/vmm-util/pxe_cp geekos.img
	/usr/local/vmm-util/tty_perm bjp600
	echo "Copied file to PXE boot area and set serial permissions for pdinda"


# Floppy boot sector (first stage boot loader).
geekos/fd_boot.bin : geekos/setup.bin geekos/kernel.bin $(PROJECT_ROOT)/src/geekos/fd_boot.asm rombios vgabios vmxassist
	$(NASM) -f bin \
		-I$(PROJECT_ROOT)/src/geekos/ \
		-DNUM_SETUP_SECTORS=`$(NUMSECS) geekos/setup.bin` \
		-DNUM_KERN_SECTORS=`$(NUMSECS) geekos/kernel.bin` \
		-DNUM_BIOS_SECTORS=`$(NUMSECS) rombios` \
		-DNUM_VGA_BIOS_SECTORS=`$(NUMSECS) vgabios` \
		-DNUM_VMXASSIST_SECTORS=`$(NUMSECS) vmxassist` \
		$(PROJECT_ROOT)/src/geekos/fd_boot.asm \
		-o $@

# Setup program (second stage boot loader).
geekos/setup.bin : geekos/kernel.exe $(PROJECT_ROOT)/src/geekos/setup.asm
	$(NASM) -f bin \
		-I$(PROJECT_ROOT)/src/geekos/ \
		-DENTRY_POINT=0x`egrep 'Main$$' geekos/kernel.syms |awk '{print $$1}'` \
	        -DVMM_FINAL_ADDR=$(KERNEL_BASE_ADDR) \
	        -DVMM_SIZE=$(MAX_VMM) \
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


generate_sizes: force
	echo "#ifndef __vmm_sizes" > $(VMM_SIZES)
	echo "#define __vmm_sizes" >> $(VMM_SIZES)
	echo "#define KERNEL_LOAD_ADDRESS " $(KERNEL_BASE_ADDR)  >> $(VMM_SIZES) 
	echo "#define KERNEL_SETUP_LENGTH (" `$(NUMSECS) geekos/setup.bin` "*512)" >> $(VMM_SIZES) 
	echo "#define KERNEL_CORE_LENGTH (" `$(NUMSECS)  geekos/kernel.bin` "*512)" >> $(VMM_SIZES) 
	echo "#define KERNEL_START (KERNEL_LOAD_ADDRESS)" >> $(VMM_SIZES)
	echo "#define KERNEL_END (KERNEL_LOAD_ADDRESS+KERNEL_CORE_LENGTH-1)" >> $(VMM_SIZES)
	echo "#define BIOS_LENGTH (" `$(NUMSECS) rombios` "*512)" >> $(VMM_SIZES) 
	echo "#define VGA_BIOS_LENGTH (" `$(NUMSECS) vgabios` "*512)" >> $(VMM_SIZES) 
	echo "#define VMXASSIST_LENGTH (" `$(NUMSECS) vmxassist` "*512)" >> $(VMM_SIZES) 
	echo "#define BIOS_START (KERNEL_LOAD_ADDRESS+KERNEL_CORE_LENGTH)" >> $(VMM_SIZES) 
	echo "#define VGA_BIOS_START (BIOS_START+BIOS_LENGTH)" >> $(VMM_SIZES)
	echo "#define VMXASSIST_START (VGA_BIOS_START+VGA_BIOS_LENGTH)" >> $(VMM_SIZES)
	echo "//Note this is a second copy of the rom bios for debug" >> $(VMM_SIZES)
	echo "#define BIOS2_START (VMXASSIST_START+VMXASSIST_LENGTH)" >> $(VMM_SIZES)
	echo "#define VM_BOOT_PACKAGE_START (BIOS_START) " >> $(VMM_SIZES)
	echo "#define VM_BOOT_PACKAGE_END  (BIOS2_START+BIOS_LENGTH-1) " >> $(VMM_SIZES)
	echo "#endif" >> $(VMM_SIZES)

make_show_sizes: generate_sizes ../src/geekos/show_sizes.c
	$(HOST_CC) -I../include/geekos ../src/geekos/show_sizes.c -o show_sizes

show_sizes: make_show_sizes
	./show_sizes


get_kernel_size: make_show_sizes
	./show_sizes | grep 

force:


rombios: force
	(cd ../src/vmboot/rombios; make)
	cp ../src/vmboot/rombios/BIOS-bochs-latest rombios
	$(PAD) rombios 512
	@echo "Rom bios lives at f000:0000 and is" `$(NUMSECS) rombios` "sectors long"


vgabios: force
	(cd ../src/vmboot/vgabios;  make)
	cp ../src/vmboot/vgabios/vgabios.bin vgabios
	$(PAD) vgabios 512
	@echo "Vga bios lives at c000:0000 and is" `$(NUMSECS) vgabios` "sectors long"

vmxassist: force
	(cd ../src/vmboot/vmxassist; make)
	cp ../src/vmboot/vmxassist/vmxassist.bin vmxassist
	$(PAD) vmxassist 512
	@echo "vmxassist lives at d000:0000 and is" `$(NUMSECS) vmxassist` "sectors long"

# Clean build directories of generated files
clean :
	rm -f rombios vgabios vmxassist
	(cd ../src/vmboot/rombios; make clean)
	(cd ../src/vmboot/vgabios;  make clean)
	(cd ../src/vmboot/vmxassist; make clean)
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
