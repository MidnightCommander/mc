/* Keyboard support routines.

   Copyright (C) 1994,1995 the Free Software Foundation.

   Written by: 1994, 1995 Miguel de Icaza.
               1994, 1995 Janne Kukonlehto.
	       1995  Jakub Jelinek.
	       1997  Norbert Warmuth
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#include <ctype.h>
#include <errno.h>

#include "global.h"
#include "tty.h"
#include "mouse.h"
#include "key.h"
#include "main.h"
#include "win.h"
#include "cons.saver.h"
#include "../vfs/vfs.h"

#ifdef HAVE_TEXTMODE_X11_SUPPORT
#include <X11/Xlib.h>
#endif

#ifdef __linux__
#    if defined(__GLIBC__) && (__GLIBC__ < 2)
#        include <linux/termios.h>	/* This is needed for TIOCLINUX */
#    else
#        include <termios.h>
#    endif
#    include <sys/ioctl.h>
#endif

#define GET_TIME(tv)    (gettimeofday(&tv, (struct timezone *)NULL))
#define DIF_TIME(t1,t2) ((t2.tv_sec -t1.tv_sec) *1000+ \
			 (t2.tv_usec-t1.tv_usec)/1000)
			 
/* timeout for old_esc_mode in usec */
#define ESCMODE_TIMEOUT 1000000

/* Linux console keyboard modifiers */
#define SHIFT_PRESSED 1
#define ALTR_PRESSED 2
#define CONTROL_PRESSED 4
#define ALTL_PRESSED 8


int mou_auto_repeat = 100;
int double_click_speed = 250;
int old_esc_mode = 0;

int use_8th_bit_as_meta = 1;

typedef struct key_def {
    char ch;			/* Holds the matching char code */
    int code;			/* The code returned, valid if child == NULL */
    struct key_def *next;
    struct key_def *child;	/* sequence continuation */
    int action;			/* optional action to be done. Now used only
                                   to mark that we are just after the first
                                   Escape */
} key_def;

/* This holds all the key definitions */
static key_def *keys = 0;

static int input_fd;
static int disabled_channels = 0; /* Disable channels checking */
static int xgetch_second (void);
static int get_modifier (void);

/* File descriptor monitoring add/remove routines */
typedef struct SelectList {
    int fd;
    select_fn callback;
    void *info;
    struct SelectList *next;
} SelectList;

static SelectList *select_list = 0;

void add_select_channel (int fd, select_fn callback, void *info)
{
    SelectList *new;

    new = g_new (SelectList, 1);
    new->fd = fd;
    new->callback = callback;
    new->info = info;
    new->next = select_list;
    select_list = new;
}

void delete_select_channel (int fd)
{
    SelectList *p = select_list;
    SelectList *p_prev = 0;
    SelectList *p_next;

    while (p) {
	if (p->fd == fd) {
	    p_next = p->next;

	    if (p_prev)
		p_prev->next = p_next;
	    else
		select_list = p_next;

	    g_free (p);
	    p = p_next;
	    continue;
	}

	p_prev = p;
	p = p->next;
    }
}

inline static int add_selects (fd_set *select_set)
{
    SelectList *p;
    int        top_fd = 0;

    if (disabled_channels)
	return 0;
    
    for (p = select_list; p; p = p->next){
	FD_SET (p->fd, select_set);
	if (p->fd > top_fd)
	    top_fd = p->fd;
    }
    return top_fd;
}

static void check_selects (fd_set *select_set)
{
    SelectList *p;

    if (disabled_channels)
	return;
    
    for (p = select_list; p; p = p->next)
	if (FD_ISSET (p->fd, select_set))
	    (*p->callback)(p->fd, p->info);
}

void channels_down (void)
{
    disabled_channels ++;
}

void channels_up (void)
{
    if (!disabled_channels)
	fprintf (stderr,
		 "Error: channels_up called with disabled_channels = 0\n");
    disabled_channels--;
}

typedef const struct {
    int code;
    char *seq;
    int action;
} key_define_t;

