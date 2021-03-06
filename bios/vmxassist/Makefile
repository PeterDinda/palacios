#
# Makefile
#
# Leendert van Doorn, leendert@watson.ibm.com
# Copyright (c) 2005, International Business Machines Corporation.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 Temple
# Place - Suite 330, Boston, MA 02111-1307 USA.
#

# External CFLAGS can do more harm than good.
CFLAGS := -m32


# The emulator code lives in ROM space
TEXTADDR=0x000D0000

DEFINES=-DDEBUG -DTEXTADDR=$(TEXTADDR)

# Disable PIE/SSP if GCC supports them. They can break us.
CFLAGS  += $(call cc-option,$(CC),-nopie,)
CFLAGS  += $(call cc-option,$(CC),-fno-stack-protector,)
CFLAGS  += $(call cc-option,$(CC),-fno-stack-protector-all,)

CPP      = cpp -P
OBJCOPY  = objcopy -p -O binary -R .note -R .comment -R .bss -S --gap-fill=0
CFLAGS  += $(DEFINES) -I. -fno-builtin -O2 -msoft-float
LDFLAGS  = -m elf_i386

OBJECTS = head.o trap.o vm86.o setup.o util.o

.PHONY: all
all: vmxassist.bin

vmxassist.bin: vmxassist.ld $(OBJECTS)
	$(CPP) $(DEFINES) vmxassist.ld > vmxassist.tmp
	$(LD) -o vmxassist $(LDFLAGS) -nostdlib --fatal-warnings -N -T vmxassist.tmp $(OBJECTS)
	nm -n vmxassist > vmxassist.sym
	$(OBJCOPY) vmxassist vmxassist.tmp
	dd if=vmxassist.tmp of=vmxassist.bin ibs=512 conv=sync
	rm -f vmxassist.tmp

head.o: machine.h vm86.h head.S
	$(CC) $(CFLAGS) -D__ASSEMBLY__ $(DEFINES) -c head.S

trap.o: machine.h vm86.h offsets.h trap.S
	$(CC) $(CFLAGS) -D__ASSEMBLY__ $(DEFINES) -c trap.S

vm86.o: machine.h vm86.h vm86.c
	$(CC) $(CFLAGS) -c vm86.c

setup.o: machine.h vm86.h setup.c
	$(CC) $(CFLAGS) -c setup.c

util.o: machine.h vm86.h util.c
	$(CC) $(CFLAGS) -c util.c

offsets.h: gen
	./gen > offsets.h

gen:	vm86.h gen.c
	$(CC) $(CFLAGS) -I. -o gen gen.c

.PHONY: clean
clean:
	rm -f vmxassist vmxassist.tmp vmxassist.bin vmxassist.run vmxassist.sym head.s
	rm -f $(OBJECTS)
	rm -f gen gen.o offsets.h

