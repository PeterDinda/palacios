#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>

#define PARAGRAPH_PADDR 0x80000000
#define PARAGRAPH_LEN (1024*1024*4)

int main(int argc, char *argv[]) 
{
  int start;

  if (argc < 2) {
    printf("Usage: write_paragraph_devmem start\n");
    return 0;
  }
  
  start = atoi(argv[1]);

  int fd = open("/dev/mem", O_RDWR | O_SYNC);

  if (fd<0) { 
    perror("Cannot open /dev/mem");
    return -1;
  }

  unsigned char *mem = mmap(NULL, PARAGRAPH_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, PARAGRAPH_PADDR);

  if (mem == MAP_FAILED) {
    perror("Can't map memory");
    return -1;
  } else { 
    printf("Mapped to 0x%p (%d bytes)\n", mem, PARAGRAPH_LEN);
  }
  
  int i;
  for (i = 0; i < PARAGRAPH_LEN; i++) {
    *(mem+i) = i+start;
    if (i<16) { printf("0x%p = %d\n", mem+i, *(mem+i)); } 
  }
  printf("Wrote %d bytes\n", PARAGRAPH_LEN);

}
