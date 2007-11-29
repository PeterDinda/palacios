geekos/idt.o: ../src/geekos/idt.c ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/defs.h ../include/geekos/idt.h \
  ../include/geekos/int.h
geekos/int.o: ../src/geekos/int.c ../include/geekos/idt.h \
  ../include/geekos/int.h ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/defs.h ../include/geekos/serial.h \
  ../include/geekos/irq.h ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h \
  ../include/geekos/io.h
geekos/trap.o: ../src/geekos/trap.c ../include/geekos/idt.h \
  ../include/geekos/int.h ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/defs.h ../include/geekos/kthread.h \
  ../include/geekos/list.h ../include/geekos/trap.h
geekos/irq.o: ../src/geekos/irq.c ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/idt.h ../include/geekos/int.h \
  ../include/geekos/defs.h ../include/geekos/io.h ../include/geekos/irq.h
geekos/io.o: ../src/geekos/io.c ../include/geekos/io.h \
  ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h
geekos/keyboard.o: ../src/geekos/keyboard.c ../include/geekos/kthread.h \
  ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/list.h ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/irq.h \
  ../include/geekos/int.h ../include/geekos/defs.h ../include/geekos/io.h \
  ../include/geekos/keyboard.h
geekos/screen.o: ../src/geekos/screen.c \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdarg.h \
  ../include/geekos/kassert.h ../include/geekos/screen.h \
  ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/io.h ../include/geekos/int.h ../include/geekos/defs.h \
  ../include/geekos/fmtout.h ../include/geekos/../libc/fmtout.h
geekos/timer.o: ../src/geekos/timer.c \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/limits.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/syslimits.h \
  /usr/include/limits.h /usr/include/features.h /usr/include/sys/cdefs.h \
  /usr/include/gnu/stubs.h /usr/include/bits/posix1_lim.h \
  /usr/include/bits/local_lim.h /usr/include/linux/limits.h \
  /usr/include/bits/posix2_lim.h ../include/geekos/io.h \
  ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/int.h ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/defs.h \
  ../include/geekos/irq.h ../include/geekos/kthread.h \
  ../include/geekos/list.h ../include/geekos/timer.h
geekos/mem.o: ../src/geekos/mem.c ../include/geekos/defs.h \
  ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/kassert.h ../include/geekos/screen.h \
  ../include/geekos/bootinfo.h ../include/geekos/gdt.h \
  ../include/geekos/int.h ../include/geekos/malloc.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h \
  ../include/geekos/mem.h ../include/geekos/list.h \
  ../include/geekos/paging.h
geekos/crc32.o: ../src/geekos/crc32.c ../include/geekos/crc32.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h \
  ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/kassert.h ../include/geekos/screen.h
geekos/gdt.o: ../src/geekos/gdt.c ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/segment.h ../include/geekos/int.h \
  ../include/geekos/defs.h ../include/geekos/tss.h \
  ../include/geekos/gdt.h
geekos/tss.o: ../src/geekos/tss.c ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/defs.h ../include/geekos/gdt.h \
  ../include/geekos/segment.h ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h \
  ../include/geekos/tss.h
geekos/segment.o: ../src/geekos/segment.c ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h \
  ../include/geekos/tss.h ../include/geekos/segment.h
geekos/bget.o: ../src/geekos/bget.c ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h \
  ../include/geekos/kassert.h ../include/geekos/screen.h \
  ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/bget.h
geekos/malloc.o: ../src/geekos/malloc.c ../include/geekos/screen.h \
  ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/int.h ../include/geekos/kassert.h \
  ../include/geekos/defs.h ../include/geekos/bget.h \
  ../include/geekos/malloc.h
geekos/synch.o: ../src/geekos/synch.c ../include/geekos/kthread.h \
  ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/list.h ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/int.h \
  ../include/geekos/defs.h ../include/geekos/synch.h
