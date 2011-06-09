include $(PWD)/../.config

LDFLAGS += --whole-archive  --script=$(PWD)/ld.cmd

ifdef V3_CONFIG_SYMMOD
LDFLAGS += --script=$(PWD)/ld.symmod.cmd
endif

EXTRA_CFLAGS  += -I$(PWD)/../palacios/include/ -include autoconf.h -DMODULE=1 -D__KERNEL__=1


v3vee-y := 	palacios.o \
		palacios-dev.o \
		palacios-vm.o \
		palacios-mm.o \
		palacios-queue.o \
	        palacios-hashtable.o \
		linux-exts.o

v3vee-$(V3_CONFIG_CONSOLE) += palacios-console.o
v3vee-$(V3_CONFIG_FILE) += palacios-file.o
v3vee-$(V3_CONFIG_STREAM) +=	palacios-stream.o \
				palacios-ringbuffer.o
v3vee-$(V3_CONFIG_EXT_INSPECTOR) += palacios-inspector.o
v3vee-$(V3_CONFIG_PACKET) += palacios-packet.o
v3vee-$(V3_CONFIG_SOCKET) += palacios-socket.o
v3vee-$(V3_CONFIG_KEYED_STREAMS) += palacios-keyed-stream.o
v3vee-$(V3_CONFIG_HOST_DEVICE) += palacios-host-dev.o
v3vee-$(V3_CONFIG_GRAPHICS_CONSOLE) += palacios-graphics-console.o

v3vee-$(V3_CONFIG_VNET) += 	palacios-vnet.o \
				palacios-vnet-ctrl.o   \
				palacios-vnet-brg.o





v3vee-objs := $(v3vee-y) ../libv3vee.a

obj-m := v3vee.o



all:
	$(MAKE) -C $(V3_CONFIG_LINUX_KERN) M=$(PWD) modules



clean:
	$(MAKE) -C $(V3_CONFIG_LINUX_KERN) M=$(PWD) clean

