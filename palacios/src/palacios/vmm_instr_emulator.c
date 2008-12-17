#include <stdio.h>
#include <stdlib.h>

#define addr_t unsigned long




void test() {
  int a = 10;
  int b;

  asm ("movl %1, %%eax\n\t"
       "movl %%eax, %0\n\t"
       :"=r"(b)        /* output */
       :"r"(a)         /* input */
       :"%eax"         /* clobbered register */
       );       

}

void get_flags(addr_t * flags) {
  addr_t tmp;
  asm  ("pushfq\n\t"
	"pop %0\n\t"
	:"=r"(tmp)
	: 
	);

  *flags = tmp;
}

void adc64(int * dst, int * src, addr_t * flags) {
  int tmp_dst = *dst, tmp_src = *src;
  addr_t tmp_flags = *flags;

  char * inst = "adcl";

  // Some of the flags values are not copied out in a pushf, we save them here
  addr_t flags_rsvd = *flags & ~0xfffe7fff;

  asm volatile (
       "pushfq\r\n"
       "push %3\r\n"
       "popfq\r\n"
       "adcl %2, %0\r\n"
       "pushfq\r\n"
       "pop %1\r\n"
       "popfq\r\n"
       : "=a"(tmp_dst),"=c"(tmp_flags)
       : "b"(tmp_src),"c"(tmp_flags), "0"(tmp_dst)
    );

  *dst = tmp_dst;
  *flags = tmp_flags;
  *flags |= flags_rsvd;

}


void adc32(int * dst, int * src, addr_t * flags) {
  int tmp_dst = *dst, tmp_src = *src;
  addr_t tmp_flags = *flags;

  
  asm volatile (
       "pushfd\r\n"
	"push %3\r\n"
	"popfd\r\n"
	"adcl %2, %0\r\n"
	"pushfd\r\n"
	"pop %1\r\n"
	"popfd\r\n"
	: "=a"(tmp_dst),"=c"(tmp_flags)
       : "b"(tmp_src),"c"(tmp_flags), "0"(tmp_dst)
    );

  *dst = tmp_dst;
  *flags = tmp_flags;

}


int main(int argc, char ** argv) {
  addr_t flags;
  int dest = 4;
  int src = 5;
  
  printf("sizeof ulong: %d\n", sizeof(unsigned long));

  printf("Getting flags\n");
  get_flags(&flags);
  flags = flags | 0x1;

  printf("Flags=0x%x\n", flags);
  test();
  printf("Adding\n");
  adc64(&dest, &src, &flags);
  printf("Result=%d\n", dest);

}
