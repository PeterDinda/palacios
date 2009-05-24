DISTDIR= ../dist
PREFIX=../local
DEBUG=-DDEBUG
#DEBUG=
#PROFILER=-pg
PROFILER=

INCLUDEDIR=$(PREFIX)/include
LIBDIR=$(PREFIX)/lib
BINDIR=$(PREFIX)/bin

VTL_OBJS = vtl_util.o  if.o socks.o util.o net_util.o raw_ethernet_packet.o vtl_model.o config.o
TOR_VTL_OBJS = vtl_socks5.o tor_vtl.o vtl_dns.o

VTL_HDRS = vtl.h vtl_util.h util.h socks.h if.h net_util.h vtl_model.h raw_ethernet_packet.h debug.h config.h vtl_harness.h

DISTFILES = config.cc config.h debug.h if.cc if.h net_util.cc net_util.h raw_ethernet_packet.cc raw_ethernet_packet.h socks.cc socks.h util.cc util.h vtl.h vtl_harness.h vtl_model.cc vtl_model.h vtl_util.cc vtl_util.h Makefile .dependencies

LIBNETLDFLAGS = -L$(LIBDIR) -lnet
SSLFLAGS	= -lssl
#LIBNETLDFLAGS = 
#SSLFLAGS = 	
PCAPCFLAGS  = -I$(INCLUDEDIR)
PCAPLDFLAGS = -L$(LIBDIR) -lpcap


CXX=g++
CC=/usr/bin/gcc 
AR=ar
RANLIB=ranlib

#CXXFLAGS =  -DDEBUG  -g -gstabs+ -Wall $(PCAPCFLAGS) -I/usr/kerberos/include
CXXFLAGS =   $(DEBUG) -Wall -g -gstabs+ -DUSE_SSL $(PCAPCFLAGS) -I/usr/kerberos/include
LDFLAGS  =  -L$(LIBDIR) $(PCAPLDFLAGS) $(LIBNETLDFLAGS) $(SSLFLAGS) 


all: vtl

#vtl_test: $(VTL_OBJS) vtl_test.o
#	$(CXX) $(CXXFLAGS)  $(VTL_OBJS) vtl_test.o $(LDFLAGS) -o vtl_test

#vtl_ack_test: vtl_ack_test.o $(VTL_OBJS) libvtl.a
#	$(CXX)  $(CXXFLAGS) -L. vtl_ack_test.o -Bstatic -lvtl -Bdynamic $(LDFLAGS) -o vtl_ack_test


vtl: $(VTL_OBJS)
	$(AR) rcs libvtl.a $(VTL_OBJS) 


%.o : %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $(@F) $(PROFILER)

depend:
	$(CXX) $(CXXFLAGS) -MM $(VNET_OBJS:.o=.cc) > .dependencies

clean: 
	rm -f *.o
	rm -f $(VTL_OBJS) libvtl.a
#	rm -f vtl_ack_test

install: vtl
	mkdir -p $(BINDIR)
	mkdir -p $(LIBDIR)
	mkdir -p $(INCLUDEDIR)
	cp  libvtl.a $(LIBDIR)/libvtl.a
	cp  $(VTL_HDRS) $(INCLUDEDIR)/


dist: $(DISTFILES)
	mkdir -p $(DISTDIR)/vtl
	cp $(DISTFILES) $(DISTDIR)/vtl

include .dependencies



