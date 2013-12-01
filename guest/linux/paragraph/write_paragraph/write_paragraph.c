#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>

#define PARAGRAPH_PADDR 0x80000000
#define PARAGRAPH_LEN (640*480*4)

int main(int argc, char *argv[]) 
{
  int start;

  if (argc < 2) {
    printf("Usage: write_paragraph start\n");
    return 0;
  }
  
  start = atoi(argv[1]);

  int fd = open("/dev/mem", O_RDWR | O_SYNC);

  if (fd<0) { 
    perror("Cannot open /dev/mem");
    return -1;
  }

  unsigned char *mem = mmap(NULL, PARAGRAPH_LEN, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, PARAGRAPH_PADDR);

  if (mem == NULL) {
    perror("Can't map memory");
    return -1;
  } else { 
    printf("Mapped to 0x%p (%d bytes)\n", mem, PARAGRAPH_LEN);
  }
  
  int i;
  for (i = 0; i < PARAGRAPH_LEN; ++i) {
    mem[i] = i+start;
  }
  printf("Wrote %d bytes\n", PARAGRAPH_LEN);

  sleep(99999);

}
