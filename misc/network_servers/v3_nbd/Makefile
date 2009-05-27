PREFIX=..

INCLUDEDIR=$(PREFIX)/include
LIBDIR=$(PREFIX)/lib
BINDIR=$(PREFIX)/bin

NBD_OBJS = v3_nbd.o v3_disk.o raw.o iso.o

CFLAGS = -I$(INCLUDEDIR)
VTLFLAGS = -L$(LIBDIR) -Bstatic -lvtl -lnet -Bdynamic -lpcap -lssl

CXX=g++
CC=/usr/bin/gcc 
AR=ar
RANLAB=ranlib

CXXFLAGS = $(DEBUG) -g -gstabs+ -Wall $(CFLAGS) -I/usr/kerberos/include
LDFLAGS  = $(VTLFLAGS) 

all:	v3_nbd

v3_nbd: $(NBD_OBJS)
	$(CXX) $(CXXFLAGS) $(NBD_OBJS) $(LDFLAGS) -o v3_nbd

%.o : %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $(@F)

depend:
	$(CXX) $(CXXFLAGS) -MM $(NBD_OBJS:.o=.cc) > .dependencies

clean: 
	rm -f $(NBD_OBJS)
	rm -f v3_nbd
	rm -f *.log


include .dependencies



