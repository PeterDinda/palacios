EXTRA = -m32


all: libmpi_hcall.a mpi_preload.so mpi.ko test_static

libmpi_hcall.a: mpi_hc.o
	ar ruv libmpi_hcall.a mpi_hc.o

mpi_hc.o: mpi_hc.c mpi_hc.h hcall.h
	gcc   $(EXTRA)  -static -fPIC -S mpi_hc.c -o mpi_hc.s
	gcc  $(EXTRA)  -static -fPIC -c mpi_hc.c -o mpi_hc.o

mpi_preload.so: mpi_preload.c libmpi_hcall.a
	gcc   $(EXTRA) -Wall -O2 -fPIC -shared -nostdlib -I/usr/include mpi_preload.c -L. -lmpi_hcall -ldl -lc -o mpi_preload.so

test_static: test_static.c libmpi_hcall.a
	gcc   $(EXTRA) -static test_static.c -L. -lmpi_hcall -o test_static


EXTRA_CFLAGS += -I$(PWD)/../../../palacios/include

obj-m += mpi.o 

mpi.ko: mpi.c 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean: 
	rm *.o *.so *.a test_static 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

