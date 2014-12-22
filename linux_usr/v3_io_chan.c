#define __V3_PAL_SIDE__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <sys/ioctl.h>

#define __V3_PAL_SIDE__

// These two will be included in the library 
#include "v3_io_chan.h"
#include "v3_user_host_dev.h"


int
v3_get_ioreq (v3_io_chan_handle_t chan, pal_ioreq_t ** req)
{
    return v3_user_host_dev_pull_request(chan, req);
}


int
v3_push_ioresp (v3_io_chan_handle_t chan, pal_ioresp_t * resp)
{
    return v3_user_host_dev_push_response(chan, resp);
}


v3_io_chan_handle_t
v3_io_chan_open (char * url, char * vmdev)
{
    return v3_user_host_dev_rendezvous(vmdev, url);
}


void 
v3_io_chan_close (v3_io_chan_handle_t chan)
{
    v3_user_host_dev_depart(chan);
}


int
v3_raise_pal_irq (v3_io_chan_handle_t chan, uint8_t irq)
{
    return v3_user_host_dev_raise_irq(chan, irq);
}


int 
v3_lower_pal_irq (v3_io_chan_handle_t chan, uint8_t irq)
{
    return v3_user_host_dev_lower_irq(chan, irq);
}


uint64_t 
v3_read_guest_mem (v3_io_chan_handle_t chan, void *gpa, void * dest, uint64_t len){
    return v3_user_host_dev_read_guest_mem(chan, gpa, dest, len);
}


uint64_t 
v3_write_guest_mem (v3_io_chan_handle_t chan, void *gpa, void *src, uint64_t len)
{
    return v3_user_host_dev_write_guest_mem(chan, gpa, src, len);
}


int
v3_has_ioreq (v3_io_chan_handle_t chan)
{
    return v3_user_host_dev_have_request(chan);
}


void
v3_free_req_resp (pal_ioreq_t * reqresp)
{
    free(reqresp);
}

