#ifndef _PALACIOS_HOST_DEV_USER_H_
#define _PALACIOS_HOST_DEV_USER_H_

/*
 * Palacios Host Device Interface + User-space interface 
 * (c) Peter Dinda, 2011
 */


#define V3_VM_HOST_DEV_CONNECT (10244+1)

/* to detemine whether a host request is available, poll the fd for read */

/* make a request for reading/writing guest or injecting irq */
/* the arguemnt is a pointer to a palacios_host_dev_user_op struct */
/* return is negative on error, positive to indicate bytes read/written or irq injected*/
#define V3_HOST_DEV_USER_REQUEST_PUSH_IOCTL  (10244+2)

/* find out the size of the current host request, if one is pending */
/* you find out if one is pending by read poll/select on the fd */
/* the argument is a pointer to a uint64_t that will give the total size */
/* ioctl returns 1 if a request is ready, 0 if there is no request */
/* -EFAULT if there is a an error */
#define V3_HOST_DEV_HOST_REQUEST_SIZE_IOCTL  (10244+3)

/* get the current host request, if one is available */
/* the argument is a pointer to a palacios_host_dev_host_request_response */
/* of the needed size */
/* ioctl returns 1 if a request is ready+copied, 0 if there is no request */
/* -EFAULT if there is a an error */
#define V3_HOST_DEV_HOST_REQUEST_PULL_IOCTL  (10244+4)

/* write back the response to the previously pulled host request */
/* the argument is a pointer to a palacios_host_dev_host_request_response */
#define V3_HOST_DEV_USER_RESPONSE_PUSH_IOCTL (10244+5)


#ifdef __KERNEL__
#define USER __user
#else
#define USER
#endif

struct palacios_host_dev_user_op {
#define PALACIOS_HOST_DEV_USER_REQUEST_READ_GUEST  1
#define PALACIOS_HOST_DEV_USER_REQUEST_WRITE_GUEST 2
#define PALACIOS_HOST_DEV_USER_REQUEST_IRQ_GUEST   3
    uint32_t        type;   // type of operation (from the #defs above)
    void            *gpa;   // physical address in guest to read or write
    void USER      *data;   // user address of data that will be read or written
    uint64_t        len;    // number of bytes to move

    uint8_t         irq;    // irq to inject
};


struct palacios_host_dev_host_request_response {
    // data_len must remain the first field in this structure
    uint64_t  data_len;    // size of the structure + occupied data
    uint64_t  len;         // size of the structure in total
#define PALACIOS_HOST_DEV_HOST_REQUEST_READ_IO     1
#define PALACIOS_HOST_DEV_HOST_REQUEST_WRITE_IO    2
#define PALACIOS_HOST_DEV_HOST_REQUEST_READ_MEM    3
#define PALACIOS_HOST_DEV_HOST_REQUEST_WRITE_MEM   4
#define PALACIOS_HOST_DEV_HOST_REQUEST_READ_CONF   5
#define PALACIOS_HOST_DEV_HOST_REQUEST_WRITE_CONF  6
    uint32_t  type;        // one of the types given above
    uint16_t  port;        // port number, for IO
    void      *gpa;         // physical address in the guest for memory ops
    uint64_t  conf_addr;   // address in the configuration for configuration ops

    uint64_t  op_len;      // response: bytes read/written to the device
                           // request: bytes to read/write
                            
    uint8_t   data[0];     // data (if any)

} ;



#endif
