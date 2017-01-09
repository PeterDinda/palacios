include $(PWD)/../.config

NAUTILUS_DIR = $(subst ",,$(V3_CONFIG_NAUTILUS_KERN))

include $(NAUTILUS_DIR)/.config

CC = gcc
AR = ar


PALACIOS_CFLAGS = -I$(PWD)/../palacios/include -include $(PWD)/../palacios/include/autoconf.h

NAUTILUS_CFLAGS = -D__NAUTILUS__ -I$(NAUTILUS_DIR)/include -include $(NAUTILUS_DIR)/include/autoconf.h

NAUTILUS_CFLAGS +=  	-O2 \
                        -fno-omit-frame-pointer \
                        -ffreestanding \
                        -fno-stack-protector \
                        -fno-strict-aliasing \
                        -fwrapv \
                        -mno-red-zone \
                        -mcmodel=large \
                        -Wall \
                        -Wno-unused-function \
                        -Wno-unused-variable \
                        -fno-common \
                        -std=gnu99 \
                        $(call cc-option, -Wno-unused-but-set-variable,)


CFLAGS = -I. $(PALACIOS_CFLAGS) $(NAUTILUS_CFLAGS)

LDFLAGS := --whole-archive -z max-page-size=0x1000

OBJS = palacios-stubs.o  

ifdef V3_CONFIG_CONSOLE
	OBJS += console.o
endif

libnautilus.a : $(OBJS) 
	$(AR) ruv libnautilus.a $(OBJS)

%.o : %.c 
	$(CC) $(CFLAGS) -c $< -o $@


all: libnautilus.a

clean:
	rm -f *.o *.a


