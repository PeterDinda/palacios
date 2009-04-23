geekos/idt.o: ../src/geekos/idt.c ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/defs.h ../include/geekos/idt.h \
  ../include/geekos/int.h ../include/geekos/debug.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h
geekos/int.o: ../src/geekos/int.c ../include/geekos/idt.h \
  ../include/geekos/int.h ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/defs.h ../include/geekos/irq.h \
  ../include/geekos/debug.h ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/cpu.h
geekos/trap.o: ../src/geekos/trap.c ../include/geekos/idt.h \
  ../include/geekos/int.h ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/defs.h ../include/geekos/kthread.h \
  ../include/geekos/list.h ../include/geekos/trap.h \
  ../include/geekos/debug.h ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h
geekos/irq.o: ../src/geekos/irq.c ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/idt.h ../include/geekos/int.h \
  ../include/geekos/defs.h ../include/geekos/io.h ../include/geekos/irq.h
geekos/io.o: ../src/geekos/io.c ../include/geekos/io.h \
  ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h
geekos/blockdev.o: ../src/geekos/blockdev.c ../include/geekos/errno.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/malloc.h ../include/geekos/int.h \
  ../include/geekos/kassert.h ../include/geekos/defs.h \
  ../include/geekos/kthread.h ../include/geekos/list.h \
  ../include/geekos/synch.h ../include/geekos/blockdev.h \
  ../include/geekos/fileio.h
geekos/ide.o: ../src/geekos/ide.c ../include/geekos/serial.h \
  ../include/geekos/irq.h ../include/geekos/int.h \
  ../include/geekos/kassert.h ../include/geekos/screen.h \
  ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/defs.h ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/io.h ../include/geekos/errno.h \
  ../include/geekos/malloc.h ../include/geekos/timer.h \
  ../include/geekos/kthread.h ../include/geekos/list.h \
  ../include/geekos/blockdev.h ../include/geekos/fileio.h \
  ../include/geekos/ide.h
geekos/keyboard.o: ../src/geekos/keyboard.c ../include/geekos/kthread.h \
  ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/list.h ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/fmtout.h \
  ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/irq.h ../include/geekos/int.h \
  ../include/geekos/defs.h ../include/geekos/io.h \
  ../include/geekos/keyboard.h
geekos/screen.o: ../src/geekos/screen.c \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/kassert.h ../include/geekos/screen.h \
  ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  ../include/geekos/io.h ../include/geekos/int.h ../include/geekos/defs.h
geekos/timer.o: ../src/geekos/timer.c \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/limits.h \
  ../include/geekos/io.h ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/int.h ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/fmtout.h \
  ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/defs.h ../include/geekos/irq.h \
  ../include/geekos/kthread.h ../include/geekos/list.h \
  ../include/geekos/timer.h ../include/geekos/debug.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h
geekos/mem.o: ../src/geekos/mem.c ../include/geekos/defs.h \
  ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/kassert.h ../include/geekos/screen.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/bootinfo.h ../include/geekos/gdt.h \
  ../include/geekos/int.h ../include/geekos/malloc.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/paging.h ../include/geekos/list.h \
  ../include/geekos/mem.h
geekos/crc32.o: ../src/geekos/crc32.c ../include/geekos/crc32.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/kassert.h ../include/geekos/screen.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/serial.h ../include/geekos/irq.h \
  ../include/geekos/int.h ../include/geekos/defs.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  ../include/geekos/io.h
geekos/gdt.o: ../src/geekos/gdt.c ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/segment.h ../include/geekos/int.h \
  ../include/geekos/defs.h ../include/geekos/tss.h \
  ../include/geekos/gdt.h ../include/libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h
geekos/tss.o: ../src/geekos/tss.c ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/defs.h ../include/geekos/gdt.h \
  ../include/geekos/segment.h ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/tss.h ../include/geekos/serial.h \
  ../include/geekos/irq.h ../include/geekos/int.h ../include/geekos/io.h