static key_define_t mc_bindings [] = {
    { KEY_END,    ESC_STR ">", MCKEY_NOACTION },
    { KEY_HOME,   ESC_STR "<", MCKEY_NOACTION },
    { 0, 0, MCKEY_NOACTION },
};

/* Broken terminfo and termcap databases on xterminals */
static key_define_t xterm_key_defines [] = {
    { KEY_F(1),   ESC_STR "OP",   MCKEY_NOACTION },
    { KEY_F(2),   ESC_STR "OQ",   MCKEY_NOACTION },
    { KEY_F(3),   ESC_STR "OR",   MCKEY_NOACTION },
    { KEY_F(4),   ESC_STR "OS",   MCKEY_NOACTION },
    { KEY_F(1),   ESC_STR "[11~", MCKEY_NOACTION },
    { KEY_F(2),   ESC_STR "[12~", MCKEY_NOACTION },
    { KEY_F(3),   ESC_STR "[13~", MCKEY_NOACTION },
    { KEY_F(4),   ESC_STR "[14~", MCKEY_NOACTION },
    { KEY_F(5),   ESC_STR "[15~", MCKEY_NOACTION },
    { KEY_F(6),   ESC_STR "[17~", MCKEY_NOACTION },
    { KEY_F(7),   ESC_STR "[18~", MCKEY_NOACTION },
    { KEY_F(8),   ESC_STR "[19~", MCKEY_NOACTION },
    { KEY_F(9),   ESC_STR "[20~", MCKEY_NOACTION },
    { KEY_F(10),  ESC_STR "[21~", MCKEY_NOACTION },

    /* xterm keys with modifiers */
    { KEY_M_SHIFT | KEY_UP,    ESC_STR "O2A",   MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN,  ESC_STR "O2B",   MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "O2C",   MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT,  ESC_STR "O2D",   MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_PPAGE, ESC_STR "[5;5~", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_NPAGE, ESC_STR "[6;5~", MCKEY_NOACTION },

    /* rxvt keys with modifiers */
    { KEY_M_SHIFT | KEY_UP,    ESC_STR "[a",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN,  ESC_STR "[b",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "[c",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT,  ESC_STR "[d",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_PPAGE, ESC_STR "[5^", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_NPAGE, ESC_STR "[6^", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_HOME,  ESC_STR "[7^", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_END,   ESC_STR "[8^", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_HOME,  ESC_STR "[7$", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_END,   ESC_STR "[8$", MCKEY_NOACTION },

    { 0, 0, MCKEY_NOACTION },
};

static key_define_t mc_default_keys [] = {
    { ESC_CHAR,	ESC_STR, MCKEY_ESCAPE },
    { ESC_CHAR, ESC_STR ESC_STR, MCKEY_NOACTION },
    { 0, 0, MCKEY_NOACTION },
};

static void
define_sequences (key_define_t *kd)
{
    int i;
    
    for (i = 0; kd [i].code; i++)
	define_sequence(kd [i].code, kd [i].seq, kd [i].action);
}

#ifdef HAVE_TEXTMODE_X11_SUPPORT
static Display *x11_display;
static Window x11_window;
#endif /* HAVE_TEXTMODE_X11_SUPPORT */

/* This has to be called before slang_init or whatever routine
   calls any define_sequence */
