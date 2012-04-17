#
# Build Static or Dynamic versions of the control utilities
#
#

# define this at the command-line or here
# to get dynamically linked versions
STATIC = 1

#
# Executables that implement core functionality
#
BASE_EXECS = 	v3_mem \
                v3_mem_free \
		v3_create \
                v3_create_bind \
		v3_free  \
	  	v3_launch \
		v3_stop   \
		v3_pause  \
		v3_continue \
		v3_core_move \
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
		v3_pci


#
# Examples
#
EXAMPLE_EXECS = v3_user_host_dev_example \
		v3_os_debug \
		v3_user_keyed_stream_example \
		v3_user_keyed_stream_file 

#
# Currently experimental things
#
EXPERIMENTAL_EXECS = 	v3_simulate  \
			v3_inject_ecc_scrubber_mce  \
                        v3_top_inject \
                        v3_env_inject \
			v3_syscall




#
# Things that have been built elsewhere - just for reference here
#
PRECOMPILED_EXECS = x0vncserver

COPIED_EXECS = v3_x0vncserver

#
# Libraries that we need to build
#
LIBS = libv3_user_host_dev.a libv3_user_keyed_stream.a

BUILD_EXECS = $(BASE_EXECS) $(EXAMPLE_EXECS) $(EXPERIMENTAL_EXECS) 
BUILD_LIBS = $(LIBS)


CFILES = $(BUILD_EXECS:%=%.c) $(BUILD_LIBS:lib%.a=%.c) 

ifeq ($(STATIC),1)
  CFLAGS += -static 
endif

CFLAGS += -I../linux_module

CC = gcc
AR = ar

all: $(BUILD_EXECS) $(BUILD_LIBS) $(COPIED_EXECS)


#
# Most tools compile in a straightforward way
#
% :: %.c v3_ctrl.h
	$(CC) $(CFLAGS) $< -o $@


#
# Console support requires curses
#
ifeq ($(STATIC),1)
  CURSES_CFLAGS = -DNCURSES_STATIC
endif

v3_cons: v3_cons.c
	$(CC) $(CFLAGS) $(CURSES_CFLAGS) $< -lcurses -o $@

v3_cons_sc: v3_cons_sc.c
	$(CC) $(CFLAGS) $(CURSES_CFLAGS) $< -lcurses -o $@



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



#
# VNC support is compiled separately
#
v3_x0vncserver : x0vncserver
	cp x0vncserver v3_x0vncserver


#
# Cleanup and dependency builds are automated
#
clean:
	rm -f $(BUILD_EXECS) $(BUILD_LIBS) $(COPIED_EXECS)

depend:
	$(CC) -MM $(CFILES) -I../linux_module > .dependencies