geekos/segment.o: ../src/geekos/segment.c ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/tss.h ../include/geekos/segment.h
geekos/bget.o: ../src/geekos/bget.c ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/kassert.h ../include/geekos/screen.h \
  ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/bget.h
geekos/malloc.o: ../src/geekos/malloc.c ../include/geekos/screen.h \
  ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/int.h ../include/geekos/kassert.h \
  ../include/geekos/defs.h ../include/geekos/bget.h \
  ../include/geekos/malloc.h
geekos/synch.o: ../src/geekos/synch.c ../include/geekos/kthread.h \
  ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/list.h ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/fmtout.h \
  ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/int.h ../include/geekos/defs.h \
  ../include/geekos/synch.h
geekos/kthread.o: ../src/geekos/kthread.c ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/defs.h ../include/geekos/int.h \
  ../include/geekos/mem.h ../include/geekos/list.h \
  ../include/geekos/paging.h ../include/geekos/bootinfo.h \
  ../include/geekos/symbol.h ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/kthread.h ../include/geekos/malloc.h \
  ../include/geekos/serial.h ../include/geekos/irq.h \
  ../include/geekos/io.h
geekos/vm_cons.o: ../src/geekos/vm_cons.c ../include/geekos/fmtout.h \
  ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/idt.h ../include/geekos/int.h \
  ../include/geekos/kassert.h ../include/geekos/screen.h \
  ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/defs.h ../include/geekos/vm_cons.h \
  ../include/geekos/io.h
geekos/debug.o: ../src/geekos/debug.c ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/debug.h ../include/geekos/fmtout.h \
  ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/vm_cons.h ../include/geekos/io.h \
  ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/screen.h
geekos/serial.o: ../src/geekos/serial.c ../include/geekos/serial.h \
  ../include/geekos/irq.h ../include/geekos/int.h \
  ../include/geekos/kassert.h ../include/geekos/screen.h \
  ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/defs.h ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/io.h ../include/geekos/reboot.h \
  ../include/geekos/gdt.h ../include/geekos/idt.h
geekos/reboot.o: ../src/geekos/reboot.c ../include/geekos/reboot.h \
  ../include/libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h
geekos/paging.o: ../src/geekos/paging.c ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/int.h ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/defs.h ../include/geekos/idt.h \
  ../include/geekos/kthread.h ../include/geekos/list.h \
  ../include/geekos/mem.h ../include/geekos/paging.h \
  ../include/geekos/bootinfo.h ../include/geekos/malloc.h \
  ../include/geekos/gdt.h ../include/geekos/segment.h \
  ../include/geekos/crc32.h ../include/geekos/debug.h
geekos/main.o: ../src/geekos/main.c ../include/geekos/bootinfo.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdbool.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/geekos/mem.h ../include/geekos/defs.h \
  ../include/geekos/list.h ../include/geekos/kassert.h \
  ../include/geekos/paging.h ../include/geekos/crc32.h \
  ../include/geekos/tss.h ../include/geekos/int.h \
  ../include/geekos/kthread.h ../include/geekos/trap.h \
  ../include/geekos/timer.h ../include/geekos/keyboard.h \
  ../include/geekos/io.h ../include/geekos/serial.h \
  ../include/geekos/irq.h ../include/geekos/reboot.h \
  ../include/geekos/ide.h ../include/geekos/vm_cons.h \
  ../include/geekos/debug.h ../include/geekos/gdt.h
common/fmtout.o: ../src/common/fmtout.c \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/limits.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h
common/string.o: ../src/common/string.c ../include/libc/fmtout.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stdarg.h \
  ../include/libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h
common/memmove.o: ../src/common/memmove.c ../include/libc/string.h \
  /home/jarusl/palacios/devtools/i386/lib/gcc/i386-elf/3.4.6/include/stddef.h
