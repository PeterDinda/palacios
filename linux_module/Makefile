
LDFLAGS += --whole-archive  --script=$(PWD)/link.cmd

EXTRA_CFLAGS  += -I$(PWD)/../palacios/include/ -DMODULE=1 -D__KERNEL__=1


v3vee-objs:= 	palacios.o \
		palacios-dev.o \
		palacios-vm.o \
		palacios-file.o \
		palacios-stream.o \
		palacios-console.o \
		palacios-mm.o \
		palacios-serial.o \
		palacios-queue.o \
		palacios-ringbuffer.o 

ifdef CONFIG_PALACIOS_VNET
		v3vee-objs += palacios-vnet.o		
endif
ifdef CONFIG_PALACIOS_PACKET
		v3vee-objs += palacios-packet.o palacios-hashtable.o
endif
ifdef CONFIG_PALACIOS_SOCKET
		v3vee-objs += palacios-socket.o
endif

v3vee-objs += ../libv3vee.a 



obj-m := v3vee.o



all:
	$(MAKE) -C $(CONFIG_LINUX_KERN) M=$(PWD) modules



clean:
	$(MAKE) -C $(CONFIG_LINUX_KERN) M=$(PWD) clean

