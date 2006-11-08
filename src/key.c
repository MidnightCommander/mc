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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include "global.h"
#include "tty.h"
#include "mouse.h"
#include "key.h"
#include "layout.h"	/* winch_flag */
#include "main.h"
#include "win.h"
#include "cons.saver.h"

#ifdef USE_VFS
#include "../vfs/gc.h"
#endif

#ifdef HAVE_TEXTMODE_X11_SUPPORT
#    include "x11conn.h"
#endif

#ifdef __linux__
#    if defined(__GLIBC__) && (__GLIBC__ < 2)
#        include <linux/termios.h>	/* TIOCLINUX */
#    else
#        include <termios.h>
#    endif
#    include <sys/ioctl.h>
#endif				/* __linux__ */

#ifdef __CYGWIN__
#    include <termios.h>
#    include <sys/ioctl.h>
#endif                         /* __CYGWIN__ */

#ifdef __QNXNTO__
#	include <dlfcn.h>
#	include <Ph.h>
#	include <sys/dcmd_chr.h>
#endif

#define GET_TIME(tv)    (gettimeofday(&tv, (struct timezone *)NULL))
#define DIF_TIME(t1,t2) ((t2.tv_sec -t1.tv_sec) *1000+ \
			 (t2.tv_usec-t1.tv_usec)/1000)

/* Linux console keyboard modifiers */
#define SHIFT_PRESSED		(1 << 0)
#define ALTR_PRESSED		(1 << 1)
#define CONTROL_PRESSED		(1 << 2)
#define ALTL_PRESSED		(1 << 3)

int mou_auto_repeat = 100;
int double_click_speed = 250;
int old_esc_mode = 0;
/* timeout for old_esc_mode in usec */
int keyboard_key_timeout = 1000000;	/* settable via env */

int use_8th_bit_as_meta = 0;

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
static key_def *keys = NULL;

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

#ifdef __QNXNTO__
    typedef int (*ph_dv_f) (void *, void *);
    typedef int (*ph_ov_f) (void *);
    typedef int (*ph_pqc_f) (unsigned short, PhCursorInfo_t *);
    ph_dv_f ph_attach;
    ph_ov_f ph_input_group;
    ph_pqc_f ph_query_cursor;
#endif

static SelectList *select_list = NULL;

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
    SelectList *p_prev = NULL;
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

    for (p = select_list; p; p = p->next) {
	FD_SET (p->fd, select_set);
	if (p->fd > top_fd)
	    top_fd = p->fd;
    }
    return top_fd;
}

static void check_selects (fd_set *select_set)
{
    SelectList *p;
    gboolean retry;

    if (disabled_channels)
	return;

    do {
	retry = FALSE;
	for (p = select_list; p; p = p->next)
	    if (FD_ISSET (p->fd, select_set)) {
		FD_CLR (p->fd, select_set);
		(*p->callback)(p->fd, p->info);
		retry = TRUE;
		break;
	    }
    } while (retry);
}

void channels_down (void)
{
    disabled_channels++;
}

void channels_up (void)
{
    if (!disabled_channels)
	fputs ("Error: channels_up called with disabled_channels = 0\n",
	       stderr);
    disabled_channels--;
}