void init_key (void)
{
    char *term = (char *) getenv ("TERM");
    
    /* This has to be the first define_sequence */
    /* So, we can assume that the first keys member has ESC */
    define_sequences (mc_default_keys);
    
    /* Terminfo on irix does not have some keys */
    if ((!strncmp (term, "iris-ansi", 9)) || (!strncmp (term, "xterm", 5)))
	define_sequences (xterm_key_defines);
    
    define_sequences (mc_bindings);
    
    /* load some additional keys (e.g. direct Alt-? support) */
    load_xtra_key_defines();
    
#ifdef __QNX__
    if (strncmp(term, "qnx", 3) == 0){
	/* Modify the default value of use_8th_bit_as_meta: we would
	 * like to provide a working mc for a newbie who knows nothing
	 * about [Options|Display bits|Full 8 bits input]...
	 * 
	 * Don't use 'meta'-bit, when we are dealing with a 
	 * 'qnx*'-type terminal: clear the default value!
	 * These terminal types use 0xFF as an escape character,
	 * so use_8th_bit_as_meta==1 must not be enabled!
	 * 
	 * [mc-4.1.21+,slint.c/getch(): the DEC_8BIT_HACK stuff
	 * is not used now (doesn't even depend on use_8th_bit_as_meta
	 * as in mc-3.1.2)...GREAT!...no additional code is required!]
	 */
	use_8th_bit_as_meta = 0;
    }
#endif /* __QNX__ */

#ifdef HAVE_TEXTMODE_X11_SUPPORT
    x11_display = XOpenDisplay (0);
    if (x11_display)
        x11_window = DefaultRootWindow (x11_display);
#endif /* HAVE_TEXTMODE_X11_SUPPORT */
}

/* This has to be called after SLang_init_tty/slint_init */
void init_key_input_fd (void)
{
#ifdef HAVE_SLANG
    input_fd = SLang_TT_Read_FD;
#endif
}


static void
xmouse_get_event (Gpm_Event *ev)
{
    int btn;
    static struct timeval tv1 = { 0, 0 }; /* Force first click as single */
    static struct timeval tv2;
    static int clicks;
    static int last_btn = 0;

    /* Decode Xterm mouse information to a GPM style event */

    /* Variable btn has following meaning: */
    /* 0 = btn1 dn, 1 = btn2 dn, 2 = btn3 dn, 3 = btn up */
    btn = getch () - 32;
    
    /* There seems to be no way of knowing which button was released */
    /* So we assume all the buttons were released */

    if (btn == 3){
	if (last_btn) {
	    ev->type = GPM_UP | (GPM_SINGLE << clicks);
	    ev->buttons = 0;
	    last_btn = 0;
	    GET_TIME (tv1);
	    clicks = 0;
	} else {
	    /* Bogus event, maybe mouse wheel */
	    ev->type = 0;
	}
    } else {
        ev->type = GPM_DOWN;
	GET_TIME (tv2);
	if (tv1.tv_sec && (DIF_TIME (tv1,tv2) < double_click_speed)){
	    clicks++;
	    clicks %= 3;
	} else
	    clicks = 0;
	
        switch (btn) {
	case 0:
            ev->buttons = GPM_B_LEFT;
            break;
	case 1:
            ev->buttons = GPM_B_MIDDLE;
            break;
	case 2:
            ev->buttons = GPM_B_RIGHT;
            break;
	case 64:
            ev->buttons = GPM_B_UP;
            break;
	case 65:
            ev->buttons = GPM_B_DOWN;
            break;
	default:
            /* Nothing */
	    ev->type = 0;
	    ev->buttons = 0;
            break;
        }
	last_btn = ev->buttons;
    }
    /* Coordinates are 33-based */
    /* Transform them to 1-based */
    ev->x = getch () - 32;
    ev->y = getch () - 32;
}

static key_def *create_sequence (char *seq, int code, int action)
{
    key_def *base, *p, *attach;

    for (base = attach = NULL; *seq; seq++){
	p = g_new (key_def, 1);
	if (!base) base = p;
	if (attach) attach->child = p;
	
	p->ch   = *seq;
	p->code = code;
	p->child = p->next = NULL;
	if (!seq[1])
	    p->action = action;
	else
	    p->action = MCKEY_NOACTION;
	attach = p;
    }
    return base;
}

/* The maximum sequence length (32 + null terminator) */
#define SEQ_BUFFER_LEN 33
static int seq_buffer [SEQ_BUFFER_LEN];
static int *seq_append = 0;

