include $(PWD)/../.config

LDFLAGS += --whole-archive  --script=$(PWD)/ld.cmd

ifdef V3_CONFIG_SYMMOD
LDFLAGS += --script=$(PWD)/ld.symmod.cmd
endif

EXTRA_CFLAGS  += -I$(PWD)/../palacios/include/ -include autoconf.h -DMODULE=1 -D__KERNEL__=1


v3vee-objs := 	palacios.o \
		palacios-dev.o \
		palacios-vm.o \
		palacios-mm.o \
		palacios-queue.o 

ifdef V3_CONFIG_CONSOLE
	v3vee-objs += 	palacios-console.o
endif

ifdef V3_CONFIG_FILE
	v3vee-objs += 	palacios-file.o 
endif

ifdef V3_CONFIG_STREAM
	v3vee-objs +=	palacios-stream.o \
			palacios-ringbuffer.o
endif

ifdef V3_CONFIG_EXT_INSPECTOR
	v3vee-objs += palacios-debugfs.o
endif

ifdef V3_CONFIG_VNET
	v3vee-objs += palacios-vnet.o		
endif

ifdef V3_CONFIG_PACKET
	v3vee-objs += palacios-packet.o
endif

ifdef V3_CONFIG_SOCKET
	v3vee-objs += palacios-socket.o
endif

v3vee-objs += ../libv3vee.a 



obj-m := v3vee.o



all:
	$(MAKE) -C $(V3_CONFIG_LINUX_KERN) M=$(PWD) modules



clean:
	$(MAKE) -C $(V3_CONFIG_LINUX_KERN) M=$(PWD) clean

