#ifndef _PALACIOS_KEYED_STREAM_USER_H_
#define _PALACIOS_KEYED_STREAM_USER_H_

/*
 * Palacios Keyed Stream User Interface
 * (c) Clint Sbisa, 2011
 */


// Issue a V3_VM_KSTREAM_USER_CONNECT on the VM to acquire an fd for the device

// get size of pending request
// Note that this is not the wrong ioctl - the connect ioctl applies to the VM device
// the following ioctls apply to the FD returned by the connect
#define V3_KSTREAM_REQUEST_SIZE_IOCTL  (11244+1)
// get the pending request
#define V3_KSTREAM_REQUEST_PULL_IOCTL  (11244+2)
// push a response to the previously pulled request
#define V3_KSTREAM_RESPONSE_PUSH_IOCTL (11244+3)

#ifdef __KERNEL__
#define USER __user
#else
#define USER
#endif


struct palacios_user_keyed_stream_url {
    uint64_t len;
    char     url[0];  // len describes it
};


//
// This structure is used for both requests (kernel->user)
// and responses (user->kernel)
//
// for a readkey request, the buf contains the tag
// for a readkey response, the buf contains the data
// for a writekey request, the buf contains the data + key
// for a writekey request, the buf contains nothing
struct palacios_user_keyed_stream_op {

    uint64_t len; // total structure length (all)

    int     type; // request or response type
#define PALACIOS_KSTREAM_OPEN      1  // not used
#define PALACIOS_KSTREAM_CLOSE     2  // not used
#define PALACIOS_KSTREAM_OPEN_KEY  3
#define PALACIOS_KSTREAM_CLOSE_KEY 4
#define PALACIOS_KSTREAM_WRITE_KEY 5
#define PALACIOS_KSTREAM_READ_KEY  6

    sint64_t xfer;      // total bytes read or written (request/response)
                        // 

    void    *user_key;  // user tag for an open key (response)

    uint64_t buf_len;   // buffer len
    uint64_t data_off;  // offset of data within the buffer
                        // 0..buffer_len-1 is tag
                        // rest is data
    char buf[0];        // expanded as needed (key or value)

    // The buffer contains the key or the value 
};





#endif