static int push_char (int c)
{
    if (!seq_append)
	seq_append = seq_buffer;
    
    if (seq_append == &(seq_buffer [SEQ_BUFFER_LEN-2]))
	return 0;
    *(seq_append++) = c;
    *seq_append = 0;
    return 1;
}

/*
 * Return 1 on success, 0 on error.
 * An error happens if SEQ is a beginning of an existing longer sequence.
 */
int define_sequence (int code, char *seq, int action)
{
    key_def *base;

    if (strlen (seq) > SEQ_BUFFER_LEN-1)
	return 0;
    
    for (base = keys; (base != 0) && *seq; ){
	if (*seq == base->ch){
	    if (base->child == 0){
		if (*(seq+1)){
		    base->child = create_sequence (seq+1, code, action);
		    return 1;
		} else {
		    /* The sequence matches an existing one.  */
                    base->code = code;
                    base->action = action;
		    return 1;
		}
	    } else {
		base = base->child;
		seq++;
	    }
	} else {
	    if (base->next)
		base = base->next;
	    else {
		base->next = create_sequence (seq, code, action);
		return 1;
	    }
	}
    }

    if (!*seq) {
	/* Attempt to redefine a sequence with a shorter sequence.  */
	return 0;
    }

    keys = create_sequence (seq, code, action);
    return 1;
}

static int *pending_keys;

/* Apply corrections for the keycode generated in get_key_code() */
static int
correct_key_code (int code)
{
    unsigned int c = code & ~KEY_M_MASK;	/* code without modifier */
    unsigned int mod = code & KEY_M_MASK;	/* modifier */

    /* Add key modifiers directly from X11 or OS */
    mod |= get_modifier ();

    /* This is needed if the newline is reported as carriage return */
    if (c == '\r')
	c = '\n';

    /* This is reported to be useful on AIX */
    if (c == KEY_SCANCEL)
	c = '\t';

    /* Convert Shift+Tab and Ctrl+Tab to Back Tab */
    if ((c == '\t') && (mod & (KEY_M_SHIFT | KEY_M_CTRL))) {
	c = KEY_BTAB;
	mod = 0;
    }

    /* F0 is the same as F10 for out purposes */
    if (c == KEY_F (0))
	c = KEY_F (10);

    /*
     * We are not interested if Ctrl was pressed when entering control
     * characters, so assume that it was.  When checking for such keys,
     * XCTRL macro should be used.  In some cases, we are interested,
     * e.g. to distinguish Ctrl-Enter from Enter.
     */
    if (c < 32 && c != ESC_CHAR && c != '\t' && c != '\n') {
	mod |= KEY_M_CTRL;
    }

    /* Remove Shift and Ctrl information from ordinary characters */
    if (c >= 32 && c < 256) {
	mod &= ~(KEY_M_SHIFT | KEY_M_CTRL);
    }

    /* Convert Shift+Fn to F(n+10) */
    if (c >= KEY_F (1) && c <= KEY_F (10) && (mod & KEY_M_SHIFT)) {
	c += 10;
    }

    /* Remove Shift information from function keys */
    if (c >= KEY_F (1) && c <= KEY_F (20)) {
	mod &= ~KEY_M_SHIFT;
    }

    if (!alternate_plus_minus)
	switch (c) {
	case KEY_KP_ADD:
	    c = '+';
	    break;
	case KEY_KP_SUBTRACT:
	    c = '-';
	    break;
	case KEY_KP_MULTIPLY:
	    c = '*';
	    break;
	}

    return (mod | c);
}

