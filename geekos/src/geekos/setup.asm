; -*- fundamental -*-
; GeekOS setup code
; Copyright (c) 2001,2004 David H. Hovemeyer <daveho@cs.umd.edu>
; (c) 2008, Peter Dinda <pdinda@northwestern.edu> 
; (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
; (c) 2008, The V3VEE Project <http://www.v3vee.org> 
; $Revision: 1.8 $

; This is free software.  You are permitted to use,
; redistribute, and modify it as specified in the file "COPYING".

; A lot of this code is adapted from Kernel Toolkit 0.2
; and Linux version 2.2.x, so the following copyrights apply:

; Copyright (C) 1991, 1992 Linus Torvalds
; modified by Drew Eckhardt
; modified by Bruce Evans (bde)
; adapted for Kernel Toolkit by Luigi Sgro

%define __BIG_KERNEL__

%include "defs.asm"

[BITS 16]
[ORG 0x0]

start:
		db	0xEB
		db      46 ;trampoline

; This is the setup header, and it must start at %cs:2 (old 0x9020:2)

        	db	'H'		; header signature
		db      'd'
		db      'r'
		db      'S'
		dw	0x0203		; header version number (>= 0x0105)
					; or else old loadlin-1.5 will fail)
realmode_swtch:	dw	0, 0		; default_switch, SETUPSEG
start_sys_seg:	dw	SYSSEG
		dw	kernel_version	; pointing to kernel version string
					; above section of header is compatible
					; with loadlin-1.5 (header v1.5). Don't
					; change it.

type_of_loader:	db	0		; = 0, old one (LILO, Loadlin,
					;      Bootlin, SYSLX, bootsect...)
					; See Documentation/i386/boot.txt for
					; assigned ids
	
; flags, unused bits must be zero (RFU) bit within loadflags
loadflags:  db 1
;LOADED_HIGH	equ 1			; If set, the kernel is loaded high
;CAN_USE_HEAP	equ 0x80 			; If set, the loader also has set
					; heap_end_ptr to tell how much
					; space behind setup.S can be used for
					; heap purposes.
					; Only the loader knows what is free
;%ifndef __BIG_KERNEL__
;		db	1
;%else
;		db	1
;%endif

setup_move_size: dw  0x8000		; size to move, when setup is not
					; loaded at 0x90000. We will move setup 
					; to 0x90000 then just before jumping
					; into the kernel. However, only the
					; loader knows how much data behind
					; us also needs to be loaded.

code32_start: dd 0x100000				; here loaders can put a different
					; start address for 32-bit code.
;%ifndef __BIG_KERNEL__
;		dd	0x100000	;   0x1000 = default for zImage
;%else
;		dd	0x100000	; 0x100000 = default for big kernel
;%endif

ramdisk_image:	dd	0		; address of loaded ramdisk image
					; Here the loader puts the 32-bit
					; address where it loaded the image.
					; This only will be read by the kernel.

ramdisk_size:	dd	0		; its size in bytes

bootsect_kludge:
		dd	0		; obsolete

heap_end_ptr:	dw	modelist+1024	; (Header version 0x0201 or later)
					; space from here (exclusive) down to
					; end of setup code can be used by setup
					; for local heap purposes.

pad1:		dw	0
cmd_line_ptr:	dd      0		; (Header version 0x0202 or later)
					; If nonzero, a 32-bit pointer
					; to the kernel command line.
					; The command line should be
					; located between the start of
					; setup and the end of low
					; memory (0xa0000), or it may
					; get overwritten before it
					; gets read.  If this field is
					; used, there is no longer
					; anything magical about the
					; 0x90000 segment; the setup
					; can be located anywhere in
					; low memory 0x10000 or higher.

ramdisk_max:	  dd 0xffffffff
;kernel_alignment:  dd 0x200000          ; physical addr alignment required for
		      			; protected mode relocatable kernel
;%ifdef CONFIG_RELOCATABLE
;relocatable_kernel:    db 1
;%else
;relocatable_kernel:    db 0
;%endif
;pad2:                  db 0
;pad3:                  dw 0

;cmdline_size:   dd   COMMAND_LINE_SIZE-1     ;length of the command line,
                                              ;added with boot protocol
                                              ;version 2.06

trampoline:	call	start_setup
;		ALIGN 16
space: 
       %rep  1024
       	     db 0
       %endrep				; The offset at this point is 0x240	
					
; End of setup header ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


start_setup:

	; Redefine the data segment so we can access variables
	; declared in this file.
	mov	ax, SETUPSEG
	mov	ds, ax

	; Use int 15h to find out size of extended memory in KB.
	; Extended memory is the memory above 1MB.  So by
	; adding 1MB to this amount, we get the total amount
	; of system memory.  We can only detect 64MB this way,
	; but that's OK for now.
	;mov	ah, 0x88
	;int	0x15
	;add	ax, 1024	; 1024 KB == 1 MB
	mov 	ax, 0xe801
	int 	0x15
	add	ax, 1024	; 1024 KB == 1 MB
	mov	[mem_size_kbytes], ax
	mov	[mem_size_eblocks], bx

	; Kill the floppy motor.
	call	Kill_Motor

	; Block interrupts, since we can't meaningfully handle them yet
	; and we no longer need BIOS services.
	cli

	; Set up IDT and GDT registers
	lidt	[IDT_Pointer]
	lgdt	[GDT_Pointer]

	; Initialize the interrupt controllers, and enable the
	; A20 address line
	call	Init_PIC
	call	Enable_A20

	; Switch to protected mode!
	mov	ax, 0x01
	lmsw	ax


	; Jump to 32 bit code.
	jmp	dword KERNEL_CS:(SETUPSEG << 4) + setup_32

