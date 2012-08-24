/* 
 * V3 Console utility
 * Taken from Palacios console display in MINIX ( by Erik Van der Kouwe )
 * (c) Jack lange, 2010
 * (c) Peter Dinda, 2011 (Scan code encoding)
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h> 
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <curses.h>
#include <termios.h>
#include <linux/kd.h>
#include <linux/keyboard.h>

#include "v3_ctrl.h"

static int use_curses = 0;
static int debug_enable = 0;


typedef enum { CONSOLE_CURS_SET = 1,
	       CONSOLE_CHAR_SET = 2,
	       CONSOLE_SCROLL = 3,
	       CONSOLE_UPDATE = 4,
               CONSOLE_RESOLUTION = 5 } console_op_t;



static struct {
    WINDOW * win;
    int x;
    int y;
    int rows;
    int cols;
    struct termios termios_old;
    unsigned char old_kbd_mode;
} console;


struct cursor_msg {
    int x;
    int y;
} __attribute__((packed));

struct character_msg {
    int x;
    int y;
    char c;
    unsigned char style;
} __attribute__((packed));

struct scroll_msg {
    int lines;
} __attribute__((packed));

struct resolution_msg {
    int cols;
    int rows;
} __attribute__((packed));


struct cons_msg {
    unsigned char op;
    union {
	struct cursor_msg cursor;
	struct character_msg  character;
	struct scroll_msg scroll;
	struct resolution_msg resolution;
    };
} __attribute__((packed)); 




static int handle_char_set(struct character_msg * msg) {
    char c = msg->c;

    if (debug_enable) {
	fprintf(stderr, "setting char (%c), at (x=%d, y=%d)\n", c, msg->x, msg->y);
    }

    if (c == 0) {
	c = ' ';
    }


    if ((c < ' ') || (c >= 127)) {
	if (debug_enable) { 
	    fprintf(stderr, "unexpected control character %d\n", c);
	}
	c = '?';
    }

    if (use_curses) {
	/* clip whatever falls outside the visible area to avoid errors */
	if ((msg->x < 0) || (msg->y < 0) ||
	    (msg->x > console.win->_maxx) || 
	    (msg->y > console.win->_maxy)) {

	    if (debug_enable) { 
		fprintf(stderr, "Char out of range (x=%d,y=%d) MAX:(x=%d,y=%d)\n",
			msg->x, msg->y, console.win->_maxx, console.win->_maxy);
	    }

	    return -1;
	}

	if ((msg->x == console.win->_maxx) &&
	    (msg->y == console.win->_maxy)) {
	    return -1;
	}

	mvwaddch(console.win, msg->y, msg->x, c);

    } else {
	//stdout text display
	while (console.y < msg->y) {
	    printf("\n");
	    console.x = 0;
	    console.y++;
	}

	while (console.x < msg->x) {
	    printf(" ");
	    console.x++;
	}

	printf("%c", c);
	console.x++;

	assert(console.x <= console.cols); 

	if (console.x == console.cols) {
	    printf("\n");
	    console.x = 0;
	    console.y++;
	}
    }

    return 0;
}

int handle_curs_set(struct cursor_msg * msg) {
    if (debug_enable) {
	fprintf(stderr, "cursor set: (x=%d, y=%d)\n", msg->x, msg->y);
    }

    if (use_curses) {
	/* nothing to do now, cursor is set before update to make sure it isn't 
	 * affected by character_set
	 */

	console.x = msg->x;
	console.y = msg->y;
    }
    
    return 0;
}


int handle_scroll(struct scroll_msg * msg) {
    int lines = msg->lines;

    if (debug_enable) {
	fprintf(stderr, "scroll: %d lines\n", lines);
    }


    assert(lines >= 0);

    if (use_curses) {
	while (lines > 0) {
	    scroll(console.win);
	    lines--;
	}
    } else {
	console.y -= lines;	
    }
}

int handle_text_resolution(struct resolution_msg * msg) {
    if (debug_enable) {
	fprintf(stderr, "text resolution: rows=%d, cols=%d\n", msg->rows, msg->cols);
    }


    console.rows = msg->rows;
    console.cols = msg->cols;

    return 0;
}