int get_key_code (int no_delay)
{
    int c;
    static key_def *this = NULL, *parent;
    static struct timeval esctime = { -1, -1 };
    static int lastnodelay = -1;
    
    if (no_delay != lastnodelay) {
        this = NULL;
        lastnodelay = no_delay;
    }

 pend_send:
    if (pending_keys){
	int d = *pending_keys++;
 check_pend:
	if (!*pending_keys){
	    pending_keys = 0;
	    seq_append = 0;
	}
	if (d == ESC_CHAR && pending_keys){
	    d = ALT(*pending_keys++);
	    goto check_pend;
	}
	if ((d & 0x80) && use_8th_bit_as_meta)
	    d = ALT(d & 0x7f);
	this = NULL;
	return correct_key_code (d);
    }

 nodelay_try_again:
    if (no_delay) {
        nodelay (stdscr, TRUE);
    }
    c = getch ();
#if defined(USE_NCURSES) && defined(KEY_RESIZE)
    if (c == KEY_RESIZE)
        goto nodelay_try_again;
#endif
    if (no_delay) {
        nodelay (stdscr, FALSE);
        if (c == -1) {
            if (this != NULL && parent != NULL && 
                parent->action == MCKEY_ESCAPE && old_esc_mode) {
                struct timeval current, timeout;
                
                if (esctime.tv_sec == -1)
                    return -1;
                GET_TIME (current);
                timeout.tv_sec = ESCMODE_TIMEOUT / 1000000 + esctime.tv_sec;
                timeout.tv_usec = ESCMODE_TIMEOUT % 1000000 + esctime.tv_usec;
                if (timeout.tv_usec > 1000000) {
                    timeout.tv_usec -= 1000000;
                    timeout.tv_sec++;
                }
                if (current.tv_sec < timeout.tv_sec)
                    return -1;
                if (current.tv_sec == timeout.tv_sec && 
                    current.tv_usec < timeout.tv_usec)
                    return -1;
                this = NULL;
		pending_keys = seq_append = NULL;
		return ESC_CHAR;
            }
            return -1;
        }
    } else if (c == -1){
	/* Maybe we got an incomplete match.
	   This we do only in delay mode, since otherwise
	   getch can return -1 at any time. */
	if (seq_append) {
	    pending_keys = seq_buffer;
	    goto pend_send;
	}
	this = NULL;
	return -1;
    }
    
    /* Search the key on the root */
    if (!no_delay || this == NULL) {
        this = keys;
        parent = NULL;

        if ((c & 0x80) && use_8th_bit_as_meta) {
            c &= 0x7f;

	    /* The first sequence defined starts with esc */
	    parent = keys;
	    this = keys->child;
        }
    }
    while (this){
	if (c == this->ch){
	    if (this->child){
		if (!push_char (c)){
		    pending_keys = seq_buffer;
		    goto pend_send;
		}
		parent = this;
		this = this->child;
		if (parent->action == MCKEY_ESCAPE && old_esc_mode) {
		    if (no_delay) {
		        GET_TIME (esctime);
		        if (this == NULL) {
		            /* Shouldn't happen */
		            fprintf (stderr, "Internal error\n");
		            exit (1);
		        }
		        goto nodelay_try_again;
		    }
		    esctime.tv_sec = -1;
		    c = xgetch_second ();
		    if (c == -1) {
		        pending_keys = seq_append = NULL;
		        this = NULL;
		        return ESC_CHAR;
		    }
		} else {
		    if (no_delay)
		        goto nodelay_try_again;
		    c = getch ();
		}
	    } else {
		/* We got a complete match, return and reset search */
		int code;
		
		pending_keys = seq_append = NULL;
		code = this->code;
		this = NULL;
		return correct_key_code (code);
	    }
	} else {
	    if (this->next)
		this = this->next;
	    else {
	        if (parent != NULL && parent->action == MCKEY_ESCAPE) {
	            /* This is just to save a lot of define_sequences */
	            if (isalpha(c)
			|| (c == '\n') || (c == '\t') || (c == XCTRL('h'))
			|| (c == KEY_BACKSPACE) || (c == '!') || (c == '\r')
			|| c == 127 || c == '+' || c == '-' || c == '\\' 
			|| c == '?')
			c = ALT(c);
		    else if (isdigit(c))
	                c = KEY_F (c-'0');
		    else if (c == ' ')
			c = ESC_CHAR;
		    pending_keys = seq_append = NULL;
		    this = NULL;
		    return correct_key_code (c);
		}
		/* Did not find a match or {c} was changed in the if above,
		   so we have to return everything we had skipped
		 */
		push_char (c);
		pending_keys = seq_buffer;
		goto pend_send;
	    }
	}
    }
    this = NULL;
    return correct_key_code (c);
}

