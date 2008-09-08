#include <geekos/ring_buffer.h>
#include <geekos/malloc.h>
#include <geekos/ktypes.h>



void init_ring_buffer(struct ring_buffer * ring, uint_t size) {
  ring->buf = Malloc(size);
  ring->size = size;
  
  ring->start = 0;
  ring->end = 0;
  ring->current_len = 0;
}


struct ring_buffer * create_ring_buffer(uint_t size) {
  struct ring_buffer * ring = (struct ring_buffer *)Malloc(sizeof(struct ring_buffer));

  init_ring_buffer(ring, size);

  return ring;
}

void free_ring_buffer(struct ring_buffer * ring) {
  Free(ring->buf);
  Free(ring);
}





static inline uchar_t * get_buf_ptr(struct ring_buffer * ring) {
  return (uchar_t *)&(ring->buf + ring->start);
}


static inline int get_section_size(struct ring_buffer * ring) {
  return ring->size - ring->start;
}

static inline int is_loop(struct ring_buffer * ring, uint_t len) {
  if ((ring->start >= ring->end) && (ring->current_len > 0)) {
    // end is past the end of the buffer
    if (get_section_size(ring) < len) {
      return 1;
    }
  }

  return 0;
}


int rb_read(struct ring_buffer * ring, char * dst, uint_t len) {
  int read_len = 0;
  int ring_data_len = ring->current_len;

  read_len = (len > ring_data_len) ? ring_data_len : len;

  if (is_loop(ring, read_len)) {
    int first_len = get_section_size(ring);

    memcpy(dst, get_buf_ptr(ring), first_len);
    read_len -= first_len;
    ring->start = 0;

    memcpy(dst + first_len, get_buf_ptr, read_len);

  } else {
    memcpy(dst, get_buf_ptr(ring), read_len);
  }

  return read_len;
  
}
