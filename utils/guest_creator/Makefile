
CC = gcc
CFLAGS = -Wall -g 
LDFLAGS = 


OBJS = main.c ezxml.c

all: builder


builder: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o build_vm

clean: 	
	rm -f *.o build_vm