#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>

#define PARAGRAPH_SYSFS_RESOURCE "/sys/devices/pci0000:00/0000:00:02.0/resource0"
#define PARAGRAPH_LEN (1024*1024*4)

int main(int argc, char *argv[]) 
{
  int start;
  char *res;

  if (argc < 3) {
    printf("Usage: write_paragraph_sysfs sysfsresource start\nyou probably want to use %s\n",PARAGRAPH_SYSFS_RESOURCE);
    return 0;
  }
  
  res = argv[1];
  start = atoi(argv[2]);

  int fd = open(res, O_RDWR | O_SYNC);

  if (fd<0) { 
    perror("Cannot open sysfs file");
    printf("sysfs file = %s\n", res);
    return -1;
  }

  unsigned char *mem = mmap(NULL, PARAGRAPH_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

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

  sleep(99999);

}
