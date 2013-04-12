/* Copyright (C) 2002-2004 RealVNC Ltd.  All Rights Reserved.
 *    
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <rfb/Logger_stdio.h>
#include <rfb/LogWriter.h>
#include <rfb/VNCServerST.h>
#include <rfb/Configuration.h>
#include <rfb/SSecurityFactoryStandard.h>

#include <network/TcpSocket.h>

#include "Image.h"
#include <signal.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>


#include <rfb/Encoder.h>


using namespace rfb;
using namespace rdr;
using namespace network;

#include "v3_fb.h"



#define NO_KEY { 0, 0 }

struct key_code {
    uint8_t scan_code;
    uint8_t capital;
};


static const struct key_code ascii_to_key_code[] = {             // ASCII Value Serves as Index
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x00 - 0x03
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x04 - 0x07
    { 0x0E, 0 },    { 0x0F, 0 },    { 0x1C, 0 },    NO_KEY,      // 0x08 - 0x0B
    NO_KEY,         { 0x1C, 0 },    NO_KEY,         NO_KEY,      // 0x0C - 0x0F
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x10 - 0x13
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x14 - 0x17
    NO_KEY,         NO_KEY,         NO_KEY,         { 0x01, 0 }, // 0x18 - 0x1B
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x1C - 0x1F
    { 0x39, 0 },    { 0x02, 1 },    { 0x28, 1 },    { 0x04, 1 }, // 0x20 - 0x23
    { 0x05, 1 },    { 0x06, 1 },    { 0x08, 1 },    { 0x28, 0 }, // 0x24 - 0x27
    { 0x0A, 1 },    { 0x0B, 1 },    { 0x09, 1 },    { 0x0D, 1 }, // 0x28 - 0x2B
    { 0x33, 0 },    { 0x0C, 0 },    { 0x34, 0 },    { 0x35, 0 }, // 0x2C - 0x2F
    { 0x0B, 0 },    { 0x02, 0 },    { 0x03, 0 },    { 0x04, 0 }, // 0x30 - 0x33
    { 0x05, 0 },    { 0x06, 0 },    { 0x07, 0 },    { 0x08, 0 }, // 0x34 - 0x37
    { 0x09, 0 },    { 0x0A, 0 },    { 0x27, 1 },    { 0x27, 0 }, // 0x38 - 0x3B
    { 0x33, 1 },    { 0x0D, 0 },    { 0x34, 1 },    { 0x35, 1 }, // 0x3C - 0x3F
    { 0x03, 1 },    { 0x1E, 1 },    { 0x30, 1 },    { 0x2E, 1 }, // 0x40 - 0x43
    { 0x20, 1 },    { 0x12, 1 },    { 0x21, 1 },    { 0x22, 1 }, // 0x44 - 0x47
    { 0x23, 1 },    { 0x17, 1 },    { 0x24, 1 },    { 0x25, 1 }, // 0x48 - 0x4B
    { 0x26, 1 },    { 0x32, 1 },    { 0x31, 1 },    { 0x18, 1 }, // 0x4C - 0x4F
    { 0x19, 1 },    { 0x10, 1 },    { 0x13, 1 },    { 0x1F, 1 }, // 0x50 - 0x53
    { 0x14, 1 },    { 0x16, 1 },    { 0x2F, 1 },    { 0x11, 1 }, // 0x54 - 0x57
    { 0x2D, 1 },    { 0x15, 1 },    { 0x2C, 1 },    { 0x1A, 0 }, // 0x58 - 0x5B
    { 0x2B, 0 },    { 0x1B, 0 },    { 0x07, 1 },    { 0x0C, 1 }, // 0x5C - 0x5F
    { 0x29, 0 },    { 0x1E, 0 },    { 0x30, 0 },    { 0x2E, 0 }, // 0x60 - 0x63
    { 0x20, 0 },    { 0x12, 0 },    { 0x21, 0 },    { 0x22, 0 }, // 0x64 - 0x67
    { 0x23, 0 },    { 0x17, 0 },    { 0x24, 0 },    { 0x25, 0 }, // 0x68 - 0x6B
    { 0x26, 0 },    { 0x32, 0 },    { 0x31, 0 },    { 0x18, 0 }, // 0x6C - 0x6F
    { 0x19, 0 },    { 0x10, 0 },    { 0x13, 0 },    { 0x1F, 0 }, // 0x70 - 0x73
    { 0x14, 0 },    { 0x16, 0 },    { 0x2F, 0 },    { 0x11, 0 }, // 0x74 - 0x77
    { 0x2D, 0 },    { 0x15, 0 },    { 0x2C, 0 },    { 0x1A, 1 }, // 0x78 - 0x7B
    { 0x2B, 1 },    { 0x1B, 1 },    { 0x29, 1 },    { 0x0E, 0 }  // 0x7C - 0x7F
};

static uint8_t convert_to_scancode(rdr::U32 key, bool down)
{
    

    unsigned char scancode=0;

    if (key<0x80) { 
	struct key_code c = ascii_to_key_code[key];
	scancode=c.scan_code;
    } else {
	switch (key) { 
	    case 0xffe1:  //left shift
		scancode = 0x2a;
		break;

	    case 0xffe2:  //right shift
		scancode = 0x36;
		break;

	    case 0xffe3:  //left ctrl
	    case 0xffe4:  //right ctrl
		scancode = 0x1d;   // translated as left ctrl
		break;

	    case 0xffe7:  //left meta
	    case 0xffe8:  //right meta
	    case 0xffe9:  //left alt
	    case 0xffea:  //right alt
		scancode = 0x38;  // translated as a left alt
		break; 

	    case 0xff08: // backspace
		scancode = 0x0e;
		break; 

	    case 0xff09: // tab
		scancode = 0x0f;  
		break; 

	    case 0xff0d: // return
		scancode = 0x1c;
		break; 

	    case 0xff1b: // escape
		scancode = 0x01;
		break; 

	    case 0xff63: // insert
		scancode = 0x52;
		break; 

	    case 0xffff: // delete
		scancode = 0x53;
		break; 

	    case 0xff50: // home
		scancode = 0x47;
		break; 

	    case 0xff57: // end
		scancode = 0x4f;
		break; 
		
	    case 0xff55: // pageup
		scancode = 0x49;
		break; 

	    case 0xff56: // pagedown
		scancode = 0x51;
		break; 

	    case 0xff51: // left
		scancode = 0x4b;
		break; 

	    case 0xff52: // up
		scancode = 0x48;
		break; 

	    case 0xff53: // right
		scancode = 0x4d;
		break; 

	    case 0xff54: // down
		scancode = 0x50;
		break; 

	    case 0xffbe: // f1
		scancode = 0x3b;
		break; 
	    case 0xffbf: // f2
		scancode = 0x3c;
		break; 
	    case 0xffc0: // f3
		scancode = 0x3d;
		break; 
	    case 0xffc1: // f4
		scancode = 0x3e;
		break; 
	    case 0xffc2: // f5
		scancode = 0x3f;
		break; 
	    case 0xffc3: // f6
		scancode = 0x40;
		break; 
	    case 0xffc4: // f7
		scancode = 0x41;
		break; 
	    case 0xffc5: // f8
		scancode = 0x42;
		break; 
	    case 0xffc6: // f9
		scancode = 0x43;
		break;
	    case 0xffc7: // f10
		scancode = 0x44;
		break; 
	    case 0xffc8: // f11
		scancode = 0x57;
		break; 
	    case 0xffc9: // f12
		scancode = 0x58;
		break; 


	    default:
		scancode = 0;
		fprintf(stderr,"Ignoring key 0x%x (down=%d)\n", key, down);
	}
    }
    
    if (scancode==0) { 
	return 0;
    }

    if (!down) { 
	scancode|=0x80;
    }
    
    return scancode;
}

	
	
	
    



LogWriter vlog("main");

StringParameter displayname("display", "The X display", "");
IntParameter rfbport("rfbport", "TCP port to listen for RFB protocol",5900);
StringParameter geometry("geometry", "Height and width", "1024x768");

VncAuthPasswdFileParameter vncAuthPasswdFile;

static void CleanupSignalHandler(int sig)
{
  // CleanupSignalHandler allows C++ object cleanup to happen because it calls
  // exit() rather than the default which is to abort.
  fprintf(stderr,"CleanupSignalHandler called\n");
  exit(1);
}


inline int sign(int x) { return x<0; }

inline int abs(int x) { if (x<0) { return -x; } else {return x;}}

class V3Desktop : public SDesktop, public rfb::ColourMap
{
public:
    V3Desktop(int fd)  // fd is the open fd of the VM's device
    : v3_fd(fd), pb(0), server(0), oldButtonMask(0)
    {

	mouse_inited=false; // not yet
    
      // Let's get the needed resolution
      if (v3_get_fb_spec(v3_fd,&v3_spec)) { 
	  fprintf(stderr, "Can't get spec from VM\n");
	  exit(-1);
      }

      // sanity check:
      fprintf(stderr,"VM's spec is %u x %u x %u with %u bits per channel and rgb offsets (%u,%u,%u)\n",
	      v3_spec.width, v3_spec.height, v3_spec.bytes_per_pixel, 
	      v3_spec.bits_per_channel, v3_spec.red_offset, v3_spec.green_offset, v3_spec.blue_offset);

      if (! (v3_spec.bytes_per_pixel==4 &&
	     v3_spec.bits_per_channel==8)) { 
	  fprintf(stderr,"Error in forma compatabiliity\n");
	  exit(-1);
      }

      dpyWidth=v3_spec.width;
      dpyHeight=v3_spec.height;
      
      // Now we can build our image to match
      image = new Image(dpyWidth, dpyHeight);

      // Convert to the internal pixel format a
      pf.bpp = v3_spec.bytes_per_pixel*8;
      pf.depth = v3_spec.bits_per_channel*3;
      pf.bigEndian = 0;
      pf.trueColour = 1;
    
      //default pixel formats dont work!
      pf.redShift   = v3_spec.red_offset*8;
      pf.greenShift = v3_spec.green_offset*8;
      pf.blueShift  = v3_spec.blue_offset*8;
      pf.redMax     = 255;
      pf.greenMax   = 255;
      pf.blueMax    = 255;
    
      // assigns new pixelbuffer to xdesktop object
      pb = new FullFramePixelBuffer(pf, dpyWidth, dpyHeight,
				    (rdr::U8*)image->data, this);
    }

    virtual ~V3Desktop() {
	delete pb;
	delete image;
    }
    
    void setVNCServer(VNCServer* s) {
	server = s;
	server->setPixelBuffer(pb);
    }
    
    // -=- SDesktop interface, worry about the pointer and key events later..

    // Mouse events in VNC give absolute pixel coordinates of the mouse pointer
    // the PS/2 mouse spec means we provide relative movements with +/- 256
    // these relative movements can then be scaled by the mouse or by the software
    // therefore, we will need to trans back from absolute to relative, perhaps
    // converting one VNC event to multiple PS/2 events
    // we assume here that the translation is 1:1
    // 
    virtual void pointerEvent(const Point& pos, rdr::U8 buttonMask) {
	//vlog.info("Pointer event occurred, x position: %d, y position: %d; button mask: %d.", pos.x, pos.y, buttonMask); 
	int dx, dy;
	int incx;
	int incy;


	if (!mouse_inited) {
	    mouse_inited = true;
	    dx = pos.x;
	    dy = pos.y;
	} else {
	    // delta from current position
	    dx = pos.x - mouse_lastpos.x ;
	    dy = pos.y - mouse_lastpos.y ;
	}

	// update last position
	mouse_lastpos = pos;

#define MAXINC 32

	// dx and dy are now +/- 2^16
	// we can generate increments of up to +/- MAXINC;

	while (dx || dy) { 
	    incx = min(MAXINC, abs(dx));
	    incy = min(MAXINC, abs(dy));

	    if (v3_send_mouse(v3_fd, sign(dx), incx, sign(dy), incy, buttonMask)) { 
		fprintf(stderr, "Error in sending mouse event\n");
		exit(-1);
	    }
	    
	    dx += (dx>=0) ? -incx : +incx;
	    dy += (dy>=0) ? -incy : +incy;
	}
    }
    
    virtual void keyEvent(rdr::U32 key, bool down) {
	vlog.info("Key event received (key=%d, down=%d.",key,down);
    
	uint8_t scan_code = convert_to_scancode(key,down);

	if (scan_code && v3_send_key(v3_fd,scan_code)) {
	    fprintf(stderr, "Error in sending key event\n");
	    exit(-1);
	}
	
    }
  
    virtual void clientCutText(const char* str, int len) {
    }
    
    virtual Point getFbSize() {
	return Point(pb->width(), pb->height());
    }

    virtual void lookup(int index, int* r, int* g, int* b) {
    
	// Probably not important since we will use true-color

	/* X implementation..
	   XColor xc;
	   xc.pixel = index;
	   if (index < DisplayCells(dpy,DefaultScreen(dpy))) {
	   XQueryColor(dpy, DefaultColormap(dpy,DefaultScreen(dpy)), &xc);
	   } else {
	   xc.red = xc.green = xc.blue = 0;
	   }
	   *r = xc.red;
	   *g = xc.green;
	   *b = xc.blue;
	   */
    }
    
    virtual void poll() {
	if (server && 
	    server->clientsReadyForUpdate() && 
	    v3_have_update(v3_fd)) {

	    struct v3_frame_buffer_spec newspec = v3_spec;

	    if (v3_get_fb_data(v3_fd,&newspec,image->data)<0) { 
		fprintf(stderr,"Failed to get fb data from VM\n");
		exit(-1);
	    }

	    if (memcmp(&newspec,&v3_spec,sizeof(struct v3_frame_buffer_spec))) { 
		fprintf(stderr,"Uh oh - screen spec has changed\n");
		exit(-1);
	    }
	    
	    fprintf(stderr,"render!\n");

	    server->add_changed(pb->getRect());
	    server->tryUpdate();
	}
    }
    
