# This Makefile is a modified version that uses two recursive Make calls
# First the linux module is compiled via the target kernels build system
# Second, the module is encapsulated for the Palacios VMM

V3_MOD_NAME := test
V3_MOD_OBJ := test.ko
LINUX_MOD_OBJ := test.o

# This is the target used by the Linux build process
ifneq ($(KERNELRELEASE),)
	obj-m := $(LINUX_MOD_OBJ)
else 
# These are configuration values to be set

	KERNELDIR := /home/jarusl/linux-2.6.30.y
	PWD := $(shell pwd)
	PALACIOSDIR := $(PWD)/../../capsule
	V3_MOD_TYPE := V3_LINUX_MOD


linux: FORCE
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

capsule: FORCE 
	$(MAKE) -C $(PALACIOSDIR) MOD_NAME=$(V3_MOD_NAME) MOD_DIR=$(PWD) \
		MOD_OBJ=$(V3_MOD_OBJ) MOD_TYPE=$(V3_MOD_TYPE)

default: linux capsule	

FORCE:

endif
