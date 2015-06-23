.PHONY: clear	
EXTRA = -m64

#TEST_CASES := $(wildcard test*.c)

#TEST_TARGETS := $(wildcard (test*).c)

all: devfile_hc.o test_preload devfile_host.ko libdevfile_hcall.a devfile_preload.so

libdevfile_hcall.a: devfile_hc.o
	ar ruv libdevfile_hcall.a devfile_hc.o

devfile_hc.o: devfile_hc.c devfile_hc.h hcall.h
	gcc  $(EXTRA)  -g -static -fPIC -S devfile_hc.c -o devfile_hc.s
	gcc  $(EXTRA)  -g -static -fPIC -c devfile_hc.c -o devfile_hc.o

devfile_preload.so: devfile_preload.c libdevfile_hcall.a
	gcc   $(EXTRA) -Wall -O2 -fPIC -shared -I/usr/include devfile_preload.c -L. -ldevfile_hcall -ldl -lc -o devfile_preload.so

EXTRA_CFLAGS += -I$(PWD)/../../../palacios/include

obj-m += devfile_host.o 

devfile_host.ko: devfile_host.c 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

test_preload: test_preload.c
	gcc $(EXTRA) -Wall -g test_preload.c -o test_preload

clean: 
	rm *.o *.so *.a *.ko test_preload *.out *.symvers *.mod.c *.s *.order 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

clear:
	rm test_ld_lib *.o *.mod.o *.out *.symvers *.mod.c *.s *.order 
