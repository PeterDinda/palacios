#ifndef __HCALL__
#define __HCALL__

/*
  Calling convention:

64 bit:
  rax = hcall number
  rbx = 0x6464646464646464...
  rcx = 1st arg
  rdx = 2nd arg
  rsi = 3rd arg
  rdi = 4th arg
  r8  = 5th arg
  r9  = 6th arg
  r10 = 7th arg
  r11 = 8th arg

32 bit:
  eax = hcall number
  ebx = 0x32323232
  arguments on stack in C order (first argument is TOS)
     arguments are also 32 bit
*/
#define HCALL64(rc,id,a,b,c,d,e,f,g,h)		      \
  asm volatile ("movq %1, %%rax; "		      \
		"pushq %%rbx; "			      \
		"movq $0x6464646464646464, %%rbx; "   \
		"movq %2, %%rcx; "		      \
		"movq %3, %%rdx; "		      \
		"movq %4, %%rsi; "		      \
		"movq %5, %%rdi; "		      \
		"movq %6, %%r8 ; "		      \
		"movq %7, %%r9 ; "		      \
		"movq %8, %%r10; "		      \
		"movq %9, %%r11; "		      \
		"vmmcall ;       "		      \
		"movq %%rax, %0; "		      \
		"popq %%rbx; "			      \
		: "=m"(rc)			      \
		: "m"(id),			      \
                  "m"(a), "m"(b), "m"(c), "m"(d),     \
		  "m"(e), "m"(f), "m"(g), "m"(h)      \
		: "%rax","%rcx","%rdx","%rsi","%rdi", \
		  "%r8","%r9","%r10","%r11"	      \
		)

#define HCALL32(rc,id,a,b,c,d,e,f,g,h)		      \
  asm volatile ("movl %1, %%eax; "		      \
		"pushl %%ebx; "			      \
		"movl $0x32323232, %%ebx; "	      \
		"pushl %9;"			      \
		"pushl %8;"			      \
		"pushl %7;"			      \
		"pushl %6;"			      \
		"pushl %5;"			      \
		"pushl %4;"			      \
		"pushl %3;"			      \
		"pushl %2;"			      \
		"vmmcall ;       "		      \
		"movl %%eax, %0; "		      \
		"addl $32, %%esp; "		      \
		"popl %%ebx; "			      \
		: "=r"(rc)			      \
		: "m"(id),			      \
		  "m"(a), "m"(b), "m"(c), "m"(d),     \
		"m"(e), "m"(f), "m"(g), "m"(h)	      \
		: "%eax"			      \
		)

#ifdef __x86_64__
#define HCALL(rc,id,a,b,c,d,e,f,g,h)  HCALL64(rc,id,a,b,c,d,e,f,g,h)
#else
#define HCALL(rc,id,a,b,c,d,e,f,g,h)  HCALL32(rc,id,a,b,c,d,e,f,g,h)   
#endif

#endif
