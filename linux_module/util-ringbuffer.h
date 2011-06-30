/*
 * Ringbuffer 
 * (c) Lei Xia  2010
 */
 

#ifndef __PALACIOS_RING_BUFFER_H__
#define __PALACIOS_RING_BUFFER_H__

struct ringbuf {
    unsigned char * buf;
    unsigned int size;

    unsigned int start;
    unsigned int end;
    unsigned int current_len;
};


struct ringbuf * create_ringbuf(unsigned int size);
void free_ringbuf(struct ringbuf * ring);
int ringbuf_read(struct ringbuf * ring, unsigned char * dst, unsigned int len);
int ringbuf_write(struct ringbuf * ring, unsigned char * src, unsigned int len);
int ringbuf_data_len(struct ringbuf * ring);

#endif

