#
# Build Static or Dynamic versions of the control utilities
#
#

# define this at the command-line or here
# to get dynamically linked versions
STATIC = 0

#
# Executables that implement core functionality
#
BASE_EXECS = 	v3_mem \
                v3_mem_reset \
		v3_create \
                v3_create_bind \
		v3_free  \
	  	v3_launch \
	        v3_reset  \
		v3_stop   \
		v3_pause  \
		v3_continue \
		v3_core_move \
                v3_mem_move \
		v3_load \
		v3_save \
		v3_cons \
		v3_cons_sc \
		v3_stream \
		v3_monitor \
                v3_hypercall \
		v3_debug \
		v3_send \
		v3_receive \
		v3_pci \
		v3_guest_mem_access \
		v3_guest_mem_track \
                v3_dvfs


#
# Examples
#
EXAMPLE_EXECS = v3_user_host_dev_example \
		v3_os_debug \
		v3_user_keyed_stream_example \
		v3_user_keyed_stream_file \

#
# Currently experimental things
#
EXPERIMENTAL_EXECS = 	v3_simulate  \
			v3_inject_ecc_scrubber_mce  \
                        v3_top_inject \
                        v3_env_inject \
			v3_syscall \
			v3_register_gm \
			v3_devfile_shadow




#
# Things that have been built elsewhere - just for reference here
#
PRECOMPILED_EXECS = x0vncserver

COPIED_EXECS = v3_x0vncserver v3_x0gui

#
# Libraries that we need to build
#
LIBS = libv3_user_host_dev.a libv3_user_keyed_stream.a libv3_ctrl.a libv3_guest_mem.a libv3_fb.a libv3_user_dvfs.a libv3_io_chan.so

BUILD_EXECS = $(BASE_EXECS) $(EXAMPLE_EXECS) $(EXPERIMENTAL_EXECS) 
BUILD_LIBS = $(LIBS)


CFILES = $(BUILD_EXECS:%=%.c) $(BUILD_LIBS:lib%.a=%.c) 

ifeq ($(STATIC),1)
  CFLAGS += -static 
endif

CFLAGS += -I../linux_module -I../palacios/include/

CC = gcc -g
AR = ar

all: $(BUILD_EXECS) $(BUILD_LIBS) $(COPIED_EXECS)


libv3_ctrl.a : v3_ctrl.c v3_ctrl.h
	$(CC) $(CFLAGS) -I../linux_module -c v3_ctrl.c
	$(AR) ruv libv3_ctrl.a v3_ctrl.o
	rm -rf v3_ctrl.o 

libv3_io_chan.so: v3_io_chan.c v3_io_chan.h v3_user_host_dev.c v3_user_host_dev.h ../linux_module/iface-host-dev.h 
	$(CC) $(CFLAGS) -I../linux_module -fPIC -c v3_io_chan.c
	$(CC) $(CFLAGS) -I../linux_module -fPIC -c v3_user_host_dev.c
	$(CC) -shared -o $@ v3_io_chan.o v3_user_host_dev.o
	cp $@ /usr/lib64/
	cp v3_io_chan.h /usr/include/
#
# JSON library for use in some tools
#
libjson.a : cJSON.c cJSON.h
	$(CC) $(CFLAGS) -c cJSON.c
	$(AR) ruv libjson.a cJSON.o
	rm -rf cJSON.o
#
# Most tools compile in a straightforward way
#
% :: %.c v3_ctrl.h libv3_ctrl.a
	$(CC) $(CFLAGS) $< -lv3_ctrl -L. -o $@


#
# Console support requires curses
#
ifeq ($(STATIC),1)
  CURSES_CFLAGS = -DNCURSES_STATIC
  CURSES_LIBS = -lcurses                           
else
  CURSES_CFLAGS = 
  CURSES_LIBS = -lcurses 
endif

v3_cons: v3_cons.c
	$(CC) $(CFLAGS) $(CURSES_CFLAGS) $< $(CURSES_LIBS) -o $@

v3_cons_sc: v3_cons_sc.c
	$(CC) $(CFLAGS) $(CURSES_CFLAGS) $< $(CURSES_LIBS) -o $@

v3_create: v3_create.c ezxml.c libv3_ctrl.a 
	$(CC) $(CFLAGS) $^ -lv3_ctrl -L. -o $@



#
# User host devices include a library and some examples
#
libv3_user_host_dev.a : v3_user_host_dev.c v3_user_host_dev.h v3_ctrl.h
	$(CC) $(CFLAGS) -I../linux_module -c v3_user_host_dev.c 
	$(AR) ruv libv3_user_host_dev.a v3_user_host_dev.o
	rm -f v3_user_host_dev.o

