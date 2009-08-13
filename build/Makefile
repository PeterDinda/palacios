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
GEEKOS_BUILD_DIR := $(PROJECT_ROOT)/geekos/build
GUEST_ISO_DIR := /opt/vmm-tools/isos

# List of targets to build by default.
# These targets encompass everything needed to boot
# and run GeekOS.
ALL_TARGETS := geekos-full

QEMU := /usr/local/qemu/bin/qemu-system-x86_64


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


geekos:
	cp $(PROJECT_ROOT)/libv3vee.a $(GEEKOS_BUILD_DIR)/palacios/
	(cd $(GEEKOS_BUILD_DIR) && make $(GEEKOS_FLAGS))


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
	for d in $(GEEKOS_BUILD_DIR) do \
		(cd $$d && make clean); \
	done


