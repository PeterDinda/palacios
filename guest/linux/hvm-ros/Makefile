CC:=gcc
AR:=ar
ARFLAGS:=ruv

CFLAGS:= -Wall -static

ifeq ($(DEBUG),1)
	CFLAGS += -DDEBUG_ENABLE=1
endif

ifeq ($(INFO),1)
	CFLAGS += -DINFO_ENABLE=1
endif

all: libv3_hvm_ros_user.a test

libv3_hvm_ros_user.a: v3_hvm_ros_user.o v3_hvm_ros_user_low_level.o
	$(AR) $(ARFLAGS) libv3_hvm_ros_user.a v3_hvm_ros_user.o v3_hvm_ros_user_low_level.o

v3_hvm_ros_user.o: v3_hvm_ros_user.c v3_hvm_ros_user.h 
	$(CC) $(CFLAGS) -c v3_hvm_ros_user.c

v3_hvm_ros_user_low_level.o: v3_hvm_ros_user_low_level.S
	$(CC) $(CFLAGS) -c v3_hvm_ros_user_low_level.S

test: test.c libv3_hvm_ros_user.a v3_hvm_ros_user.h
	$(CC) $(CFLAGS) -static test.c -L. -lv3_hvm_ros_user -o test

clean:
	rm -f libv3_hvm_ros_user.a v3_hvm_ros_user.o v3_hvm_ros_user_low_level.o test
