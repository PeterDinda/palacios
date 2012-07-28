
CC = gcc
INC = -I../../linux_usr
CFLAGS = -Wall -g  $(INC)
LDFLAGS = 


OBJS = main.c ezxml.c ../../linux_usr/libv3_ctrl.a

all: builder


builder: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -lv3_ctrl -L../../linux_usr -o build_vm

clean: 	
	rm -f *.o build_vm