typedef const struct {
    int code;
    const char *seq;
    int action;
} key_define_t;

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

    /* old xterm Shift-arrows */
    { KEY_M_SHIFT | KEY_UP,    ESC_STR "O2A",   MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN,  ESC_STR "O2B",   MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "O2C",   MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT,  ESC_STR "O2D",   MCKEY_NOACTION },

    /* new xterm Shift-arrows */
    { KEY_M_SHIFT | KEY_UP,    ESC_STR "[1;2A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN,  ESC_STR "[1;2B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "[1;2C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT,  ESC_STR "[1;2D", MCKEY_NOACTION },

    /* more xterm keys with modifiers */
    { KEY_M_CTRL  | KEY_PPAGE, ESC_STR "[5;5~", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_NPAGE, ESC_STR "[6;5~", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_IC,    ESC_STR "[2;5~", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_DC,    ESC_STR "[3;5~", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_HOME,  ESC_STR "[1;5H", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_END,   ESC_STR "[1;5F", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_HOME,  ESC_STR "[1;2H", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_END,   ESC_STR "[1;2F", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_UP,    ESC_STR "[1;5A", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_DOWN,  ESC_STR "[1;5B", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_RIGHT, ESC_STR "[1;5C", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_LEFT,  ESC_STR "[1;5D", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_IC,    ESC_STR "[2;2~", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DC,    ESC_STR "[3;2~", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_UP,    ESC_STR "[1;6A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN,  ESC_STR "[1;6B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "[1;6C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT,  ESC_STR "[1;6D", MCKEY_NOACTION },

    /* rxvt keys with modifiers */
    { KEY_M_SHIFT | KEY_UP,    ESC_STR "[a",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN,  ESC_STR "[b",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "[c",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT,  ESC_STR "[d",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_UP,    ESC_STR "Oa",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_DOWN,  ESC_STR "Ob",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_RIGHT, ESC_STR "Oc",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_LEFT,  ESC_STR "Od",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_PPAGE, ESC_STR "[5^", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_NPAGE, ESC_STR "[6^", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_HOME,  ESC_STR "[7^", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_END,   ESC_STR "[8^", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_HOME,  ESC_STR "[7$", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_END,   ESC_STR "[8$", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_IC,    ESC_STR "[2^", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_DC,    ESC_STR "[3^", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DC,    ESC_STR "[3$", MCKEY_NOACTION },

    /* konsole keys with modifiers */
    { KEY_M_SHIFT | KEY_HOME,  ESC_STR "O2H",   MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_END,   ESC_STR "O2F",   MCKEY_NOACTION },

    /* gnome-terminal */
    { KEY_M_SHIFT | KEY_UP,    ESC_STR "[2A",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN,  ESC_STR "[2B",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "[2C",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT,  ESC_STR "[2D",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_UP,    ESC_STR "[5A",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_DOWN,  ESC_STR "[5B",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_RIGHT, ESC_STR "[5C",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_LEFT,  ESC_STR "[5D",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_UP,    ESC_STR "[6A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN,  ESC_STR "[6B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "[6C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT,  ESC_STR "[6D", MCKEY_NOACTION },

    /* gnome-terminal - application mode */
    { KEY_M_CTRL  | KEY_UP,    ESC_STR "O5A",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_DOWN,  ESC_STR "O5B",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_RIGHT, ESC_STR "O5C",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_LEFT,  ESC_STR "O5D",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_UP,    ESC_STR "O6A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN,  ESC_STR "O6B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "O6C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT,  ESC_STR "O6D", MCKEY_NOACTION },

    /* iTerm */
    { KEY_M_SHIFT | KEY_PPAGE, ESC_STR "[5;2~", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_NPAGE, ESC_STR "[6;2~", MCKEY_NOACTION },

    /* keypad keys */
    { KEY_IC,                  ESC_STR "Op",  MCKEY_NOACTION },
    { KEY_DC,                  ESC_STR "On",  MCKEY_NOACTION },
    { '/',                     ESC_STR "Oo",  MCKEY_NOACTION },
    { '\n',                    ESC_STR "OM",  MCKEY_NOACTION },

    { 0, 0, MCKEY_NOACTION },
};

/* qansi-m terminals have a much more key combinatios,
   which are undefined in termcap/terminfo */
static key_define_t qansi_key_defines[] =
{
    /* qansi-m terminal */
    {KEY_M_CTRL | KEY_NPAGE,   ESC_STR "[u", MCKEY_NOACTION}, /* Ctrl-PgDown */
    {KEY_M_CTRL | KEY_PPAGE,   ESC_STR "[v", MCKEY_NOACTION}, /* Ctrl-PgUp   */
    {KEY_M_CTRL | KEY_HOME,    ESC_STR "[h", MCKEY_NOACTION}, /* Ctrl-Home   */
    {KEY_M_CTRL | KEY_END,     ESC_STR "[y", MCKEY_NOACTION}, /* Ctrl-End    */
    {KEY_M_CTRL | KEY_IC,      ESC_STR "[`", MCKEY_NOACTION}, /* Ctrl-Insert */
    {KEY_M_CTRL | KEY_DC,      ESC_STR "[p", MCKEY_NOACTION}, /* Ctrl-Delete */
    {KEY_M_CTRL | KEY_LEFT,    ESC_STR "[d", MCKEY_NOACTION}, /* Ctrl-Left   */
    {KEY_M_CTRL | KEY_RIGHT,   ESC_STR "[c", MCKEY_NOACTION}, /* Ctrl-Right  */
    {KEY_M_CTRL | KEY_DOWN,    ESC_STR "[b", MCKEY_NOACTION}, /* Ctrl-Down   */
    {KEY_M_CTRL | KEY_UP,      ESC_STR "[a", MCKEY_NOACTION}, /* Ctrl-Up     */
    {KEY_M_CTRL | KEY_KP_ADD,  ESC_STR "[s", MCKEY_NOACTION}, /* Ctrl-Gr-Plus */
    {KEY_M_CTRL | KEY_KP_SUBTRACT, ESC_STR "[t", MCKEY_NOACTION}, /* Ctrl-Gr-Minus */
    {KEY_M_CTRL  | '\t',       ESC_STR "[z", MCKEY_NOACTION}, /* Ctrl-Tab    */
    {KEY_M_SHIFT | '\t',       ESC_STR "[Z", MCKEY_NOACTION}, /* Shift-Tab   */
    {KEY_M_CTRL | KEY_F(1),    ESC_STR "[1~", MCKEY_NOACTION}, /* Ctrl-F1    */
    {KEY_M_CTRL | KEY_F(2),    ESC_STR "[2~", MCKEY_NOACTION}, /* Ctrl-F2    */
    {KEY_M_CTRL | KEY_F(3),    ESC_STR "[3~", MCKEY_NOACTION}, /* Ctrl-F3    */
    {KEY_M_CTRL | KEY_F(4),    ESC_STR "[4~", MCKEY_NOACTION}, /* Ctrl-F4    */
    {KEY_M_CTRL | KEY_F(5),    ESC_STR "[5~", MCKEY_NOACTION}, /* Ctrl-F5    */
    {KEY_M_CTRL | KEY_F(6),    ESC_STR "[6~", MCKEY_NOACTION}, /* Ctrl-F6    */
    {KEY_M_CTRL | KEY_F(7),    ESC_STR "[7~", MCKEY_NOACTION}, /* Ctrl-F7    */
    {KEY_M_CTRL | KEY_F(8),    ESC_STR "[8~", MCKEY_NOACTION}, /* Ctrl-F8    */
    {KEY_M_CTRL | KEY_F(9),    ESC_STR "[9~", MCKEY_NOACTION}, /* Ctrl-F9    */
    {KEY_M_CTRL | KEY_F(10),   ESC_STR "[10~", MCKEY_NOACTION}, /* Ctrl-F10  */
    {KEY_M_CTRL | KEY_F(11),   ESC_STR "[11~", MCKEY_NOACTION}, /* Ctrl-F11  */
    {KEY_M_CTRL | KEY_F(12),   ESC_STR "[12~", MCKEY_NOACTION}, /* Ctrl-F12  */
    {KEY_M_ALT  | KEY_F(1),    ESC_STR "[17~", MCKEY_NOACTION}, /* Alt-F1    */
    {KEY_M_ALT  | KEY_F(2),    ESC_STR "[18~", MCKEY_NOACTION}, /* Alt-F2    */
    {KEY_M_ALT  | KEY_F(3),    ESC_STR "[19~", MCKEY_NOACTION}, /* Alt-F3    */
    {KEY_M_ALT  | KEY_F(4),    ESC_STR "[20~", MCKEY_NOACTION}, /* Alt-F4    */
    {KEY_M_ALT  | KEY_F(5),    ESC_STR "[21~", MCKEY_NOACTION}, /* Alt-F5    */
    {KEY_M_ALT  | KEY_F(6),    ESC_STR "[22~", MCKEY_NOACTION}, /* Alt-F6    */
    {KEY_M_ALT  | KEY_F(7),    ESC_STR "[23~", MCKEY_NOACTION}, /* Alt-F7    */
    {KEY_M_ALT  | KEY_F(8),    ESC_STR "[24~", MCKEY_NOACTION}, /* Alt-F8    */
    {KEY_M_ALT  | KEY_F(9),    ESC_STR "[25~", MCKEY_NOACTION}, /* Alt-F9    */
    {KEY_M_ALT  | KEY_F(10),   ESC_STR "[26~", MCKEY_NOACTION}, /* Alt-F10   */
    {KEY_M_ALT  | KEY_F(11),   ESC_STR "[27~", MCKEY_NOACTION}, /* Alt-F11   */
    {KEY_M_ALT  | KEY_F(12),   ESC_STR "[28~", MCKEY_NOACTION}, /* Alt-F12   */
    {KEY_M_ALT  | 'a',         ESC_STR "Na",   MCKEY_NOACTION}, /* Alt-a     */
    {KEY_M_ALT  | 'b',         ESC_STR "Nb",   MCKEY_NOACTION}, /* Alt-b     */
    {KEY_M_ALT  | 'c',         ESC_STR "Nc",   MCKEY_NOACTION}, /* Alt-c     */
    {KEY_M_ALT  | 'd',         ESC_STR "Nd",   MCKEY_NOACTION}, /* Alt-d     */
    {KEY_M_ALT  | 'e',         ESC_STR "Ne",   MCKEY_NOACTION}, /* Alt-e     */
    {KEY_M_ALT  | 'f',         ESC_STR "Nf",   MCKEY_NOACTION}, /* Alt-f     */
    {KEY_M_ALT  | 'g',         ESC_STR "Ng",   MCKEY_NOACTION}, /* Alt-g     */
    {KEY_M_ALT  | 'i',         ESC_STR "Ni",   MCKEY_NOACTION}, /* Alt-i     */
    {KEY_M_ALT  | 'j',         ESC_STR "Nj",   MCKEY_NOACTION}, /* Alt-j     */
    {KEY_M_ALT  | 'k',         ESC_STR "Nk",   MCKEY_NOACTION}, /* Alt-k     */
    {KEY_M_ALT  | 'l',         ESC_STR "Nl",   MCKEY_NOACTION}, /* Alt-l     */
    {KEY_M_ALT  | 'm',         ESC_STR "Nm",   MCKEY_NOACTION}, /* Alt-m     */
    {KEY_M_ALT  | 'n',         ESC_STR "Nn",   MCKEY_NOACTION}, /* Alt-n     */
    {KEY_M_ALT  | 'o',         ESC_STR "No",   MCKEY_NOACTION}, /* Alt-o     */
    {KEY_M_ALT  | 'p',         ESC_STR "Np",   MCKEY_NOACTION}, /* Alt-p     */
    {KEY_M_ALT  | 'q',         ESC_STR "Nq",   MCKEY_NOACTION}, /* Alt-r     */
    {KEY_M_ALT  | 's',         ESC_STR "Ns",   MCKEY_NOACTION}, /* Alt-s     */
    {KEY_M_ALT  | 't',         ESC_STR "Nt",   MCKEY_NOACTION}, /* Alt-t     */
    {KEY_M_ALT  | 'u',         ESC_STR "Nu",   MCKEY_NOACTION}, /* Alt-u     */
    {KEY_M_ALT  | 'v',         ESC_STR "Nv",   MCKEY_NOACTION}, /* Alt-v     */
    {KEY_M_ALT  | 'w',         ESC_STR "Nw",   MCKEY_NOACTION}, /* Alt-w     */
    {KEY_M_ALT  | 'x',         ESC_STR "Nx",   MCKEY_NOACTION}, /* Alt-x     */
    {KEY_M_ALT  | 'y',         ESC_STR "Ny",   MCKEY_NOACTION}, /* Alt-y     */
    {KEY_M_ALT  | 'z',         ESC_STR "Nz",   MCKEY_NOACTION}, /* Alt-z     */
    {KEY_KP_SUBTRACT,          ESC_STR "[S",   MCKEY_NOACTION}, /* Gr-Minus  */
    {KEY_KP_ADD,               ESC_STR "[T",   MCKEY_NOACTION}, /* Gr-Plus   */
    {0, NULL, MCKEY_NOACTION},
};

static key_define_t mc_default_keys [] = {
    { ESC_CHAR,	ESC_STR, MCKEY_ESCAPE },
    { ESC_CHAR, ESC_STR ESC_STR, MCKEY_NOACTION },
    { 0, NULL, MCKEY_NOACTION },
};

static void
define_sequences (key_define_t *kd)
{
    int i;

    for (i = 0; kd[i].code != 0; i++)
	define_sequence(kd[i].code, kd[i].seq, kd[i].action);
}

#ifdef HAVE_TEXTMODE_X11_SUPPORT

static Display *x11_display;
static Window x11_window;

static void
init_key_x11 (void)
{
    if (!getenv ("DISPLAY"))
	return;

    x11_display = mc_XOpenDisplay (0);

    if (x11_display)
	x11_window = DefaultRootWindow (x11_display);
}
#endif				/* HAVE_TEXTMODE_X11_SUPPORT */


/* This has to be called before slang_init or whatever routine
   calls any define_sequence */
void
init_key (void)
{
    const char *term = getenv ("TERM");
    char *kt = getenv ("KEYBOARD_KEY_TIMEOUT_US");
    if (kt != NULL)
	keyboard_key_timeout = atoi (kt);

    /* This has to be the first define_sequence */
    /* So, we can assume that the first keys member has ESC */
    define_sequences (mc_default_keys);

    /* Terminfo on irix does not have some keys */
    if (xterm_flag
	|| (term != NULL
	    && (strncmp (term, "iris-ansi", 9) == 0
		|| strncmp (term, "xterm", 5) == 0
		|| strncmp (term, "rxvt", 4) == 0
		|| strcmp (term, "screen") == 0)))
	define_sequences (xterm_key_defines);

    /* load some additional keys (e.g. direct Alt-? support) */
    load_xtra_key_defines ();

#ifdef __QNX__
    if (term && strncmp (term, "qnx", 3) == 0) {
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
#endif				/* __QNX__ */

#ifdef HAVE_TEXTMODE_X11_SUPPORT
    init_key_x11 ();
#endif				/* HAVE_TEXTMODE_X11_SUPPORT */

    /* Load the qansi-m key definitions
       if we are running under the qansi-m terminal */
    if (term != NULL && (strncmp (term, "qansi-m", 7) == 0)) {
	define_sequences (qansi_key_defines);
    }
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

    if (btn == 3) {
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
	if (btn >= 32 && btn <= 34) {
	    btn -= 32;
	    ev->type = GPM_DRAG;
	} else
	    ev->type = GPM_DOWN;

	GET_TIME (tv2);
	if (tv1.tv_sec && (DIF_TIME (tv1,tv2) < double_click_speed)) {
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
	    clicks = 0;
	    break;
	case 65:
	    ev->buttons = GPM_B_DOWN;
	    clicks = 0;
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

static key_def *create_sequence (const char *seq, int code, int action)
{
    key_def *base, *p, *attach;

    for (base = attach = NULL; *seq; seq++) {
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
int define_sequence (int code, const char *seq, int action)
{
    key_def *base;

    if (strlen (seq) > SEQ_BUFFER_LEN-1)
	return 0;

    for (base = keys; (base != 0) && *seq; ) {
	if (*seq == base->ch) {
	    if (base->child == 0) {
		if (*(seq+1)) {
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
    #ifdef __QNXNTO__
       unsigned int qmod;                       /* bunch of the QNX console
						   modifiers needs unchanged */
    #endif /* __QNXNTO__ */

    /*
     * Add key modifiers directly from X11 or OS.
     * Ordinary characters only get modifiers from sequences.
     */
    if (c < 32 || c >= 256) {
	mod |= get_modifier ();
    }

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

#ifdef __QNXNTO__
    qmod=get_modifier();

    if ((c == 127) && (mod==0)) /* Add Ctrl/Alt/Shift-BackSpace */
    {
	mod |= get_modifier();
	c = KEY_BACKSPACE;
    }

    if ((c=='0') && (mod==0)) /* Add Shift-Insert on key pad */
    {
	if ((qmod & KEY_M_SHIFT) == KEY_M_SHIFT)
	{
	   mod = KEY_M_SHIFT;
	   c = KEY_IC;
	}
    }

    if ((c=='.') && (mod==0)) /* Add Shift-Del on key pad */
    {
	if ((qmod & KEY_M_SHIFT) == KEY_M_SHIFT)
	{
	   mod = KEY_M_SHIFT;
	   c = KEY_DC;
	}
    }
#endif /* __QNXNTO__ */

    /* Unrecognized 0177 is delete (preserve Ctrl) */
    if (c == 0177) {
	c = KEY_BACKSPACE;
    }

    /* Unrecognized Ctrl-d is delete */
    if (c == (31 & 'd')) {
	c = KEY_DC;
	mod &= ~KEY_M_CTRL;
    }

    /* Unrecognized Ctrl-h is backspace */
    if (c == (31 & 'h')) {
	c = KEY_BACKSPACE;
	mod &= ~KEY_M_CTRL;
    }

    /* Shift+BackSpace is backspace */
    if (c == KEY_BACKSPACE && (mod & KEY_M_SHIFT)) {
	mod &= ~KEY_M_SHIFT;
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
    if (pending_keys) {
	int d = *pending_keys++;
 check_pend:
	if (!*pending_keys) {
	    pending_keys = 0;
	    seq_append = 0;
	}
	if (d == ESC_CHAR && pending_keys) {
	    d = ALT(*pending_keys++);
	    goto check_pend;
	}
	if ((d > 127 && d < 256) && use_8th_bit_as_meta)
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
		timeout.tv_sec = keyboard_key_timeout / 1000000 + esctime.tv_sec;
		timeout.tv_usec = keyboard_key_timeout % 1000000 + esctime.tv_usec;
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
    } else if (c == -1) {
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

	if ((c > 127 && c < 256) && use_8th_bit_as_meta) {
	    c &= 0x7f;

	    /* The first sequence defined starts with esc */
	    parent = keys;
	    this = keys->child;
	}
    }
    while (this) {
	if (c == this->ch) {
	    if (this->child) {
		if (!push_char (c)) {
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
			    fputs ("Internal error\n", stderr);
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

		    /* Convert escape-digits to F-keys */
		    if (isdigit(c))
			c = KEY_F (c - '0');
		    else if (c == ' ')
			c = ESC_CHAR;
		    else
			c = ALT(c);

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

    while (1) {
	FD_ZERO (&select_set);
	FD_SET  (input_fd, &select_set);	/* Add stdin */
	maxfdp = max (add_selects (&select_set), input_fd);

	if (set_timeout) {
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 100000;
	    timeptr = &timeout;
	} else
	    timeptr = 0;

	v = select (maxfdp + 1, &select_set, NULL, NULL, timeptr);
	if (v > 0) {
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
    while (1) {
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

/* Returns a character read from stdin with appropriate interpretation */
/* Also takes care of generated mouse events */
/* Returns EV_MOUSE if it is a mouse event */
/* Returns EV_NONE  if non-blocking or interrupt set and nothing was done */
int
get_event (struct Gpm_Event *event, int redo_event, int block)
{
    int c;
    static int flag;		/* Return value from select */
#ifdef HAVE_LIBGPM
    static struct Gpm_Event ev;	/* Mouse event */
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
	    if (gpm_fd < 0) {
		/* Connection to gpm broken, possibly gpm has died */
		mouse_enabled = 0;
		use_mouse_p = MOUSE_NONE;
		break;
	    } else {
		FD_SET (gpm_fd, &select_set);
		maxfdp = max (maxfdp, gpm_fd);
	    }
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

	if (!block || winch_flag) {
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
	    if (!block || winch_flag)
		return EV_NONE;
	    vfs_timeout_handler ();
	}
	if (flag == -1 && errno == EINTR)
	    return EV_NONE;

	check_selects (&select_set);

	if (FD_ISSET (input_fd, &select_set))
	    break;
#ifdef HAVE_LIBGPM
	if (use_mouse_p == MOUSE_GPM && gpm_fd > 0
	    && FD_ISSET (gpm_fd, &select_set)) {
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

    timeout.tv_sec = keyboard_key_timeout / 1000000;
    timeout.tv_usec = keyboard_key_timeout % 1000000;
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
	fputs ("\033>", stdout);
	fflush (stdout);
    }
}

void
application_keypad_mode (void)
{
    if (console_flag || xterm_flag) {
	fputs ("\033=", stdout);
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
    if (use_mouse_p == MOUSE_GPM && mouse_enabled && gpm_fd > 0) {
	FD_SET (gpm_fd, &select_set);
	maxfdp = max (maxfdp, gpm_fd);
    }
#endif
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return (select (maxfdp + 1, &select_set, 0, 0, &timeout) <= 0);
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
#ifdef __QNXNTO__
    int mod_status, shift_ext_status;
    static int in_photon = 0;
    static int ph_ig = 0;
    PhCursorInfo_t cursor_info;
#endif				/* __QNXNTO__ */

#ifdef HAVE_TEXTMODE_X11_SUPPORT
    if (x11_window) {
	Window root, child;
	int root_x, root_y;
	int win_x, win_y;
	unsigned int mask;

	mc_XQueryPointer (x11_display, x11_window, &root, &child, &root_x,
		       &root_y, &win_x, &win_y, &mask);

	if (mask & ShiftMask)
	    result |= KEY_M_SHIFT;
	if (mask & ControlMask)
	    result |= KEY_M_CTRL;
	return result;
    }
#endif				/* HAVE_TEXTMODE_X11_SUPPORT */
#ifdef __QNXNTO__

    if (in_photon == 0) {
	/* First time here, let's load Photon library and attach
	   to Photon */
	in_photon = -1;
	if (getenv ("PHOTON2_PATH") != NULL) {
	    /* QNX 6.x has no support for RTLD_LAZY */
	    void *ph_handle = dlopen ("/usr/lib/libph.so", RTLD_NOW);
	    if (ph_handle != NULL) {
		ph_attach = (ph_dv_f) dlsym (ph_handle, "PhAttach");
		ph_input_group =
		    (ph_ov_f) dlsym (ph_handle, "PhInputGroup");
		ph_query_cursor =
		    (ph_pqc_f) dlsym (ph_handle, "PhQueryCursor");
		if ((ph_attach != NULL) && (ph_input_group != NULL)
		    && (ph_query_cursor != NULL)) {
		    if ((*ph_attach) (0, 0)) {	/* Attached */
			ph_ig = (*ph_input_group) (0);
			in_photon = 1;
		    }
		}
	    }
	}
    }
    /* We do not have Photon running. Assume we are in text
       console or xterm */
    if (in_photon == -1) {
	if (devctl
	    (fileno (stdin), DCMD_CHR_LINESTATUS, &mod_status,
	     sizeof (int), NULL) == -1)
	    return 0;
	shift_ext_status = mod_status & 0xffffff00UL;
	mod_status &= 0x7f;
	if (mod_status & _LINESTATUS_CON_ALT)
	    result |= KEY_M_ALT;
	if (mod_status & _LINESTATUS_CON_CTRL)
	    result |= KEY_M_CTRL;
	if ((mod_status & _LINESTATUS_CON_SHIFT)
	    || (shift_ext_status & 0x00000800UL))
	    result |= KEY_M_SHIFT;
    } else {
	(*ph_query_cursor) (ph_ig, &cursor_info);
	if (cursor_info.key_mods & 0x04)
	    result |= KEY_M_ALT;
	if (cursor_info.key_mods & 0x02)
	    result |= KEY_M_CTRL;
	if (cursor_info.key_mods & 0x01)
	    result |= KEY_M_SHIFT;
    }
#endif /* __QNXNTO__ */

#if defined __linux__ || (defined __CYGWIN__ && defined TIOCLINUX)
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
    }
#endif				/* !__linux__ */
    return result;
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
	mc_XCloseDisplay (x11_display);
#endif
}
