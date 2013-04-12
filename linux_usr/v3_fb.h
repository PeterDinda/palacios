#ifndef __v3_fb_h__
#define __v3_fb_h__

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#include <stdint.h>


EXTERNC struct v3_frame_buffer_spec {
    uint32_t height;
    uint32_t width;
    uint8_t  bytes_per_pixel; 
    uint8_t  bits_per_channel;
    uint8_t  red_offset;   // byte offset in pixel to get to red channel
    uint8_t  green_offset; // byte offset in pixel to get to green channel
    uint8_t  blue_offset;  // byte offset in pixel to get to blue channel
};

EXTERNC int v3_get_fb_spec(int fd, struct v3_frame_buffer_spec *spec);
EXTERNC int v3_have_update(int fd);
EXTERNC int v3_get_fb_data(int fd, struct v3_frame_buffer_spec *spec, void *data);


EXTERNC int v3_send_key(int fd, uint8_t scan_code);
EXTERNC int v3_send_mouse(int fd, 
			  uint8_t sx, uint8_t dx, // dx sign and dx
			  uint8_t sy, uint8_t dy, // dy sign and dy
			  uint8_t buttons);       // button state



#endif