[BITS 32]
setup_32:

	; set up data segment registers
	mov	ax, KERNEL_DS
	mov	ds, ax
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	mov	ss, ax

	; Create the stack for the initial kernel thread.
	mov	esp, KERN_STACK + 4096

	; Build Boot_Info struct on stack.
	; Note that we push the fields on in reverse order,
	; since the stack grows downwards.

	;Zheng 08/02/2008
	xor eax, eax
	mov eax, [(SETUPSEG<<4)+ramdisk_size]
	push eax

	xor eax, eax
	mov eax, [(SETUPSEG<<4)+ramdisk_image]
	push eax	

	xor	eax, eax
	mov	ax, [(SETUPSEG<<4)+mem_size_kbytes]
	xor 	ebx, ebx
	mov	bx, [(SETUPSEG<<4)+mem_size_eblocks]
	shl	ebx, 6
	add	eax, ebx
	push	eax		; memSizeKB
	
	mov	eax, VMM_SIZE
	shl	eax, 9          ; Multiply the vmm size by 512 to get byte size
	push	ebx             ; size of the VMM

	push	dword 8		; bootInfoSize

	; Pass pointer to Boot_Info struct as argument to kernel
	; entry point.
	push	esp

	; Push return address to make this look like a call
	; XXX - untested
	push	dword (SETUPSEG<<4)+.returnAddr


	; Far jump into kernel
	jmp	KERNEL_CS:ENTRY_POINT

.returnAddr:
	; We shouldn't return here.
.here:	jmp .here




[BITS 16]

; Kill the floppy motor.
; This code was shamelessly stolen from Linux.
Kill_Motor:
	mov	dx, 0x3f2
	xor	al, al
	out	dx, al
	ret

Init_PIC:
	; Initialize master and slave PIC!
	mov	al, ICW1
	out	0x20, al		; ICW1 to master
	call	Delay
	out	0xA0, al		; ICW1 to slave
	call	Delay
	mov	al, ICW2_MASTER
	out	0x21, al		; ICW2 to master
	call	Delay
	mov	al, ICW2_SLAVE
	out	0xA1, al		; ICW2 to slave
	call	Delay
	mov	al, ICW3_MASTER
	out	0x21, al		; ICW3 to master
	call	Delay
	mov	al, ICW3_SLAVE
	out	0xA1, al		; ICW3 to slave
	call	Delay
	mov	al, ICW4
	out	0x21, al		; ICW4 to master
	call	Delay
	out	0xA1, al		; ICW4 to slave
	call	Delay
	mov	al, 0xff		; mask all ints in slave
	out	0xA1, al		; OCW1 to slave
	call	Delay
	mov	al, 0xfb		; mask all ints but 2 in master
	out	0x21, al		; OCW1 to master
	call	Delay
	ret

; Linux uses this code.
; The idea is that some systems issue port I/O instructions
; faster than the device hardware can deal with them.
Delay:
	jmp	.done
.done:	ret

; Enable the A20 address line, so we can correctly address
; memory above 1MB.
Enable_A20:
	mov	al, 0xD1
	out	0x64, al
	call	Delay
	mov	al, 0xDF
	out	0x60, al
	call	Delay
	ret


; ----------------------------------------------------------------------
; Setup data
; ----------------------------------------------------------------------

mem_size_kbytes: dw 0
mem_size_eblocks: dw 0


; ----------------------------------------------------------------------
; The GDT.  Creates flat 32-bit address space for the kernel
; code, data, and stack.  Note that this GDT is just used
; to create an environment where we can start running 32 bit
; code.  The kernel will create and manage its own GDT.
; ----------------------------------------------------------------------

; GDT initialization stuff
NUM_GDT_ENTRIES equ 3		; number of entries in GDT
GDT_ENTRY_SZ equ 8		; size of a single GDT entry

align 8, db 0
GDT:
	; Descriptor 0 is not used
	dw 0
	dw 0
	dw 0
	dw 0

	; Descriptor 1: kernel code segment
	dw 0xFFFF	; bytes 0 and 1 of segment size
	dw 0x0000	; bytes 0 and 1 of segment base address
	db 0x00		; byte 2 of segment base address
	db 0x9A		; present, DPL=0, non-system, code, non-conforming,
			;   readable, not accessed
	db 0xCF		; granularity=page, 32 bit code, upper nibble of size
	db 0x00		; byte 3 of segment base address

	; Descriptor 2: kernel data and stack segment
	; NOTE: what Intel calls an "expand-up" segment
	; actually means that the stack will grow DOWN,
	; towards lower memory.  So, we can use this descriptor
	; for both data and stack references.
	dw 0xFFFF	; bytes 0 and 1 of segment size
	dw 0x0000	; bytes 0 and 1 of segment base address
	db 0x00		; byte 2 of segment base address
	db 0x92		; present, DPL=0, non-system, data, expand-up,
			;   writable, not accessed
	db 0xCF		; granularity=page, big, upper nibble of size
	db 0x00		; byte 3 of segment base address

GDT_Pointer:
	dw NUM_GDT_ENTRIES*GDT_ENTRY_SZ	; limit
	dd (SETUPSEG<<4) + GDT		; base address

IDT_Pointer:
	dw 0
	dd 00

; Here's a bunch of information about your current kernel..
kernel_version:	db	"1.0.0VMMHack"
		db	" ("
		db	"copyright"
		db	"@"
		db	"2008"
		db	") "
		db	""
		db	0

modelist:
