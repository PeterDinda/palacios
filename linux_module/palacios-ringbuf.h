/* Ringbuffer implementation for Palacios
 */

#ifndef PALACIOS_RINGBUF_H
#define PALACIOS_RINGBUF_H

extern struct v3_ringbuf * v3_create_ringbuf(unsigned int size);
extern void v3_free_ringbuf(struct v3_ringbuf * ring);
extern int v3_ringbuf_read(struct v3_ringbuf * ring, unsigned char * dst, unsigned int len);
extern int v3_ringbuf_write(struct v3_ringbuf * ring, unsigned char * src, unsigned int len);



#endif