geekos/kthread.o: ../src/geekos/kthread.c ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/defs.h ../include/geekos/int.h \
  ../include/geekos/mem.h ../include/geekos/list.h \
  ../include/geekos/paging.h ../include/geekos/bootinfo.h \
  ../include/geekos/symbol.h ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h \
  ../include/geekos/kthread.h ../include/geekos/malloc.h
geekos/serial.o: ../src/geekos/serial.c ../include/geekos/serial.h \
  ../include/geekos/irq.h ../include/geekos/int.h \
  ../include/geekos/kassert.h ../include/geekos/screen.h \
  ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/defs.h ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h \
  ../include/geekos/io.h ../include/geekos/reboot.h \
  ../include/geekos/gdt.h ../include/geekos/idt.h
geekos/reboot.o: ../src/geekos/reboot.c ../include/geekos/reboot.h \
  ../include/libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h
geekos/paging.o: ../src/geekos/paging.c ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h \
  ../include/geekos/int.h ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/defs.h ../include/geekos/idt.h \
  ../include/geekos/kthread.h ../include/geekos/list.h \
  ../include/geekos/mem.h ../include/geekos/paging.h \
  ../include/geekos/bootinfo.h ../include/geekos/malloc.h \
  ../include/geekos/gdt.h ../include/geekos/segment.h \
  ../include/geekos/crc32.h ../include/geekos/serial.h \
  ../include/geekos/irq.h ../include/geekos/io.h
geekos/vmx.o: ../src/geekos/vmx.c ../include/geekos/vmx.h \
  ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/vmcs.h ../include/geekos/serial.h \
  ../include/geekos/irq.h ../include/geekos/int.h \
  ../include/geekos/kassert.h ../include/geekos/screen.h \
  ../include/geekos/defs.h ../include/geekos/string.h \
  ../include/geekos/../libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h \
  ../include/geekos/io.h ../include/geekos/vmcs_gen.h \
  ../include/geekos/mem.h ../include/geekos/list.h \
  ../include/geekos/paging.h ../include/geekos/bootinfo.h
geekos/vmcs_gen.o: ../src/geekos/vmcs_gen.c ../include/geekos/vmcs_gen.h \
  ../include/geekos/vmcs.h ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/serial.h ../include/geekos/irq.h \
  ../include/geekos/int.h ../include/geekos/kassert.h \
  ../include/geekos/screen.h ../include/geekos/defs.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h \
  ../include/geekos/io.h
geekos/main.o: ../src/geekos/main.c ../include/geekos/bootinfo.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h \
  ../include/geekos/screen.h ../include/geekos/ktypes.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdbool.h \
  ../include/geekos/mem.h ../include/geekos/defs.h \
  ../include/geekos/list.h ../include/geekos/kassert.h \
  ../include/geekos/paging.h ../include/geekos/crc32.h \
  ../include/geekos/tss.h ../include/geekos/int.h \
  ../include/geekos/kthread.h ../include/geekos/trap.h \
  ../include/geekos/timer.h ../include/geekos/keyboard.h \
  ../include/geekos/io.h ../include/geekos/serial.h \
  ../include/geekos/irq.h ../include/geekos/reboot.h \
  ../include/geekos/vmx.h ../include/geekos/vmcs.h \
  ../include/geekos/vmcs_gen.h
common/fmtout.o: ../src/common/fmtout.c \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdarg.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h \
  ../include/geekos/string.h ../include/geekos/../libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/limits.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/syslimits.h \
  /usr/include/limits.h /usr/include/features.h /usr/include/sys/cdefs.h \
  /usr/include/gnu/stubs.h /usr/include/bits/posix1_lim.h \
  /usr/include/bits/local_lim.h /usr/include/linux/limits.h \
  /usr/include/bits/posix2_lim.h ../include/geekos/fmtout.h \
  ../include/geekos/../libc/fmtout.h
common/string.o: ../src/common/string.c ../include/libc/fmtout.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stdarg.h \
  ../include/libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h
common/memmove.o: ../src/common/memmove.c ../include/libc/string.h \
  /usr/lib/gcc/i386-redhat-linux/3.4.6/include/stddef.h
