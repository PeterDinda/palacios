#
# Copyright (c) 2001, 2002 Swedish Institute of Computer Science.
# All rights reserved. 
# 
# Redistribution and use in source and binary forms, with or without modification, 
# are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
# SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
# OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
# OF SUCH DAMAGE.
#
# This file is part of the lwIP TCP/IP stack.
# 
# Author: Adam Dunkels <adam@sics.se>
#

## modified by Lei Xia 

PROJECT_ROOT=../../../

#Set this to where you have the lwip core module
LWIPDIR=..

CC_PREFIX :=  $(PROJECT_ROOT)../devtools/i386/bin/i386-elf-
CC=$(CC_PREFIX)gcc
CFLAGS=-g -Wall -fpic -DIPv4 -DLWIP_DEBUG
ARCHIVE=ar

CFLAGS:=$(CFLAGS) \
	-I$(PROJECT_ROOT)include/ -I$(PROJECT_ROOT)include/lwip  -I$(PROJECT_ROOT)include/lwip/ipv4 -I$(PROJECT_ROOT)include/libc 

# COREFILES, CORE4FILES: The minimum set of files needed for lwIP.
COREFILES=$(LWIPDIR)/core/mem.c $(LWIPDIR)/core/memp.c $(LWIPDIR)/core/netif.c \
	$(LWIPDIR)/core/pbuf.c $(LWIPDIR)/core/stats.c $(LWIPDIR)/core/sys.c \
        $(LWIPDIR)/core/tcp.c $(LWIPDIR)/core/tcp_in.c $(LWIPDIR)/core/raw.c\
        $(LWIPDIR)/core/tcp_out.c $(LWIPDIR)/core/udp.c $(LWIPDIR)/core/init.c 
CORE4FILES=$(LWIPDIR)/core/ipv4/icmp.c $(LWIPDIR)/core/ipv4/ip.c \
	$(LWIPDIR)/core/ipv4/inet.c $(LWIPDIR)/core/ipv4/ip_addr.c \
	$(LWIPDIR)/core/ipv4/inet_chksum.c $(LWIPDIR)/core/ipv4/ip_frag.c


# APIFILES: The files which implement the sequential and socket APIs.
APIFILES=$(LWIPDIR)/api/api_lib.c $(LWIPDIR)/api/api_msg.c $(LWIPDIR)/api/tcpip.c \
	$(LWIPDIR)/api/err.c $(LWIPDIR)/api/sockets.c $(LWIPDIR)/api/netbuf.c $(LWIPDIR)/api/netdb.c  $(LWIPDIR)/api/netifapi.c

# NETIFFILES: Files implementing various generic network interface functions.'
NETIFFILES=$(LWIPDIR)/netif/loopif.c $(LWIPDIR)/netif/ne2kif.c $(LWIPDIR)/netif/etharp.c

# ARCHFILES: Architecture specific files.
ARCHFILES=$(LWIPDIR)/arch/sys_arch.c

# APPFILES: Application files
APPFILES=$(LWIPDIR)/apps/ping.c

# LWIPFILES: All the above.
LWIPFILES=$(COREFILES) $(CORE4FILES) $(APIFILES) $(NETIFFILES) $(ARCHFILES) $(APPFILES)
LWIPFILESW=$(wildcard $(LWIPFILES))
LWIPOBJS=$(notdir $(LWIPFILESW:.c=.o))

# LWIPLIB=lwip.o

%.o:
	$(CC) $(CFLAGS) -c  $(LWIPFILES)

all: $(LWIPOBJS)
	cp *.o $(PROJECT_ROOT)build/lwip/

.PHONY: all

clean:
	rm -f *.o .depend*

depend dep: .depend

include .depend

# $(LWIPLIB): $(LWIPOBJS)
#	$(CC) -g -nostartfiles -shared -static $^ -o $@ 

.depend: $(LWIPFILES)
	$(CC) $(CFLAGS) -MM $^ > .depend || rm -f .depend