int handle_update( void ) {
    if (debug_enable) {
	fprintf(stderr, "update\n");
    }    

    if (use_curses) {

	if ( (console.x >= 0) && (console.y >= 0) &&
	     (console.x <= console.win->_maxx) &&
	     (console.y <= console.win->_maxy) ) {

	    wmove(console.win, console.y, console.x);

	}

	wrefresh(console.win);
    } else {
	fflush(stdout);
    }
}


int handle_console_msg(int cons_fd) {
    int ret = 0;
    struct cons_msg msg;

    ret = read(cons_fd, &msg, sizeof(struct cons_msg));

    switch (msg.op) {
	case CONSOLE_CURS_SET:
	    //	    printf("Console cursor set (x=%d, y=%d)\n", msg.cursor.x, msg.cursor.y);
	    handle_curs_set(&(msg.cursor));
	    break;
	case CONSOLE_CHAR_SET:
	    handle_char_set(&(msg.character));
	    /*	    printf("Console character set (x=%d, y=%d, c=%c, style=%c)\n", 
	      msg.character.x, msg.character.y, msg.character.c, msg.character.style);*/
	    break;
	case CONSOLE_SCROLL:
	    //  printf("Console scroll (lines=%d)\n", msg.scroll.lines);
	    handle_scroll(&(msg.scroll));
	    break;
	case CONSOLE_UPDATE:
	    // printf("Console update\n");
	    handle_update();
	    break;
	case CONSOLE_RESOLUTION:
	    handle_text_resolution(&(msg.resolution));
	    break;
	default:
	    printf("Invalid console message operation (%d)\n", msg.op);
	    break;
    }

    return 0;
}


int send_key(int cons_fd, char scan_code) {

    return 0;
}



void handle_exit(void) {
    if ( debug_enable ) {
	fprintf(stderr, "Exiting from console terminal\n");
    }

    if (use_curses) {
	endwin();
    }

    //    tcsetattr(STDIN_FILENO, TCSANOW, &console.termios_old);

    // ioctl(STDIN_FILENO, KDSKBMODE, K_XLATE);
}


#define NO_KEY { 0, 0 }

struct key_code {
    unsigned char scan_code;
    unsigned char capital;
};

