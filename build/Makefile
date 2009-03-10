# Top level Makefile for V3Vee
#
#  Northwestern University 
# (c) 2008, Jack Lange <jarusl@cs.northwestern.edu>
# (c) 2008, Peter Dinda <pdinda@northwestern.edu> 
# (c) 2008, Lei Xia <xiaxlei@gmail.com>
# (c) 2008, The V3VEE Project <http://www.v3vee.org> 
#
# Required software to build V3Vee:
# - GNU Make (http://www.gnu.org/software/make)
# - nasm (http://nasm.sourceforge.net)
# - Perl5, AWK (any version), egrep
#






PROJECT_ROOT := ..
PALACIOS_BUILD_DIR := $(PROJECT_ROOT)/palacios/build
GEEKOS_BUILD_DIR := $(PROJECT_ROOT)/geekos/build
GUEST_ISO_DIR := /opt/vmm-tools/isos

# List of targets to build by default.
# These targets encompass everything needed to boot
# and run GeekOS.
ALL_TARGETS := geekos-full

QEMU := /usr/local/qemu/bin/qemu-system-x86_64



#when -DNDEBUG is set the kassert functions are disabled
#JRLDEBUG=-DNDEBUG

# DEBUG=1 means VMM_DEBUG, VMM_INFO, and VMM_TRACE are enabled
# as are SERIAL_PRINT_DEBUG
# 
DEBUG_SECTIONS := DEBUG=1

ifeq ($(DEBUG_ALL),1)
  DEBUG_SECTIONS:= $(DEBUG_SECTIONS) DEBUG_ALL=1
endif

ifeq ($(DEBUG_SHADOW_PAGING),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_SHADOW_PAGING=1
else 
ifeq ($(DEBUG_SHADOW_PAGING),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_SHADOW_PAGING=0
endif
endif

ifeq ($(DEBUG_CTRL_REGS),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_CTRL_REGS=1
else 
ifeq ($(DEBUG_CTRL_REGS),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_CTRL_REGS=0
endif
endif

ifeq ($(DEBUG_INTERRUPTS),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_INTERRUPTS=1
else 
ifeq ($(DEBUG_INTERRUPTS),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_INTERRUPTS=0
endif
endif

ifeq ($(DEBUG_IO),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_IO=1
else 
ifeq ($(DEBUG_IO),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_IO=0
endif
endif

ifeq ($(DEBUG_KEYBOARD),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_KEYBOARD=1
else 
ifeq ($(DEBUG_KEYBOARD),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_KEYBOARD=0
endif
endif

ifeq ($(DEBUG_PIC),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_PIC=1
else 
ifeq ($(DEBUG_PIC),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_PIC=0
endif
endif

ifeq ($(DEBUG_PIT),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_PIT=1
else 
ifeq ($(DEBUG_PIT),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_PIT=0
endif
endif

ifeq ($(DEBUG_NVRAM),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_NVRAM=1
else 
ifeq ($(DEBUG_NVRAM),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_NVRAM=0
endif
endif

ifeq ($(DEBUG_GENERIC),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_GENERIC=1
else 
ifeq ($(DEBUG_GENERIC),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_GENERIC=0
endif
endif

ifeq ($(DEBUG_EMULATOR),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_EMULATOR=1
else 
ifeq ($(DEBUG_EMULATOR),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_EMULATOR=0
endif
endif

ifeq ($(DEBUG_RAMDISK),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_RAMDISK=1
else 
ifeq ($(DEBUG_RAMDISK),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_RAMDISK=0
endif
endif

ifeq ($(DEBUG_XED),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_XED=1
else 
ifeq ($(DEBUG_XED),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_XED=0
endif
endif

ifeq ($(DEBUG_HALT),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_HALT=1
else 
ifeq ($(DEBUG_HALT),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_HALT=0
endif
endif

ifeq ($(DEBUG_DEV_MGR),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_DEV_MGR=1
else 
ifeq ($(DEBUG_DEV_MGR),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_DEV_MGR=0
endif
endif

ifeq ($(DEBUG_APIC),1)
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_APIC=1
else 
ifeq ($(DEBUG_APIC),0) 
DEBUG_SECTIONS := $(DEBUG_SECTIONS) DEBUG_APIC=0
endif
endif


ifeq ($(PROFILE_VMM),1)
  GEEKOS_FLAGS:= $(GEEKOS_FLAGS) PROFILE_VMM=1
endif

ifeq ($(INSTRUMENT_VMM),1)
  GEEKOS_FLAGS:= $(GEEKOS_FLAGS) INSTRUMENT_VMM=1
endif



# ----------------------------------------------------------------------
# Targets -
#   Specifies files to be built
# ----------------------------------------------------------------------

# Default target - see definition of ALL_TARGETS in Configuration section
all : $(ALL_TARGETS)


palacios64:
	(cd $(PALACIOS_BUILD_DIR) && make ARCH=64 $(DEBUG_SECTIONS))

palacios32:
	(cd $(PALACIOS_BUILD_DIR) && make $(DEBUG_SECTIONS))


palacios-full32:
	(cd $(PALACIOS_BUILD_DIR) && make $(DEBUG_SECTIONS) world)

palacios-full64:
	(cd $(PALACIOS_BUILD_DIR) && make ARCH=64 $(DEBUG_SECTIONS) world)	

palacios-full-cray:
	(cd $(PALACIOS_BUILD_DIR) && make V=1 ARCH=64 CRAY_XT=1 $(DEBUG_SECTIONS) world)	

palacios-lean:
	(cd $(PALACIOS_BUILD_DIR) && make LEAN_AND_MEAN=1 world)



geekos: palacios32
	cp $(PALACIOS_BUILD_DIR)/libv3vee.a $(GEEKOS_BUILD_DIR)/palacios/
	cp $(PALACIOS_BUILD_DIR)/../lib/xed/libxed.a $(GEEKOS_BUILD_DIR)/palacios/
	cp $(PALACIOS_BUILD_DIR)/vm_kernel $(GEEKOS_BUILD_DIR)/palacios/
	(cd $(GEEKOS_BUILD_DIR) && make $(GEEKOS_FLAGS))


geekos-full: palacios-full32
	cp $(PALACIOS_BUILD_DIR)/libv3vee.a $(GEEKOS_BUILD_DIR)/palacios/
	cp $(PALACIOS_BUILD_DIR)/../lib/xed/libxed.a $(GEEKOS_BUILD_DIR)/palacios/
	cp $(PALACIOS_BUILD_DIR)/vm_kernel $(GEEKOS_BUILD_DIR)/palacios/
	(cd $(GEEKOS_BUILD_DIR) && make clean && make $(GEEKOS_FLAGS))


world: geekos-full palacios-full64

lean-and-mean: palacios-lean geekos-full



# make ready to boot over PXE
geekos-pxe: 
	cp $(GEEKOS_BUILD_DIR)/vmm.img /tftpboot/vmm.img

geekos-run: 
	$(QEMU) -m 1024 -serial file:serial.out -cdrom $(GUEST_ISO_DIR)/puppy.iso -fda $(GEEKOS_BUILD_DIR)/vmm.img 

geekos-iso: 
	cp $(GEEKOS_BUILD_DIR)/vmm.img iso/vmm.img
	mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -boot-info-table -o test.iso iso



force:




# Clean build directories of generated files
clean :
	for d in $(GEEKOS_BUILD_DIR) $(PALACIOS_BUILD_DIR); do \
		(cd $$d && make clean); \
	done


