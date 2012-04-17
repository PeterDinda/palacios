include $(PWD)/../.config

LDFLAGS += --whole-archive  --script=$(PWD)/ld.cmd

ifdef V3_CONFIG_SYMMOD
LDFLAGS += --script=$(PWD)/ld.symmod.cmd
endif

ifdef V3_CONFIG_CHECKPOINT
LDFLAGS += --script=$(PWD)/ld.chkpt.cmd
endif

EXTRA_CFLAGS  += -I$(PWD)/../palacios/include/ -include autoconf.h -DMODULE=1 -D__KERNEL__=1


v3vee-y := 	palacios-stubs.o \
		main.o \
		vm.o \
		mm.o \
		util-queue.o \
	        util-hashtable.o \
		linux-exts.o

v3vee-$(V3_CONFIG_CONSOLE) += iface-console.o
v3vee-$(V3_CONFIG_FILE) += iface-file.o
v3vee-$(V3_CONFIG_STREAM) +=	iface-stream.o \
				util-ringbuffer.o
v3vee-$(V3_CONFIG_EXT_INSPECTOR) += inspector.o
v3vee-$(V3_CONFIG_PACKET) += iface-packet.o
v3vee-$(V3_CONFIG_SOCKET) += iface-socket.o
v3vee-$(V3_CONFIG_KEYED_STREAMS) += iface-keyed-stream.o
v3vee-$(V3_CONFIG_HOST_DEVICE) += iface-host-dev.o
v3vee-$(V3_CONFIG_GRAPHICS_CONSOLE) += iface-graphics-console.o
v3vee-$(V3_CONFIG_EXT_MACH_CHECK) += mcheck.o

v3vee-$(V3_CONFIG_VNET) += 	palacios-vnet.o \
				palacios-vnet-ctrl.o   \
				palacios-vnet-brg.o

v3vee-$(V3_CONFIG_HOST_HYPERCALL) += iface-host-hypercall.o
v3vee-$(V3_CONFIG_EXT_CODE_INJECT) += iface-code-inject.o
v3vee-$(V3_CONFIG_EXT_ENV_INJECT) += iface-env-inject.o
v3vee-$(V3_CONFIG_EXT_SELECTIVE_SYSCALL_EXIT) += iface-syscall.o


v3vee-$(V3_CONFIG_HOST_PCI) += iface-host-pci.o

v3vee-objs := $(v3vee-y) ../libv3vee.a
obj-m := v3vee.o


all:
	$(MAKE) -C $(V3_CONFIG_LINUX_KERN) M=$(PWD) modules

clean:
	$(MAKE) -C $(V3_CONFIG_LINUX_KERN) M=$(PWD) clean