static const struct key_code ascii_to_key_code[] = {             // ASCII Value Serves as Index
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x00 - 0x03
    NO_KEY,         NO_KEY,         NO_KEY,         { 0x0E, 0 }, // 0x04 - 0x07
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



#define writeit(fd,c)  do { if (debug_enable) { fprintf(stderr,"scancode 0x%x\n",(c));} if (write((fd),&(c),1)!=1) { return -1; } } while (0)

int send_char_to_palacios_as_scancodes(int fd, unsigned char c)
{
    unsigned char sc;

    if (debug_enable) {
	fprintf(stderr,"key '%c'\n",c);
    }

    if (c<0x80) { 
	struct key_code k = ascii_to_key_code[c];
	
	if (k.scan_code==0 && k.capital==0) { 
	    if (debug_enable) { 
		fprintf(stderr,"Cannot send key '%c' to palacios as it maps to no scancode\n",c);
	    }
	} else {
	    if (k.capital) { 
		//shift down
		sc = 0x2a ; // left shift down
		writeit(fd,sc);
	    }
	    
	    
	    sc = k.scan_code;
	    
	    writeit(fd,sc);  // key down
	    
	    sc |= 0x80;      // key up
	    
	    writeit(fd,sc);
	    
	    if (k.capital) { 
		sc = 0x2a | 0x80;
		writeit(fd,sc);
	    }
	}
	    
    } else {

	
	if (debug_enable) { 
	    fprintf(stderr,"Cannot send key '%c' to palacios because it is >=0x80\n",c);
	}

	
    }
    return 0;
}


#define MIN_TTY_COLS  80
#define MIN_TTY_ROWS  25
int check_terminal_size (void)
{
    unsigned short n_cols = 0;
    unsigned short n_rows = 0;
    struct winsize winsz; 

    ioctl (fileno(stdin), TIOCGWINSZ, &winsz);
    n_cols = winsz.ws_col;
    n_rows = winsz.ws_row;

    if (n_cols < MIN_TTY_COLS || n_rows < MIN_TTY_ROWS) {
        printf ("Your window is not large enough.\n");
        printf ("It must be at least %dx%d, but yours is %dx%d\n",
                MIN_TTY_COLS, MIN_TTY_ROWS, n_cols, n_rows);
    return (-1);
    }

    /* SUCCESS */
    return (0);
}


int main(int argc, char* argv[]) {
    int vm_fd;
    int cons_fd;
    char * vm_dev = NULL;
    struct termios termios;

    use_curses = 1;

    if (argc < 2) {
	printf("usage: v3_cons_sc <vm_device>\n");
	return -1;
    }

    /* Check for minimum Terminal size at start */
    if (0 != check_terminal_size()) {
        printf ("Error: terminal too small!\n");
        return -1;
    }

    vm_dev = argv[1];

    vm_fd = open(vm_dev, O_RDONLY);

    if (vm_fd == -1) {
	printf("Error opening VM device: %s\n", vm_dev);
	return -1;
    }

    cons_fd = ioctl(vm_fd, V3_VM_CONSOLE_CONNECT, NULL); 

    /* Close the file descriptor.  */ 
    close(vm_fd); 

    if (cons_fd < 0) {
	printf("Error opening stream Console\n");
	return -1;
    }

    tcgetattr(STDIN_FILENO, &console.termios_old);
    atexit(handle_exit);

    console.x = 0;
    console.y = 0;


    if (use_curses) {
	gettmode();
	console.win = initscr();
	
	if (console.win == NULL) {
	    fprintf(stderr, "Error initialization curses screen\n");
	    exit(-1);
	}

	scrollok(console.win, 1);

	erase();
    }

    /*
    termios = console.termios_old;
    termios.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | IGNPAR);
    termios.c_iflag &= ~(INLCR | INPCK | ISTRIP | IXOFF | IXON | PARMRK); 
    //termios.c_iflag &= ~(ICRNL | INLCR );    

    //  termios.c_iflag |= SCANCODES; 
    //    termios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL); 
    //termios.c_lflag &= ~(ICANON | IEXTEN | ISIG | NOFLSH);
    termios.c_lflag &= ~(ICANON | ECHO);

    termios.c_cc[VMIN] = 1;
    termios.c_cc[VTIME] = 0;

    tcflush(STDIN_FILENO, TCIFLUSH);
    tcsetattr(STDIN_FILENO, TCSANOW, &termios);
    */    

    raw();
    cbreak();
    noecho();
    keypad(console.win, TRUE);

    //ioctl(STDIN_FILENO, KDSKBMODE, K_RAW);

    while (1) {
	int ret; 
	int bytes_read = 0;
	fd_set rset;

	FD_ZERO(&rset);
	FD_SET(cons_fd, &rset);
	FD_SET(STDIN_FILENO, &rset);

	ret = select(cons_fd + 1, &rset, NULL, NULL, NULL);
	
	//	printf("Returned from select...\n");

	if (ret == 0) {
	    continue;
	} else if (ret == -1) {
	    perror("Select returned error...\n");
	    return -1;
	}

	if (FD_ISSET(cons_fd, &rset)) {
	    if (handle_console_msg(cons_fd) == -1) {
		printf("Console Error\n");
		return -1;
	    }
	}

	if (FD_ISSET(STDIN_FILENO, &rset)) {
	    unsigned char key = getch();

	    if (key == '\\') { // ESC
		break;
	    } else if (key == '`') {
		unsigned char sc = 0x44; // F10
		writeit(cons_fd,sc);
		sc |= 0x80;
		writeit(cons_fd,sc);
            } else if (key == '~') {  // CTRL-C 
                unsigned char sc;
                sc = 0x1d;  // left ctrl down
                writeit(cons_fd,sc);
                sc = 0x2e; // c down
		writeit(cons_fd,sc);
		sc = 0x2e | 0x80;   // c up
                writeit(cons_fd,sc);
                sc = 0x1d | 0x80;   // left ctrl up
                writeit(cons_fd,sc);
            } else {
		if (send_char_to_palacios_as_scancodes(cons_fd,key)) {
		    printf("Error sending key to console\n");
		    return -1;
		}
	    }
	    
	}
    } 

    erase();

    printf("Console terminated\n");

    close(cons_fd);

    return 0; 
}


