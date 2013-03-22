# Generated automatically from Makefile.in:boilerplate.mk by configure.

V3FB_DIR = ../../palacios/linux_usr

SRCS = Image.cxx x0vncserver.cxx

OBJS = $(SRCS:.cxx=.o)  $(CSRCS:.c=.o)

program = x0vncserver

DEP_LIBS = ../rfb/librfb.a ../network/libnetwork.a ../rdr/librdr.a

EXTRA_LIBS = $(top_srcdir)/zlib/libz.a  -L/usr/lib64 $(top_srcdir)/Xregion/libXregion.a $(top_srcdir)/xlib/libX11.a $(top_srcdir)/xlib/libSM.a $(top_srcdir)/xlib/libICE.a $(top_srcdir)/xlib/libXtst.a $(top_srcdir)/xlib/libXext.a -L$(V3FB_DIR) -lv3_fb # -lX11 -lSM -lICE  -lXtst -lXext 

DIR_CPPFLAGS = -I$(top_srcdir)  -I$(V3FB_DIR) # X_CFLAGS are really CPPFLAGS

all:: $(program)

$(program): $(OBJS) buildtime.o $(DEP_LIBS)
	rm -f $(program)
	$(CXXLD) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) buildtime.o $(DEP_LIBS) $(LIBS) $(EXTRA_LIBS)

buildtime.o: $(OBJS) $(DEP_LIBS)

# followed by boilerplate.mk

all::
	@subdirs="$(SUBDIRS)"; for d in $$subdirs; do (cd $$d; $(MAKE) $@) || exit 1; done

clean::
	@subdirs="$(SUBDIRS)"; for d in $$subdirs; do (cd $$d; $(MAKE) $@) || exit 1; done

clean::
	rm -f $(program) $(library) *.o

SHELL = /bin/sh
top_srcdir = ..

CC = gcc
CFLAGS = -g $(DIR_CFLAGS) #-O2 -Wall 
CCLD = $(CC)
CXX = c++
CXXFLAGS = -g #-O2 -Wall
CXXLD = $(CXX)
CPPFLAGS = 
DEFS =  
ALL_CPPFLAGS = $(CPPFLAGS) $(DEFS) $(DIR_CPPFLAGS)
LIBS = 
LDFLAGS = --whole-archive --static
RANLIB = ranlib
AR = ar cq

.SUFFIXES:
.SUFFIXES: .cxx .c .o

.c.o:
	$(CC) $(ALL_CPPFLAGS) $(CFLAGS) -c $<

.cxx.o:
	$(CXX) $(ALL_CPPFLAGS) $(CXXFLAGS) -c $<