v3_user_host_dev_example : v3_user_host_dev_example.c libv3_user_host_dev.a
	$(CC) $(CFLAGS) $< -I../linux_module -L. -lv3_user_host_dev -o $@

v3_os_debug : v3_os_debug.c libv3_user_host_dev.a
	$(CC) $(CFLAGS) $< -I../linux_module -L. -lv3_user_host_dev -o $@

#
# Guarded module registration for GEARS
#
v3_register_gm: v3_register_gm.c libjson.a libv3_ctrl.a v3_ctrl.h
	$(CC) $(CFLAGS) $< -L. -lm -lv3_ctrl -ljson -o $@

#
# User keyed stream support includes a library and examples
#
#
libv3_user_keyed_stream.a :: v3_user_keyed_stream.c v3_user_keyed_stream.h v3_ctrl.h
	$(CC) $(CFLAGS) -I../linux_module -c v3_user_keyed_stream.c 
	$(AR) ruv libv3_user_keyed_stream.a v3_user_keyed_stream.o
	rm -f v3_user_keyed_stream.o


v3_user_keyed_stream_example : v3_user_keyed_stream_example.c libv3_user_keyed_stream.a
	$(CC) $(CFLAGS) $< -I../linux_module -L. -lv3_user_keyed_stream -o $@

v3_user_keyed_stream_file : v3_user_keyed_stream_file.c libv3_user_keyed_stream.a
	$(CC) $(CFLAGS) $< -I../linux_module -L. -lv3_user_keyed_stream -o $@


# guest memory access by mmap 
# guest memory reference tracking via mem_track interface
# examples
#
libv3_guest_mem.a : v3_guest_mem.c v3_guest_mem.h v3_ctrl.h
	$(CC) $(CFLAGS) -c v3_guest_mem.c 
	$(AR) ruv libv3_guest_mem.a v3_guest_mem.o
	rm -f v3_guest_mem.o

v3_guest_mem_access : v3_guest_mem_access.c libv3_guest_mem.a libv3_ctrl.a
	$(CC) $(CFLAGS) $< -L. -lv3_guest_mem -lv3_ctrl -o $@

v3_guest_mem_track : v3_guest_mem_track.c libv3_guest_mem.a libv3_ctrl.a
	$(CC) $(CFLAGS) $< -L. -lv3_guest_mem -lv3_ctrl -o $@


# Use this for shadow process
v3_devfile_shadow : v3_devfile_shadow.c libv3_guest_mem.a libv3_ctrl.a
	$(CC) $(CFLAGS) $< -L. -lv3_guest_mem -lv3_ctrl -o $@

#
# User DVFS control includse a library and a caller
#
libv3_user_dvfs.a : v3_user_dvfs.c v3_user_dvfs.h v3_ctrl.h
	$(CC) $(CFLAGS) -I../linux_module -c v3_user_dvfs.c 
	$(AR) ruv libv3_user_dvfs.a v3_user_dvfs.o
	rm -f v3_user_dvfs.o

v3_dvfs : v3_dvfs.c libv3_user_dvfs.a libv3_ctrl.a
	$(CC) $(CFLAGS) $< -I../linux_module -L. -lv3_user_dvfs -lv3_ctrl -o $@


#
# FB (Graphics Console) user interface library (needed for VNC access
#  to GC - also wraps mouse and keyboard event delivery)
libv3_fb.a : v3_fb.c v3_fb.h
	$(CC) $(CFLAGS) -I../linux_module -c v3_fb.c
	$(AR) ruv libv3_fb.a v3_fb.o
	rm -rf v3_fb.o 



#
# VNC support is compiled separately
#
v3_x0vncserver : 
	@if [ -a x0vncserver ] ; \
	then \
		cp x0vncserver v3_x0vncserver ; \
	else \
		echo "In order to use v3_vncclient/server you must have" ; \
		echo "previously built or received palacios/linux_usr/x0vncserver" ; \
		echo "To learn more about this, look in palacios/linux_usr/vnc"; \
	fi;

#
# GUI is compiled separately
#
v3_x0gui : 
	@if [ -a gui/Palacios ] ; \
	then \
		cp gui/Palacios v3_x0gui ; \
	else \
		echo "In order to use v3_gui (the graphical interface)" ;\
		echo "you must first cd to palacios/linux_usr/gui" ;\
		echo "and compile it according to the README given there";\
	fi;

#
# Cleanup and dependency builds are automated
#
clean:
	rm -f $(BUILD_EXECS) $(BUILD_LIBS) $(COPIED_EXECS)

depend:
	$(CC) -MM $(CFILES) -I../linux_module > .dependencies

