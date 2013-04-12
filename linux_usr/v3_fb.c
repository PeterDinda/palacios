#include "v3_fb.h"


// This is the data structure that is passed back and forth with user-land
// ioctl
#define V3_VM_FB_INPUT 256+1
struct v3_fb_input {
    enum { V3_FB_KEY, V3_FB_MOUSE, V3_FB_BOTH}   data_type;
    uint8_t scan_code;
    uint8_t sx;      // sign bit for deltax
    uint8_t dx;      // deltax
    uint8_t sy;      // sign bit for deltay
    uint8_t dy;      // deltay
    uint8_t buttons; // button state
};

#define V3_VM_FB_QUERY 256+2
#define __user 
struct v3_fb_query_response {
    enum e { V3_FB_DATA_ALL, V3_FB_DATA_BOX, V3_FB_UPDATE, V3_FB_SPEC }  request_type;
    struct v3_frame_buffer_spec spec;    // in: desired spec; out: actual spec
    uint32_t x, y, w, h;                 // region to copy (0s = all) in/out args
    int updated;                         // whether this region has been updated or not
    void __user *data;                   // user space pointer to copy data to
};

int v3_send_key(int fd, uint8_t scan_code) 
{
    struct v3_fb_input e;

    e.data_type=V3_FB_KEY;
    e.scan_code=scan_code;

    if (ioctl(fd,V3_VM_FB_INPUT,&e)<0) { 
	perror("v3_send_key");
	return -1;
    }
    
    return 0;
}


int v3_send_mouse(int fd, uint8_t sx, uint8_t dx, uint8_t sy, uint8_t dy, uint8_t buttons)
{
    struct v3_fb_input e;

    e.data_type=V3_FB_MOUSE;
    e.sx=sx;
    e.dx=dx;
    e.sy=sy;
    e.dy=dy;
    e.buttons=buttons;

    if (ioctl(fd,V3_VM_FB_INPUT,&e)<0) { 
	perror("v3_send_mouse");
	return -1;
    }

    return 0;
}

int v3_get_fb_spec(int fd, struct v3_frame_buffer_spec *spec)
{
    struct v3_fb_query_response q;

    q.request_type=V3_FB_SPEC;
    
    if (ioctl(fd,V3_VM_FB_QUERY,&q)<0) { 
	perror("v3_get_fb_spec");
	return -1;

    }

    *spec = q.spec;
    
    return 0;
}

int v3_get_fb_data(int fd, struct v3_frame_buffer_spec *spec, void *data)
{
    struct v3_fb_query_response q;

    q.request_type=V3_FB_DATA_ALL;
    q.spec=*spec;
    q.data=data;

    if (ioctl(fd,V3_VM_FB_QUERY,&q)<0) { 
	perror("v3_get_fb_data");
	return -1;
    }

    *spec = q.spec;
    
    return 0;
    
}

int v3_have_update(int fd)
{
    struct v3_fb_query_response q;
    int updated;

    q.request_type=V3_FB_UPDATE;
    q.x=0;
    q.y=0;
    q.w=(uint32_t) -1;
    q.h=(uint32_t) -1;

    if (ioctl(fd,V3_VM_FB_QUERY,&q)<0) {
	perror("v3_get_fb_data");
	return -1;
    }

    return q.updated;

}    