protected:
    int v3_fd;
    struct v3_frame_buffer_spec v3_spec;
    bool mouse_inited;
    Point mouse_lastpos;

    PixelFormat pf;
    PixelBuffer* pb;
    VNCServer* server;
    Image* image;
    int oldButtonMask;
    bool haveXtest;
    int dpyHeight;
    int dpyWidth;
};

char* programName;

static void usage()
{
  fprintf(stderr, "\nusage: %s [<parameters>] /dev/v3-vmN \n", programName);
  fprintf(stderr,"\n"
          "Parameters can be turned on with -<param> or off with -<param>=0\n"
          "Parameters which take a value can be specified as "
          "-<param> <value>\n"
          "Other valid forms are <param>=<value> -<param>=<value> "
          "--<param>=<value>\n"
          "Parameter names are case-insensitive.  The parameters are:\n\n");
  Configuration::listParams(79, 14);
  exit(1);
}

int main(int argc, char** argv)
{
  int v3_fd;
  char *v3_dev;

  initStdIOLoggers();
  LogWriter::setLogParams("*:stderr:30");

  programName = argv[0];

  // Grab the v3 device before all the OO stuff goes inscrutable on us
  if (argc<2) { 
      usage();
  }
  v3_dev = argv[argc-1];
  argc--;
  fprintf(stderr,"Will attempt to connect to VM %s\n",v3_dev);




  Display* dpy;

  for (int i = 1; i < argc; i++) {
    if (Configuration::setParam(argv[i]))
      continue;

    if (argv[i][0] == '-') {
      if (i+1 < argc) {
        if (Configuration::setParam(&argv[i][1], argv[i+1])) {
          i++;
          continue;
        }
      }
      usage();
    }

    usage();
  }

  CharArray dpyStr(displayname.getData());
  dpy = NULL;

  signal(SIGHUP, CleanupSignalHandler);
  signal(SIGINT, CleanupSignalHandler);
  signal(SIGTERM, CleanupSignalHandler);

  try {
    v3_fd = open(v3_dev, O_RDONLY);    

    if (v3_fd<0) { 
	perror("Cannot open VM");
	exit(-1);
    }
  
    V3Desktop desktop(v3_fd);
    VNCServerST server("v3x0vncserver", &desktop);
    desktop.setVNCServer(&server);

    TcpSocket::initTcpSockets();
    TcpListener listener((int)rfbport);
    vlog.info("Listening on port %d", (int)rfbport);
    
    while (true) {
      fd_set rfds;
      struct timeval tv;

      tv.tv_sec = 0;
      tv.tv_usec = 50*1000;

      FD_ZERO(&rfds);
      FD_SET(listener.getFd(), &rfds);

      std::list<Socket*> sockets;
      server.getSockets(&sockets);
      std::list<Socket*>::iterator i;
      for (i = sockets.begin(); i != sockets.end(); i++) {
        FD_SET((*i)->getFd(), &rfds);
      }

      int n = select(FD_SETSIZE, &rfds, 0, 0, &tv);
      if (n < 0) throw rdr::SystemException("select",errno);

      if (FD_ISSET(listener.getFd(), &rfds)) {
        Socket* sock = listener.accept();
        server.addClient(sock);
      }

      server.getSockets(&sockets);
      for (i = sockets.begin(); i != sockets.end(); i++) {
        if (FD_ISSET((*i)->getFd(), &rfds)) {
          server.processSocketEvent(*i);
        }
      }

      server.checkTimeouts();
      desktop.poll();
    }

  } catch (rdr::Exception &e) {
    vlog.error(e.str());
    close(v3_fd);
  };

  close(v3_fd);

  return 0;
}
