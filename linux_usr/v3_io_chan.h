#ifndef __V3_IO_CHAN_H__
#define __V3_IO_CHAN_H__

#ifndef __V3_PAL_SIDE__
#include <stdint.h>
#define PALACIOS_HOST_DEV_USER_REQUEST_READ_GUEST  1
#define PALACIOS_HOST_DEV_USER_REQUEST_WRITE_GUEST 2
#define PALACIOS_HOST_DEV_USER_REQUEST_IRQ_GUEST   3

#define PALACIOS_HOST_DEV_HOST_REQUEST_READ_IO     1
#define PALACIOS_HOST_DEV_HOST_REQUEST_WRITE_IO    2
#define PALACIOS_HOST_DEV_HOST_REQUEST_READ_MEM    3
#define PALACIOS_HOST_DEV_HOST_REQUEST_WRITE_MEM   4
#define PALACIOS_HOST_DEV_HOST_REQUEST_READ_CONF   5
#define PALACIOS_HOST_DEV_HOST_REQUEST_WRITE_CONF  6

struct palacios_host_dev_user_op {
    uint32_t        type;   // type of operation (from the #defs above)
    void            *gpa;   // physical address in guest to read or write
    void            *data;  // user address of data that will be read or written
    uint64_t        len;    // number of bytes to move
    uint8_t         irq;    // irq to inject
};


struct palacios_host_dev_host_request_response {
    // data_len must remain the first field in this structure
    uint64_t  data_len;    // size of the structure + occupied data
    uint64_t  len;         // size of the structure in total
    uint32_t  type;        // one of the types given above
    uint16_t  port;        // port number, for IO
    void      *gpa;         // physical address in the guest for memory ops
    uint64_t  conf_addr;   // address in the configuration for configuration ops

    uint64_t  op_len;      // response: bytes read/written to the device
                           // request: bytes to read/write
                            
    uint8_t   data[0];     // data (if any)

};
#endif

typedef struct palacios_host_dev_host_request_response pal_ioreq_t;
typedef struct palacios_host_dev_host_request_response pal_ioresp_t;

typedef long v3_io_chan_handle_t;

v3_io_chan_handle_t v3_io_chan_open (char * url, char * vmdev);
int v3_has_ioreq (v3_io_chan_handle_t chan);
void v3_io_chan_close(v3_io_chan_handle_t chan);
int v3_get_ioreq (v3_io_chan_handle_t chan, pal_ioresp_t ** req);
int v3_push_ioresp (v3_io_chan_handle_t chan, pal_ioresp_t * resp);
int v3_raise_pal_irq (v3_io_chan_handle_t chan, uint8_t irq);
int v3_lower_pal_irq (v3_io_chan_handle_t chan, uint8_t irq);
void v3_free_req_resp (pal_ioreq_t * reqresp);
uint64_t v3_read_guest_mem(v3_io_chan_handle_t chan, void *gpa, void * dest, uint64_t len);
uint64_t v3_write_guest_mem(v3_io_chan_handle_t chan, void *gpa, void *src, uint64_t len);

#endif
