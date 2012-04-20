KDIR=/home/kch479/kyle_guest/kyle_gl
obj-m += fsceu.o
fsceu-objs := syscall_decode.o syscall.o

syscall_decode.ko: syscall_decode.c syscall_decode.h syscall.S
	make -C $(KDIR) M=$(PWD) modules

clean: 
	rm *.o *.ko
	make -C $(KDIR) M=$(PWD) clean

