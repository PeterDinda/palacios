/* 
 * V3 Console utility
 * Taken from Palacios console display in MINIX ( by Erik Van der Kouwe )
 * (c) Jack lange, 2010
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
	fprintf(stderr, "unexpected control character %d\n", c);
	c = '?';
    }

    if (use_curses) {
	/* clip whatever falls outside the visible area to avoid errors */
	if ((msg->x < 0) || (msg->y < 0) ||
	    (msg->x > console.win->_maxx) || 
	    (msg->y > console.win->_maxy)) {

	    fprintf(stderr, "Char out of range (x=%d,y=%d) MAX:(x=%d,y=%d)\n",
		    msg->x, msg->y, console.win->_maxx, console.win->_maxy);
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
    fprintf(stderr, "Exiting from console terminal\n");

    if (use_curses) {
	endwin();
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &console.termios_old);

    ioctl(STDIN_FILENO, KDSKBMODE, K_XLATE);
}

int main(int argc, char* argv[]) {
    int vm_fd;
    int cons_fd;
    char * vm_dev = NULL;
    struct termios termios;

    use_curses = 1;

    if (argc < 2) {
	printf("Usage: ./v3_cons <vm_device>\n");
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
	//cbreak();
    noecho();
    keypad(console.win, TRUE);

    ioctl(STDIN_FILENO, KDSKBMODE, K_RAW);

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

	    if (key == 0x01) { // ESC
		break;
	    }

	    if (write(cons_fd, &key, 1) != 1) {
		fprintf(stderr, "ERrror sendign key to console\n");
		return -1;
	    }
	    
	}
    } 

    close(cons_fd);

    return 0; 
}


