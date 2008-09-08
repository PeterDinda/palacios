#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__


#include <geekos/ktypes.h>


struct ring_buffer {
  uchar_t * buf;
  uint_t size;

  uint_t start;
  uint_t end;
  uint_t current_len;

};


void init_ring_buffer(struct ring_buffer * ring, uint_t size);
struct ring_buffer * create_ring_buffer(uint_t size);

void free_ring_buffer(struct ring_buffer * ring);


int rb_read(struct ring_buffer * ring, char * dst, uint_t len);
int rb_peek(struct ring_buffer * ring, char * dst, uint_t len);
int rb_delete(struct ring_buffer * ring, uint_t len);
int rb_write(struct ring_buffer * ring, char * src, uint_t len);
int rb_data_len(struct ring_buffer * ring);
int rb_capacity(struct ring_buffer * ring);


void print_ring_buffer(struct ring_buffer * ring);


#endif // ! __RING_BUFFER_H__
