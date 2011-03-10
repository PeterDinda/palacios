KERNELDIR  := ../../linux-2.6.32.y/
CONFIG_PALACIOS_PATH := /home/jarusl/palacios




v3vee-objs := palacios.o palacios-dev.o palacios-vm.o palacios-file.o palacios-stream.o palacios-console.o palacios-mm.o palacios-serial.o palacios-queue.o palacios-ringbuffer.o 
#palacios-socket.o 
#palacios-vnet.o palacios-packet.o 

v3vee-objs += ../../palacios/libv3vee.a 


obj-m := v3vee.o


LDFLAGS = --whole-archive  --script=$(PWD)/link.cmd


EXTRA_CFLAGS := -I$(CONFIG_PALACIOS_PATH)/palacios/include/ -DMODULE=1 -D__KERNEL__=1



all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules


clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install /sbin/ -ae

