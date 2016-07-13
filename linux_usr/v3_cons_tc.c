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
#include "termkey.h"

static int use_curses = 0;
static int debug_enable = 0;


#define VCONS_EXIT  2
#define VCONS_CTRLC 3

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

static TermKey * tk;


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

    wattron(console.win, COLOR_PAIR(msg->style));
	mvwaddch(console.win, msg->y, msg->x, c);
    wattroff(console.win, COLOR_PAIR(msg->style));

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


int send_key(int cons_fd, char can_code) {

    return 0;
}



void handle_exit(void) {
    if ( debug_enable ) {
        fprintf(stderr, "Exiting from console terminal\n");
    }

    if (use_curses) {
        endwin();
    }

    termkey_destroy(tk);
    //    tcsetattr(STDIN_FILENO, TCSANOW, &console.termios_old);

    // ioctl(STDIN_FILENO, KDSKBMODE, K_XLATE);
}


#define NO_KEY { 0, 0 }

struct key_code {
    unsigned char scan_code;
    unsigned char capital;
};

static const struct key_code ascii_to_key_code[] = {             // ASCII Value Serves as Index
    NO_KEY,         NO_KEY,         {0x50, 0},         {0x48, 0},      // 0x00 - 0x03
    {0x4B, 0},         {0x4D, 0},         NO_KEY,         { 0x0E, 0 }, // 0x04 - 0x07
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

int send_char_to_palacios_as_scancodes(int fd, TermKeyKey * kc)
{
    unsigned char sc;

    if (debug_enable) {
        fprintf(stderr,"key '%c'\n",kc->code.number);
    }

    if (kc->code.number == 'x' && 
        kc->modifiers & TERMKEY_KEYMOD_CTRL && 
        kc->modifiers & TERMKEY_KEYMOD_ALT) {
        return VCONS_EXIT;
    } else if (kc->code.number == 'c' && 
               //kc->modifiers & TERMKEY_KEYMOD_CTRL && 
               kc->modifiers & TERMKEY_KEYMOD_ALT) {
        return VCONS_CTRLC;
    }

    if (kc->code.number<0x80) { 
        struct key_code k = ascii_to_key_code[kc->code.number];

        if (k.scan_code==0 && k.capital==0) { 
            if (debug_enable) { 
                fprintf(stderr,"Cannot send key '%c' to palacios as it maps to no scancode\n",kc->code.number);
            }
        } else {

            if (kc->modifiers & TERMKEY_KEYMOD_CTRL) {
                sc = 0x1d;
                writeit(fd, sc);
            }

            if (kc->modifiers & TERMKEY_KEYMOD_ALT) {
                sc = 0x38;
                writeit(fd, sc);
            }

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

            if (kc->modifiers & TERMKEY_KEYMOD_CTRL) {
                sc = 0x1d | 0x80;   // left ctrl up
                writeit(fd,sc);
            }

            if (kc->modifiers & TERMKEY_KEYMOD_ALT) {
                sc = 0x38 | 0x80; // left alt up
                writeit(fd, sc);
            }
        }

    } else {


        if (debug_enable) { 
            fprintf(stderr,"Cannot send key '%c' to palacios because it is >=0x80\n",kc->code.number);
        }


    }
    return 0;
}


static int handle_fnkey (int fd, TermKeyKey * kc) {

    unsigned char sc;

    switch (kc->code.number) {
        case 0x1:
            sc = 0x3b;
            break;
        case 0x2:
            sc = 0x3c;
            break;
        case 0x3:
            sc = 0x3d;
            break;
        case 0x4:
            sc = 0x3e;
            break;
        case 0x5:
            sc = 0x3f;
            break;
        case 0x6:
            sc = 0x40;
            break;
        case 0x7:
            sc = 0x41;
            break;
        case 0x8:
            sc = 0x42;
            break;
        case 0x9:
            sc = 0x43;
            break;
        case 0xa:
            sc = 0x44;
            break;
        case 0xb:
            // this is a ctrl+c sequence
            if (kc->modifiers & TERMKEY_KEYMOD_CTRL) {
                return VCONS_CTRLC;
            }
            sc = 0x57;
            break;
        case 0xc:
            // this is an exit sequence
            if (kc->modifiers & TERMKEY_KEYMOD_CTRL) {
                return VCONS_EXIT;
            }
            sc = 0x58;
            break;
        default:
            fprintf(stderr, "Unknown function key <%x>\n", kc->code.number);
            break;
    }

    writeit(fd, sc);

    sc |= 0x80;

    writeit(fd, sc);

    return 0;
}

static int handle_symkey (int fd, TermKeyKey * kc) {
    unsigned char sc;

    switch (kc->code.sym) {
        case TERMKEY_SYM_BACKSPACE:
            sc = 0xe;
            break;
        case TERMKEY_SYM_TAB:
            sc = 0xf;
            break;
        case TERMKEY_SYM_ENTER:
            sc = 0x1c;
            break;
        case TERMKEY_SYM_ESCAPE:
            sc = 0x1;
            break;
        case TERMKEY_SYM_SPACE:
            sc = 0x39;
            break;
        case TERMKEY_SYM_INSERT:
            sc = 0x52;
            break;
        case TERMKEY_SYM_DEL:
            sc = 0x53;
            break;
        case TERMKEY_SYM_UP:
            sc = 0x48;
            break;
        case TERMKEY_SYM_DOWN:
            sc = 0x50;
            break;
        case TERMKEY_SYM_LEFT:
            sc = 0x4b;
            break;
        case TERMKEY_SYM_RIGHT:
            sc = 0x4d;
            break;
        case TERMKEY_SYM_PAGEUP:
            sc = 0x49;
            break;
        case TERMKEY_SYM_PAGEDOWN:
            sc = 0x51;
            break;
        case TERMKEY_SYM_HOME:
            sc = 0x47;
            break;
        case TERMKEY_SYM_END:
            sc = 0x4f;
            break;
        case TERMKEY_SYM_KP0:
            sc = 0x70;
            break;
        case TERMKEY_SYM_KP1:
            sc = 0x69;
            break;
        case TERMKEY_SYM_KP2:
            sc = 0x72;
            break;
        case TERMKEY_SYM_KP3:
            sc = 0x7a;
            break;
        case TERMKEY_SYM_KP4:
            sc = 0x6b;
            break;
        case TERMKEY_SYM_KP5:
            sc = 0x73;
            break;
        case TERMKEY_SYM_KP6:
            sc = 0x74;
            break;
        case TERMKEY_SYM_KP7:
            sc = 0x6c;
            break;
        case TERMKEY_SYM_KP8:
            sc = 0x75;
            break;
        case TERMKEY_SYM_KP9:
            sc = 0x7d;
            break;
        case TERMKEY_SYM_KPENTER:
            sc = 0x5a;
            break;
        case TERMKEY_SYM_KPPLUS:
            sc = 0x79;
            break;
        case TERMKEY_SYM_KPMINUS:
            sc = 0x7b;
            break;
        case TERMKEY_SYM_KPMULT:
            sc = 0x7c;
            break;
        case TERMKEY_SYM_KPDIV:
            sc = 0x4a;
            break;
        default:
            fprintf(stderr, "Unknown sym key\n");
            return;
    }

    writeit(fd, sc);

    sc |= 0x80;

    writeit(fd, sc);

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



static void
init_colors (void)
{
    start_color();
    int i;
    for (i = 0; i < 0x100; i++) {
        init_pair(i, i & 0xf, (i >> 4) & 0xf);
    }
}



int main(int argc, char* argv[]) {
    int vm_fd;
    int cons_fd;
    int mouse = 0;
    int mouse_proto = 0;
    char buffer[50];
    char * vm_dev = NULL;
    struct termios termios;
    TERMKEY_CHECK_VERSION;
    TermKeyFormat format = TERMKEY_FORMAT_VIM;
    tk = termkey_new(0, TERMKEY_FLAG_SPACESYMBOL|TERMKEY_FLAG_CTRLC);

    if (!tk) {
        fprintf(stderr, "Could not allocate termkey instance\n");
        exit(EXIT_FAILURE);
    }

    if (termkey_get_flags(tk) & TERMKEY_FLAG_UTF8) {
        if (debug_enable) {
            printf("Termkey in UTF-8 mode\n");
        }
    } else if (termkey_get_flags(tk) & TERMKEY_FLAG_RAW) {
        if (debug_enable) {
            printf("Termkey in RAW mode\n");
        }
    }

    TermKeyResult ret;
    TermKeyKey key;

    if(mouse) {
        printf("\033[?%dhMouse mode active\n", mouse);
        if(mouse_proto)
            printf("\033[?%dh", mouse_proto);
    }

    use_curses = 1;

    if (argc < 2) {
        printf("usage: v3_cons_sc <vm_device>\n\n"
               "NOTE: to use CTRL-C within the terminal, use Alt-C\n"
               "      to exit the terminal, use CTRL-ALT-x\n\n");
        goto exit_err;
    }

    /* Check for minimum Terminal size at start */
    if (0 != check_terminal_size()) {
        printf ("Error: terminal too small!\n");
        goto exit_err;
    }

    vm_dev = argv[1];

    vm_fd = open(vm_dev, O_RDONLY);

    if (vm_fd == -1) {
        printf("Error opening VM device: %s\n", vm_dev);
        goto exit_err;
    }

    cons_fd = ioctl(vm_fd, V3_VM_CONSOLE_CONNECT, NULL); 

    /* Close the file descriptor.  */ 
    close(vm_fd); 

    if (cons_fd < 0) {
        printf("Error opening stream Console\n");
        goto exit_err;
    }

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
        init_colors();
    }


    raw();
    cbreak();
    noecho();
    keypad(console.win, TRUE);

    while (1) {

        int ret; 
        int bytes_read = 0;
        fd_set rset;

        FD_ZERO(&rset);
        FD_SET(cons_fd, &rset);
        FD_SET(STDIN_FILENO, &rset);

        ret = select(cons_fd + 1, &rset, NULL, NULL, NULL);

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

            ret = termkey_waitkey(tk, &key);

            if(ret == TERMKEY_RES_KEY) {

                termkey_strfkey(tk, buffer, sizeof buffer, &key, format);

                switch (key.type) {

                    case TERMKEY_TYPE_UNICODE: {

                           int ret = send_char_to_palacios_as_scancodes(cons_fd, &key);

                           if (ret < 0) {
                               printf("Error sending key to console\n");
                               return -1;
                           } else if (ret == VCONS_CTRLC) {

                               unsigned char sc;
                               sc = 0x1d;  // left ctrl down
                               writeit(cons_fd,sc);
                               sc = 0x2e; // c down
                               writeit(cons_fd,sc);
                               sc = 0x2e | 0x80;   // c up
                               writeit(cons_fd,sc);
                               sc = 0x1d | 0x80;   // left ctrl up
                               writeit(cons_fd,sc);

                           } else if (ret == VCONS_EXIT) {
                               exit(1);
                           }

                           break;
                                               }
                    case TERMKEY_TYPE_FUNCTION:
                           handle_fnkey(cons_fd, &key);
                           break;
                    case TERMKEY_TYPE_KEYSYM:
                           handle_symkey(cons_fd, &key);
                           break;
                    default:
                           fprintf(stderr, "Unknown key type\n");
                }
            }

        }
    }

    erase();

    printf("Console terminated\n");

    close(cons_fd);

    return 0; 

exit_err:
    termkey_destroy(tk);
    return -1;
}


