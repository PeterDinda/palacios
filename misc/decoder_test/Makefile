PREFIX=./


INCLUDEDIR=$(PREFIX)XED2/include
LIBDIR=$(PREFIX)XED2/lib/
BINDIR=$(PREFIX)/bin


TEST_OBJS =  vm_guest.o vmm_xed.o test.o



CFLAGS =  -I$(INCLUDEDIR) -D__V3VEE__ -g -gstabs -D__DECODER_TEST__



#CC=../../devtools/i386/bin/i386-elf-gcc
CC=/usr/bin/gcc -m32


#CXXFLAGS =  -DDEBUG  -g -gstabs+ -Wall  -I/usr/kerberos/include

LDFLAGS  =  -L$(LIBDIR) $(CFLAGS) -lxed

all:	test

test: $(TEST_OBJS)
	$(CC) $(CFLAGS) $(TEST_OBJS) $(LDFLAGS) -o xed_test




%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $*.o

depend:
	$(CC) $(CFLAGS) -MM $(TEST_OBJS:.o=.c) > .dependencies

clean: 
	rm -f *.o
	rm -f $(TEST_OBJS) xed_test


include .dependencies