/* If set timeout is set, then we wait 0.1 seconds, else, we block */
static void
try_channels (int set_timeout)
{
    struct timeval timeout;
    static fd_set select_set;
    struct timeval *timeptr;
    int v;
    int maxfdp;

    while (1){
	FD_ZERO (&select_set);
	FD_SET  (input_fd, &select_set);	/* Add stdin */
	maxfdp = max (add_selects (&select_set), input_fd);
	
	if (set_timeout){
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 100000;
	    timeptr = &timeout;
	} else
	    timeptr = 0;

	v = select (maxfdp + 1, &select_set, NULL, NULL, timeptr);
	if (v > 0){
	    check_selects (&select_set);
	    if (FD_ISSET (input_fd, &select_set))
		return;
	}
    }
}

/* Workaround for System V Curses vt100 bug */
static int getch_with_delay (void)
{
    int c;

    /* This routine could be used on systems without mouse support,
       so we need to do the select check :-( */
    while (1){
	if (!pending_keys)
	    try_channels (0);

	/* Try to get a character */
	c = get_key_code (0);
	if (c != -1)
	    break;
	/* Failed -> wait 0.1 secs and try again */
	try_channels (1);
    }
    /* Success -> return the character */
    return c;
}

extern int max_dirt_limit;

/* Returns a character read from stdin with appropriate interpretation */
/* Also takes care of generated mouse events */
/* Returns EV_MOUSE if it is a mouse event */
/* Returns EV_NONE  if non-blocking or interrupt set and nothing was done */
int
get_event (Gpm_Event * event, int redo_event, int block)
{
    int c;
    static int flag;		/* Return value from select */
#ifdef HAVE_LIBGPM
    static Gpm_Event ev;	/* Mouse event */
#endif
    struct timeval timeout;
    struct timeval *time_addr = NULL;
    static int dirty = 3;

    if ((dirty == 3) || is_idle ()) {
	mc_refresh ();
	doupdate ();
	dirty = 1;
    } else
	dirty++;

    vfs_timeout_handler ();

    /* Ok, we use (event->x < 0) to signal that the event does not contain
       a suitable position for the mouse, so we can't use show_mouse_pointer
       on it.
     */
    if (event->x > 0) {
	show_mouse_pointer (event->x, event->y);
	if (!redo_event)
	    event->x = -1;
    }

    /* Repeat if using mouse */
    while (mouse_enabled && !pending_keys) {
	int maxfdp;
	fd_set select_set;

	FD_ZERO (&select_set);
	FD_SET (input_fd, &select_set);
	maxfdp = max (add_selects (&select_set), input_fd);

#ifdef HAVE_LIBGPM
	if (use_mouse_p == MOUSE_GPM) {
	    if (gpm_fd == -1) {
		/* Connection to gpm broken, possibly gpm has died */
		mouse_enabled = 0;
		use_mouse_p = MOUSE_NONE;
		break;
	    }
	    FD_SET (gpm_fd, &select_set);
	    maxfdp = max (maxfdp, gpm_fd);
	}
#endif

	if (redo_event) {
	    timeout.tv_usec = mou_auto_repeat * 1000;
	    timeout.tv_sec = 0;

	    time_addr = &timeout;
	} else {
	    int seconds;

	    if ((seconds = vfs_timeouts ())) {
		/* the timeout could be improved and actually be
		 * the number of seconds until the next vfs entry
		 * timeouts in the stamp list.
		 */

		timeout.tv_sec = seconds;
		timeout.tv_usec = 0;
		time_addr = &timeout;
	    } else
		time_addr = NULL;
	}

	if (!block) {
	    time_addr = &timeout;
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 0;
	}
	enable_interrupt_key ();
	flag = select (maxfdp + 1, &select_set, NULL, NULL, time_addr);
	disable_interrupt_key ();

	/* select timed out: it could be for any of the following reasons:
	 * redo_event -> it was because of the MOU_REPEAT handler
	 * !block     -> we did not block in the select call
	 * else       -> 10 second timeout to check the vfs status.
	 */
	if (flag == 0) {
	    if (redo_event)
		return EV_MOUSE;
	    if (!block)
		return EV_NONE;
	    vfs_timeout_handler ();
	}
	if (flag == -1 && errno == EINTR)
	    return EV_NONE;

	check_selects (&select_set);

	if (FD_ISSET (input_fd, &select_set))
	    break;
#ifdef HAVE_LIBGPM
	if (use_mouse_p == MOUSE_GPM && FD_ISSET (gpm_fd, &select_set)) {
	    Gpm_GetEvent (&ev);
	    Gpm_FitEvent (&ev);
	    *event = ev;
	    return EV_MOUSE;
	}
#endif				/* !HAVE_LIBGPM */
    }
#ifndef HAVE_SLANG
    flag = is_wintouched (stdscr);
    untouchwin (stdscr);
#endif				/* !HAVE_SLANG */
    c = block ? getch_with_delay () : get_key_code (1);

#ifndef HAVE_SLANG
    if (flag)
	touchwin (stdscr);
#endif				/* !HAVE_SLANG */

    if (c == MCKEY_MOUSE
#ifdef KEY_MOUSE
	|| c == KEY_MOUSE
#endif				/* KEY_MOUSE */
	) {
	/* Mouse event */
	xmouse_get_event (event);
	if (event->type)
	    return EV_MOUSE;
	else
	    return EV_NONE;
    }

    return c;
}

/* Returns a key press, mouse events are discarded */
int mi_getch ()
{
    Gpm_Event ev;
    int       key;

    ev.x = -1;
    while ((key = get_event (&ev, 0, 1)) == EV_NONE)
	;
    return key;
}

static int xgetch_second (void)
{
    fd_set Read_FD_Set;
    int c;
    struct timeval timeout;

    timeout.tv_sec = ESCMODE_TIMEOUT / 1000000;
    timeout.tv_usec = ESCMODE_TIMEOUT % 1000000;
    nodelay (stdscr, TRUE);
    FD_ZERO (&Read_FD_Set);
    FD_SET (input_fd, &Read_FD_Set);
    select (input_fd + 1, &Read_FD_Set, NULL, NULL, &timeout);
    c = getch ();
    nodelay (stdscr, FALSE);
    return c;
}

static void
learn_store_key (char *buffer, char **p, int c)
{
    if (*p - buffer > 253)
        return;
    if (c == ESC_CHAR) {
        *(*p)++ = '\\';
        *(*p)++ = 'e';
    } else if (c < ' ') {
    	*(*p)++ = '^';
    	*(*p)++ = c + 'a' - 1;
    } else if (c == '^') {
    	*(*p)++ = '^';
    	*(*p)++ = '^';
    } else
    	*(*p)++ = (char) c;
}

char *learn_key (void)
{
/* LEARN_TIMEOUT in usec */
#define LEARN_TIMEOUT 200000

    fd_set Read_FD_Set;
    struct timeval endtime;
    struct timeval timeout;
    int c;
    char buffer [256];
    char *p = buffer;
    
    keypad(stdscr, FALSE); /* disable intepreting keys by ncurses */
    c = getch ();
    while (c == -1)
        c = getch (); /* Sanity check, should be unnecessary */
    learn_store_key (buffer, &p, c);
    GET_TIME (endtime);
    endtime.tv_usec += LEARN_TIMEOUT;
    if (endtime.tv_usec > 1000000) {
        endtime.tv_usec -= 1000000;
        endtime.tv_sec++;
    }
    nodelay (stdscr, TRUE);
    for (;;) {
        while ((c = getch ()) == -1) {
            GET_TIME (timeout);
            timeout.tv_usec = endtime.tv_usec - timeout.tv_usec;
            if (timeout.tv_usec < 0)
                timeout.tv_sec++;
            timeout.tv_sec = endtime.tv_sec - timeout.tv_sec;
            if (timeout.tv_sec >= 0 && timeout.tv_usec > 0) {
    		FD_ZERO (&Read_FD_Set);
    		FD_SET (input_fd, &Read_FD_Set);
                select (input_fd + 1, &Read_FD_Set, NULL, NULL, &timeout);
            } else
            	break;
        }
        if (c == -1)
            break;
	learn_store_key (buffer, &p, c);
    }
    keypad(stdscr, TRUE);
    nodelay (stdscr, FALSE);
    *p = 0;
    return g_strdup (buffer);
}

/* xterm and linux console only: set keypad to numeric or application
   mode. Only in application keypad mode it's possible to distinguish
   the '+' key and the '+' on the keypad ('*' and '-' ditto)*/
void
numeric_keypad_mode (void)
{
    if (console_flag || xterm_flag) {
        fprintf (stdout, "\033>");
        fflush (stdout);
    }
}

void
application_keypad_mode (void)
{
    if (console_flag || xterm_flag) {
        fprintf (stdout, "\033="); 
        fflush (stdout);
    }
}


/*
 * Check if we are idle, i.e. there are no pending keyboard or mouse
 * events.  Return 1 is idle, 0 is there are pending events.
 */
int
is_idle (void)
{
    int maxfdp;
    fd_set select_set;
    struct timeval timeout;

    FD_ZERO (&select_set);
    FD_SET (input_fd, &select_set);
    maxfdp = input_fd;
#ifdef HAVE_LIBGPM
    if (use_mouse_p == MOUSE_GPM && mouse_enabled && gpm_fd != -1) {
	FD_SET (gpm_fd, &select_set);
	maxfdp = max (maxfdp, gpm_fd);
    }
#endif
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    select (maxfdp, &select_set, 0, 0, &timeout);
    return !FD_ISSET (0, &select_set);
}


/*
 * Get modifier state (shift, alt, ctrl) for the last key pressed.
 * We are assuming that the state didn't change since the key press.
 * This is only correct if get_modifier() is called very fast after
 * the input was received, so that the user didn't release the
 * modifier keys yet.
 */
static int
get_modifier (void)
{
    int result = 0;

#ifdef HAVE_TEXTMODE_X11_SUPPORT
    if (x11_display) {
	Window root, child;
	int root_x, root_y;
	int win_x, win_y;
	unsigned int mask;

	XQueryPointer (x11_display, x11_window, &root, &child, &root_x,
		       &root_y, &win_x, &win_y, &mask);

	if (mask & ShiftMask)
	    result |= KEY_M_SHIFT;
	if (mask & ControlMask)
	    result |= KEY_M_CTRL;
	return result;
    }
#endif
#ifdef __linux__
    {
	unsigned char modifiers = 6;

	if (ioctl (0, TIOCLINUX, &modifiers) < 0)
	    return 0;

	/* Translate Linux modifiers into mc modifiers */
	if (modifiers & SHIFT_PRESSED)
	    result |= KEY_M_SHIFT;
	if (modifiers & (ALTL_PRESSED | ALTR_PRESSED))
	    result |= KEY_M_ALT;
	if (modifiers & CONTROL_PRESSED)
	    result |= KEY_M_CTRL;

	return result;
    }
#else
    return 0;
#endif
}

static void k_dispose (key_def *k)
{
    if (!k)
	return;
    k_dispose (k->child);
    k_dispose (k->next);
    g_free (k);
}

static void s_dispose (SelectList *sel)
{
    if (!sel)
	return;

    s_dispose (sel->next);
    g_free (sel);
}

void done_key ()
{
    k_dispose (keys);
    s_dispose (select_list);

#ifdef HAVE_TEXTMODE_X11_SUPPORT
    if (x11_display)
	XCloseDisplay (x11_display);
#endif /* HAVE_TEXTMODE_X11_SUPPORT */
}
