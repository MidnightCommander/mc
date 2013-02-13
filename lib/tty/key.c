/*
   Keyboard support routines.

   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2009, 2010, 2011
   The Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995
   Janne Kukonlehto, 1994, 1995
   Jakub Jelinek, 1995
   Norbert Warmuth, 1997

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file key.c
 *  \brief Source: keyboard support routines
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "lib/global.h"

#include "lib/vfs/vfs.h"

#include "tty.h"
#include "tty-internal.h"       /* mouse_enabled */
#include "mouse.h"
#include "key.h"

#include "lib/widget.h"         /* mc_refresh() */

#ifdef HAVE_TEXTMODE_X11_SUPPORT
#include "x11conn.h"
#endif

#ifdef __linux__
#if defined(__GLIBC__) && (__GLIBC__ < 2)
#include <linux/termios.h>      /* TIOCLINUX */
#else
#include <termios.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#endif /* __linux__ */

#ifdef __CYGWIN__
#include <termios.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#endif /* __CYGWIN__ */

#ifdef __QNXNTO__
#include <dlfcn.h>
#include <Ph.h>
#include <sys/dcmd_chr.h>
#endif /* __QNXNTO__ */

/*** global variables ****************************************************************************/

int mou_auto_repeat = 100;
int double_click_speed = 250;
int old_esc_mode = 0;
/* timeout for old_esc_mode in usec */
int old_esc_mode_timeout = 1000000;     /* settable via env */
int use_8th_bit_as_meta = 0;

/* This table is a mapping between names and the constants we use
 * We use this to allow users to define alternate definitions for
 * certain keys that may be missing from the terminal database
 */
const key_code_name_t key_name_conv_tab[] = {
    /* KEY_F(0) is not here, since we are mapping it to f10, so there is no reason
       to define f0 as well. Also, it makes Learn keys a bunch of problems :( */
    {KEY_F (1), "f1", N_("Function key 1"), "F1"},
    {KEY_F (2), "f2", N_("Function key 2"), "F2"},
    {KEY_F (3), "f3", N_("Function key 3"), "F3"},
    {KEY_F (4), "f4", N_("Function key 4"), "F4"},
    {KEY_F (5), "f5", N_("Function key 5"), "F5"},
    {KEY_F (6), "f6", N_("Function key 6"), "F6"},
    {KEY_F (7), "f7", N_("Function key 7"), "F7"},
    {KEY_F (8), "f8", N_("Function key 8"), "F8"},
    {KEY_F (9), "f9", N_("Function key 9"), "F9"},
    {KEY_F (10), "f10", N_("Function key 10"), "F10"},
    {KEY_F (11), "f11", N_("Function key 11"), "F11"},
    {KEY_F (12), "f12", N_("Function key 12"), "F12"},
    {KEY_F (13), "f13", N_("Function key 13"), "F13"},
    {KEY_F (14), "f14", N_("Function key 14"), "F14"},
    {KEY_F (15), "f15", N_("Function key 15"), "F15"},
    {KEY_F (16), "f16", N_("Function key 16"), "F16"},
    {KEY_F (17), "f17", N_("Function key 17"), "F17"},
    {KEY_F (18), "f18", N_("Function key 18"), "F18"},
    {KEY_F (19), "f19", N_("Function key 19"), "F19"},
    {KEY_F (20), "f20", N_("Function key 20"), "F20"},
    {KEY_BACKSPACE, "backspace", N_("Backspace key"), "Backspace"},
    {KEY_END, "end", N_("End key"), "End"},
    {KEY_UP, "up", N_("Up arrow key"), "Up"},
    {KEY_DOWN, "down", N_("Down arrow key"), "Down"},
    {KEY_LEFT, "left", N_("Left arrow key"), "Left"},
    {KEY_RIGHT, "right", N_("Right arrow key"), "Right"},
    {KEY_HOME, "home", N_("Home key"), "Home"},
    {KEY_NPAGE, "pgdn", N_("Page Down key"), "PgDn"},
    {KEY_PPAGE, "pgup", N_("Page Up key"), "PgUp"},
    {KEY_IC, "insert", N_("Insert key"), "Ins"},
    {KEY_DC, "delete", N_("Delete key"), "Del"},
    {ALT ('\t'), "complete", N_("Completion/M-tab"), "Meta-Tab"},
    {KEY_BTAB, "backtab", N_("Back Tabulation S-tab"), "Shift-Tab"},
    {KEY_KP_ADD, "kpplus", N_("+ on keypad"), "+"},
    {KEY_KP_SUBTRACT, "kpminus", N_("- on keypad"), "-"},
    {(int) '/', "kpslash", N_("Slash on keypad"), "/"},
    {KEY_KP_MULTIPLY, "kpasterisk", N_("* on keypad"), "*"},

    /* From here on, these won't be shown in Learn keys (no space) */
    {ESC_CHAR, "escape", N_("Escape key"), "Esc"},
    {KEY_LEFT, "kpleft", N_("Left arrow keypad"), "Left"},
    {KEY_RIGHT, "kpright", N_("Right arrow keypad"), "Right"},
    {KEY_UP, "kpup", N_("Up arrow keypad"), "Up"},
    {KEY_DOWN, "kpdown", N_("Down arrow keypad"), "Down"},
    {KEY_HOME, "kphome", N_("Home on keypad"), "Home"},
    {KEY_END, "kpend", N_("End on keypad"), "End"},
    {KEY_NPAGE, "kpnpage", N_("Page Down keypad"), "PgDn"},
    {KEY_PPAGE, "kpppage", N_("Page Up keypad"), "PgUp"},
    {KEY_IC, "kpinsert", N_("Insert on keypad"), "Ins"},
    {KEY_DC, "kpdelete", N_("Delete on keypad"), "Del"},
    {(int) '\n', "kpenter", N_("Enter on keypad"), "Enter"},
    {KEY_F (21), "f21", N_("Function key 21"), "F21"},
    {KEY_F (22), "f22", N_("Function key 22"), "F22"},
    {KEY_F (23), "f23", N_("Function key 23"), "F23"},
    {KEY_F (24), "f24", N_("Function key 24"), "F24"},
    {KEY_A1, "a1", N_("A1 key"), "A1"},
    {KEY_C1, "c1", N_("C1 key"), "C1"},

    /* Alternative label */
    {ESC_CHAR, "esc", N_("Escape key"), "Esc"},
    {KEY_BACKSPACE, "bs", N_("Backspace key"), "Bakspace"},
    {KEY_IC, "ins", N_("Insert key"), "Ins"},
    {KEY_DC, "del", N_("Delete key"), "Del"},
    {(int) '+', "plus", N_("Plus"), "+"},
    {(int) '-', "minus", N_("Minus"), "-"},
    {(int) '*', "asterisk", N_("Asterisk"), "*"},
    {(int) '.', "dot", N_("Dot"), "."},
    {(int) '<', "lt", N_("Less than"), "<"},
    {(int) '>', "gt", N_("Great than"), ">"},
    {(int) '=', "equal", N_("Equal"), "="},
    {(int) ',', "comma", N_("Comma"), ","},
    {(int) '\'', "apostrophe", N_("Apostrophe"), "\'"},
    {(int) ':', "colon", N_("Colon"), ":"},
    {(int) '!', "exclamation", N_("Exclamation mark"), "!"},
    {(int) '?', "question", N_("Question mark"), "?"},
    {(int) '&', "ampersand", N_("Ampersand"), "&"},
    {(int) '$', "dollar", N_("Dollar sign"), "$"},
    {(int) '"', "quota", N_("Quotation mark"), "\""},
    {(int) '%', "percent", N_("Percent sign"), "%"},
    {(int) '^', "caret", N_("Caret"), "^"},
    {(int) '~', "tilda", N_("Tilda"), "~"},
    {(int) '`', "prime", N_("Prime"), "`"},
    {(int) '_', "underline", N_("Underline"), "_"},
    {(int) '_', "understrike", N_("Understrike"), "_"},
    {(int) '|', "pipe", N_("Pipe"), "|"},
    {(int) '(', "lparenthesis", N_("Left parenthesis"), "("},
    {(int) ')', "rparenthesis", N_("Right parenthesis"), ")"},
    {(int) '[', "lbracket", N_("Left bracket"), "["},
    {(int) ']', "rbracket", N_("Right bracket"), "]"},
    {(int) '{', "lbrace", N_("Left brace"), "{"},
    {(int) '}', "rbrace", N_("Right brace"), "}"},
    {(int) '\n', "enter", N_("Enter"), "Enter"},
    {(int) '\t', "tab", N_("Tab key"), "Tab"},
    {(int) ' ', "space", N_("Space key"), "Space"},
    {(int) '/', "slash", N_("Slash key"), "/"},
    {(int) '\\', "backslash", N_("Backslash key"), "\\"},
    {(int) '#', "number", N_("Number sign #"), "#"},
    {(int) '#', "hash", N_("Number sign #"), "#"},
    /* TRANSLATORS: Please translate as in "at sign" (@). */
    {(int) '@', "at", N_("At sign"), "@"},

    /* meta keys */
    {KEY_M_CTRL, "control", N_("Ctrl"), "C"},
    {KEY_M_CTRL, "ctrl", N_("Ctrl"), "C"},
    {KEY_M_ALT, "meta", N_("Alt"), "M"},
    {KEY_M_ALT, "alt", N_("Alt"), "M"},
    {KEY_M_ALT, "ralt", N_("Alt"), "M"},
    {KEY_M_SHIFT, "shift", N_("Shift"), "S"},

    {0, NULL, NULL, NULL}
};

/*** file scope macro definitions ****************************************************************/

#define GET_TIME(tv)     (gettimeofday(&tv, (struct timezone *) NULL))
#define DIF_TIME(t1, t2) ((t2.tv_sec  - t1.tv_sec) * 1000 + (t2.tv_usec - t1.tv_usec)/1000)

/* The maximum sequence length (32 + null terminator) */
#define SEQ_BUFFER_LEN 33

/*** file scope type declarations ****************************************************************/

/* Linux console keyboard modifiers */
typedef enum
{
    SHIFT_PRESSED = (1 << 0),
    ALTR_PRESSED = (1 << 1),
    CONTROL_PRESSED = (1 << 2),
    ALTL_PRESSED = (1 << 3)
} mod_pressed_t;

typedef struct key_def
{
    char ch;                    /* Holds the matching char code */
    int code;                   /* The code returned, valid if child == NULL */
    struct key_def *next;
    struct key_def *child;      /* sequence continuation */
    int action;                 /* optional action to be done. Now used only
                                   to mark that we are just after the first
                                   Escape */
} key_def;

typedef struct
{
    int code;
    const char *seq;
    int action;
} key_define_t;

/* File descriptor monitoring add/remove routines */
typedef struct SelectList
{
    int fd;
    select_fn callback;
    void *info;
    struct SelectList *next;
} SelectList;

typedef enum KeySortType
{
    KEY_NOSORT = 0,
    KEY_SORTBYNAME,
    KEY_SORTBYCODE
} KeySortType;

#ifdef __QNXNTO__
typedef int (*ph_dv_f) (void *, void *);
typedef int (*ph_ov_f) (void *);
typedef int (*ph_pqc_f) (unsigned short, PhCursorInfo_t *);
#endif

/*** file scope variables ************************************************************************/

static key_define_t mc_default_keys[] = {
    {ESC_CHAR, ESC_STR, MCKEY_ESCAPE},
    {ESC_CHAR, ESC_STR ESC_STR, MCKEY_NOACTION},
    {0, NULL, MCKEY_NOACTION},
};

/* Broken terminfo and termcap databases on xterminals */
static key_define_t xterm_key_defines[] = {
    {KEY_F (1), ESC_STR "OP", MCKEY_NOACTION},
    {KEY_F (2), ESC_STR "OQ", MCKEY_NOACTION},
    {KEY_F (3), ESC_STR "OR", MCKEY_NOACTION},
    {KEY_F (4), ESC_STR "OS", MCKEY_NOACTION},
    {KEY_F (1), ESC_STR "[11~", MCKEY_NOACTION},
    {KEY_F (2), ESC_STR "[12~", MCKEY_NOACTION},
    {KEY_F (3), ESC_STR "[13~", MCKEY_NOACTION},
    {KEY_F (4), ESC_STR "[14~", MCKEY_NOACTION},
    {KEY_F (5), ESC_STR "[15~", MCKEY_NOACTION},
    {KEY_F (6), ESC_STR "[17~", MCKEY_NOACTION},
    {KEY_F (7), ESC_STR "[18~", MCKEY_NOACTION},
    {KEY_F (8), ESC_STR "[19~", MCKEY_NOACTION},
    {KEY_F (9), ESC_STR "[20~", MCKEY_NOACTION},
    {KEY_F (10), ESC_STR "[21~", MCKEY_NOACTION},

    /* old xterm Shift-arrows */
    {KEY_M_SHIFT | KEY_UP, ESC_STR "O2A", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_DOWN, ESC_STR "O2B", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_RIGHT, ESC_STR "O2C", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_LEFT, ESC_STR "O2D", MCKEY_NOACTION},

    /* new xterm Shift-arrows */
    {KEY_M_SHIFT | KEY_UP, ESC_STR "[1;2A", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_DOWN, ESC_STR "[1;2B", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_RIGHT, ESC_STR "[1;2C", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_LEFT, ESC_STR "[1;2D", MCKEY_NOACTION},

    /* more xterm keys with modifiers */
    {KEY_M_CTRL | KEY_PPAGE, ESC_STR "[5;5~", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_NPAGE, ESC_STR "[6;5~", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_IC, ESC_STR "[2;5~", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_DC, ESC_STR "[3;5~", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_HOME, ESC_STR "[1;5H", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_END, ESC_STR "[1;5F", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_HOME, ESC_STR "[1;2H", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_END, ESC_STR "[1;2F", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_UP, ESC_STR "[1;5A", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_DOWN, ESC_STR "[1;5B", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_RIGHT, ESC_STR "[1;5C", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_LEFT, ESC_STR "[1;5D", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_IC, ESC_STR "[2;2~", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_DC, ESC_STR "[3;2~", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_UP, ESC_STR "[1;6A", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN, ESC_STR "[1;6B", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "[1;6C", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT, ESC_STR "[1;6D", MCKEY_NOACTION},

    /* putty */
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_UP, ESC_STR "[[1;6A", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN, ESC_STR "[[1;6B", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "[[1;6C", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT, ESC_STR "[[1;6D", MCKEY_NOACTION},

    /* putty alt-arrow keys */
    /* removed as source esc esc esc trouble */
    /*
       { KEY_M_ALT | KEY_UP,    ESC_STR ESC_STR "OA", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_DOWN,  ESC_STR ESC_STR "OB", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_RIGHT, ESC_STR ESC_STR "OC", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_LEFT,  ESC_STR ESC_STR "OD", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_PPAGE, ESC_STR ESC_STR "[5~", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_NPAGE, ESC_STR ESC_STR "[6~", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_HOME,  ESC_STR ESC_STR "[1~", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_END,   ESC_STR ESC_STR "[4~", MCKEY_NOACTION },

       { KEY_M_CTRL | KEY_M_ALT | KEY_UP,    ESC_STR ESC_STR "[1;2A", MCKEY_NOACTION },
       { KEY_M_CTRL | KEY_M_ALT | KEY_DOWN,  ESC_STR ESC_STR "[1;2B", MCKEY_NOACTION },
       { KEY_M_CTRL | KEY_M_ALT | KEY_RIGHT, ESC_STR ESC_STR "[1;2C", MCKEY_NOACTION },
       { KEY_M_CTRL | KEY_M_ALT | KEY_LEFT,  ESC_STR ESC_STR "[1;2D", MCKEY_NOACTION },

       { KEY_M_CTRL | KEY_M_ALT | KEY_PPAGE, ESC_STR ESC_STR "[[5;5~", MCKEY_NOACTION },
       { KEY_M_CTRL | KEY_M_ALT | KEY_NPAGE, ESC_STR ESC_STR "[[6;5~", MCKEY_NOACTION },
       { KEY_M_CTRL | KEY_M_ALT | KEY_HOME,  ESC_STR ESC_STR "[1;5H", MCKEY_NOACTION },
       { KEY_M_CTRL | KEY_M_ALT | KEY_END,   ESC_STR ESC_STR "[1;5F", MCKEY_NOACTION },
     */
    /* xterm alt-arrow keys */
    {KEY_M_ALT | KEY_UP, ESC_STR "[1;3A", MCKEY_NOACTION},
    {KEY_M_ALT | KEY_DOWN, ESC_STR "[1;3B", MCKEY_NOACTION},
    {KEY_M_ALT | KEY_RIGHT, ESC_STR "[1;3C", MCKEY_NOACTION},
    {KEY_M_ALT | KEY_LEFT, ESC_STR "[1;3D", MCKEY_NOACTION},
    {KEY_M_ALT | KEY_PPAGE, ESC_STR "[5;3~", MCKEY_NOACTION},
    {KEY_M_ALT | KEY_NPAGE, ESC_STR "[6;3~", MCKEY_NOACTION},
    {KEY_M_ALT | KEY_HOME, ESC_STR "[1~", MCKEY_NOACTION},
    {KEY_M_ALT | KEY_END, ESC_STR "[4~", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_M_ALT | KEY_UP, ESC_STR "[1;7A", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_M_ALT | KEY_DOWN, ESC_STR "[1;7B", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_M_ALT | KEY_RIGHT, ESC_STR "[1;7C", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_M_ALT | KEY_LEFT, ESC_STR "[1;7D", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_M_ALT | KEY_PPAGE, ESC_STR "[5;7~", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_M_ALT | KEY_NPAGE, ESC_STR "[6;7~", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_M_ALT | KEY_HOME, ESC_STR "OH", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_M_ALT | KEY_END, ESC_STR "OF", MCKEY_NOACTION},

    /* rxvt keys with modifiers */
    {KEY_M_SHIFT | KEY_UP, ESC_STR "[a", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_DOWN, ESC_STR "[b", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_RIGHT, ESC_STR "[c", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_LEFT, ESC_STR "[d", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_UP, ESC_STR "Oa", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_DOWN, ESC_STR "Ob", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_RIGHT, ESC_STR "Oc", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_LEFT, ESC_STR "Od", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_PPAGE, ESC_STR "[5^", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_NPAGE, ESC_STR "[6^", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_HOME, ESC_STR "[7^", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_END, ESC_STR "[8^", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_HOME, ESC_STR "[7$", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_END, ESC_STR "[8$", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_IC, ESC_STR "[2^", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_DC, ESC_STR "[3^", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_DC, ESC_STR "[3$", MCKEY_NOACTION},

    /* konsole keys with modifiers */
    {KEY_M_SHIFT | KEY_HOME, ESC_STR "O2H", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_END, ESC_STR "O2F", MCKEY_NOACTION},

    /* gnome-terminal */
    {KEY_M_SHIFT | KEY_UP, ESC_STR "[2A", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_DOWN, ESC_STR "[2B", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_RIGHT, ESC_STR "[2C", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_LEFT, ESC_STR "[2D", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_UP, ESC_STR "[5A", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_DOWN, ESC_STR "[5B", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_RIGHT, ESC_STR "[5C", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_LEFT, ESC_STR "[5D", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_UP, ESC_STR "[6A", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN, ESC_STR "[6B", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "[6C", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT, ESC_STR "[6D", MCKEY_NOACTION},

    /* gnome-terminal - application mode */
    {KEY_M_CTRL | KEY_UP, ESC_STR "O5A", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_DOWN, ESC_STR "O5B", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_RIGHT, ESC_STR "O5C", MCKEY_NOACTION},
    {KEY_M_CTRL | KEY_LEFT, ESC_STR "O5D", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_UP, ESC_STR "O6A", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN, ESC_STR "O6B", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "O6C", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT, ESC_STR "O6D", MCKEY_NOACTION},

    /* iTerm */
    {KEY_M_SHIFT | KEY_PPAGE, ESC_STR "[5;2~", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_NPAGE, ESC_STR "[6;2~", MCKEY_NOACTION},

    /* putty */
    {KEY_M_SHIFT | KEY_PPAGE, ESC_STR "[[5;53~", MCKEY_NOACTION},
    {KEY_M_SHIFT | KEY_NPAGE, ESC_STR "[[6;53~", MCKEY_NOACTION},

    /* keypad keys */
    {KEY_IC, ESC_STR "Op", MCKEY_NOACTION},
    {KEY_DC, ESC_STR "On", MCKEY_NOACTION},
    {'/', ESC_STR "Oo", MCKEY_NOACTION},
    {'\n', ESC_STR "OM", MCKEY_NOACTION},

    {0, NULL, MCKEY_NOACTION},
};

/* qansi-m terminals have a much more key combinatios,
   which are undefined in termcap/terminfo */
static key_define_t qansi_key_defines[] = {
    /* qansi-m terminal */
    {KEY_M_CTRL | KEY_NPAGE, ESC_STR "[u", MCKEY_NOACTION},     /* Ctrl-PgDown */
    {KEY_M_CTRL | KEY_PPAGE, ESC_STR "[v", MCKEY_NOACTION},     /* Ctrl-PgUp   */
    {KEY_M_CTRL | KEY_HOME, ESC_STR "[h", MCKEY_NOACTION},      /* Ctrl-Home   */
    {KEY_M_CTRL | KEY_END, ESC_STR "[y", MCKEY_NOACTION},       /* Ctrl-End    */
    {KEY_M_CTRL | KEY_IC, ESC_STR "[`", MCKEY_NOACTION},        /* Ctrl-Insert */
    {KEY_M_CTRL | KEY_DC, ESC_STR "[p", MCKEY_NOACTION},        /* Ctrl-Delete */
    {KEY_M_CTRL | KEY_LEFT, ESC_STR "[d", MCKEY_NOACTION},      /* Ctrl-Left   */
    {KEY_M_CTRL | KEY_RIGHT, ESC_STR "[c", MCKEY_NOACTION},     /* Ctrl-Right  */
    {KEY_M_CTRL | KEY_DOWN, ESC_STR "[b", MCKEY_NOACTION},      /* Ctrl-Down   */
    {KEY_M_CTRL | KEY_UP, ESC_STR "[a", MCKEY_NOACTION},        /* Ctrl-Up     */
    {KEY_M_CTRL | KEY_KP_ADD, ESC_STR "[s", MCKEY_NOACTION},    /* Ctrl-Gr-Plus */
    {KEY_M_CTRL | KEY_KP_SUBTRACT, ESC_STR "[t", MCKEY_NOACTION},       /* Ctrl-Gr-Minus */
    {KEY_M_CTRL | '\t', ESC_STR "[z", MCKEY_NOACTION},  /* Ctrl-Tab    */
    {KEY_M_SHIFT | '\t', ESC_STR "[Z", MCKEY_NOACTION}, /* Shift-Tab   */
    {KEY_M_CTRL | KEY_F (1), ESC_STR "[1~", MCKEY_NOACTION},    /* Ctrl-F1    */
    {KEY_M_CTRL | KEY_F (2), ESC_STR "[2~", MCKEY_NOACTION},    /* Ctrl-F2    */
    {KEY_M_CTRL | KEY_F (3), ESC_STR "[3~", MCKEY_NOACTION},    /* Ctrl-F3    */
    {KEY_M_CTRL | KEY_F (4), ESC_STR "[4~", MCKEY_NOACTION},    /* Ctrl-F4    */
    {KEY_M_CTRL | KEY_F (5), ESC_STR "[5~", MCKEY_NOACTION},    /* Ctrl-F5    */
    {KEY_M_CTRL | KEY_F (6), ESC_STR "[6~", MCKEY_NOACTION},    /* Ctrl-F6    */
    {KEY_M_CTRL | KEY_F (7), ESC_STR "[7~", MCKEY_NOACTION},    /* Ctrl-F7    */
    {KEY_M_CTRL | KEY_F (8), ESC_STR "[8~", MCKEY_NOACTION},    /* Ctrl-F8    */
    {KEY_M_CTRL | KEY_F (9), ESC_STR "[9~", MCKEY_NOACTION},    /* Ctrl-F9    */
    {KEY_M_CTRL | KEY_F (10), ESC_STR "[10~", MCKEY_NOACTION},  /* Ctrl-F10  */
    {KEY_M_CTRL | KEY_F (11), ESC_STR "[11~", MCKEY_NOACTION},  /* Ctrl-F11  */
    {KEY_M_CTRL | KEY_F (12), ESC_STR "[12~", MCKEY_NOACTION},  /* Ctrl-F12  */
    {KEY_M_ALT | KEY_F (1), ESC_STR "[17~", MCKEY_NOACTION},    /* Alt-F1    */
    {KEY_M_ALT | KEY_F (2), ESC_STR "[18~", MCKEY_NOACTION},    /* Alt-F2    */
    {KEY_M_ALT | KEY_F (3), ESC_STR "[19~", MCKEY_NOACTION},    /* Alt-F3    */
    {KEY_M_ALT | KEY_F (4), ESC_STR "[20~", MCKEY_NOACTION},    /* Alt-F4    */
    {KEY_M_ALT | KEY_F (5), ESC_STR "[21~", MCKEY_NOACTION},    /* Alt-F5    */
    {KEY_M_ALT | KEY_F (6), ESC_STR "[22~", MCKEY_NOACTION},    /* Alt-F6    */
    {KEY_M_ALT | KEY_F (7), ESC_STR "[23~", MCKEY_NOACTION},    /* Alt-F7    */
    {KEY_M_ALT | KEY_F (8), ESC_STR "[24~", MCKEY_NOACTION},    /* Alt-F8    */
    {KEY_M_ALT | KEY_F (9), ESC_STR "[25~", MCKEY_NOACTION},    /* Alt-F9    */
    {KEY_M_ALT | KEY_F (10), ESC_STR "[26~", MCKEY_NOACTION},   /* Alt-F10   */
    {KEY_M_ALT | KEY_F (11), ESC_STR "[27~", MCKEY_NOACTION},   /* Alt-F11   */
    {KEY_M_ALT | KEY_F (12), ESC_STR "[28~", MCKEY_NOACTION},   /* Alt-F12   */
    {KEY_M_ALT | 'a', ESC_STR "Na", MCKEY_NOACTION},    /* Alt-a     */
    {KEY_M_ALT | 'b', ESC_STR "Nb", MCKEY_NOACTION},    /* Alt-b     */
    {KEY_M_ALT | 'c', ESC_STR "Nc", MCKEY_NOACTION},    /* Alt-c     */
    {KEY_M_ALT | 'd', ESC_STR "Nd", MCKEY_NOACTION},    /* Alt-d     */
    {KEY_M_ALT | 'e', ESC_STR "Ne", MCKEY_NOACTION},    /* Alt-e     */
    {KEY_M_ALT | 'f', ESC_STR "Nf", MCKEY_NOACTION},    /* Alt-f     */
    {KEY_M_ALT | 'g', ESC_STR "Ng", MCKEY_NOACTION},    /* Alt-g     */
    {KEY_M_ALT | 'h', ESC_STR "Nh", MCKEY_NOACTION},    /* Alt-h     */
    {KEY_M_ALT | 'i', ESC_STR "Ni", MCKEY_NOACTION},    /* Alt-i     */
    {KEY_M_ALT | 'j', ESC_STR "Nj", MCKEY_NOACTION},    /* Alt-j     */
    {KEY_M_ALT | 'k', ESC_STR "Nk", MCKEY_NOACTION},    /* Alt-k     */
    {KEY_M_ALT | 'l', ESC_STR "Nl", MCKEY_NOACTION},    /* Alt-l     */
    {KEY_M_ALT | 'm', ESC_STR "Nm", MCKEY_NOACTION},    /* Alt-m     */
    {KEY_M_ALT | 'n', ESC_STR "Nn", MCKEY_NOACTION},    /* Alt-n     */
    {KEY_M_ALT | 'o', ESC_STR "No", MCKEY_NOACTION},    /* Alt-o     */
    {KEY_M_ALT | 'p', ESC_STR "Np", MCKEY_NOACTION},    /* Alt-p     */
    {KEY_M_ALT | 'q', ESC_STR "Nq", MCKEY_NOACTION},    /* Alt-q     */
    {KEY_M_ALT | 'r', ESC_STR "Nr", MCKEY_NOACTION},    /* Alt-r     */
    {KEY_M_ALT | 's', ESC_STR "Ns", MCKEY_NOACTION},    /* Alt-s     */
    {KEY_M_ALT | 't', ESC_STR "Nt", MCKEY_NOACTION},    /* Alt-t     */
    {KEY_M_ALT | 'u', ESC_STR "Nu", MCKEY_NOACTION},    /* Alt-u     */
    {KEY_M_ALT | 'v', ESC_STR "Nv", MCKEY_NOACTION},    /* Alt-v     */
    {KEY_M_ALT | 'w', ESC_STR "Nw", MCKEY_NOACTION},    /* Alt-w     */
    {KEY_M_ALT | 'x', ESC_STR "Nx", MCKEY_NOACTION},    /* Alt-x     */
    {KEY_M_ALT | 'y', ESC_STR "Ny", MCKEY_NOACTION},    /* Alt-y     */
    {KEY_M_ALT | 'z', ESC_STR "Nz", MCKEY_NOACTION},    /* Alt-z     */
    {KEY_KP_SUBTRACT, ESC_STR "[S", MCKEY_NOACTION},    /* Gr-Minus  */
    {KEY_KP_ADD, ESC_STR "[T", MCKEY_NOACTION}, /* Gr-Plus   */
    {0, NULL, MCKEY_NOACTION},
};

/* This holds all the key definitions */
static key_def *keys = NULL;

static int input_fd;
static int disabled_channels = 0;       /* Disable channels checking */

static SelectList *select_list = NULL;

static int seq_buffer[SEQ_BUFFER_LEN];
static int *seq_append = NULL;

static int *pending_keys = NULL;

#ifdef __QNXNTO__
ph_dv_f ph_attach;
ph_ov_f ph_input_group;
ph_pqc_f ph_query_cursor;
#endif

#ifdef HAVE_TEXTMODE_X11_SUPPORT
static Display *x11_display;
static Window x11_window;
#endif /* HAVE_TEXTMODE_X11_SUPPORT */

static KeySortType has_been_sorted = KEY_NOSORT;

/* *INDENT-OFF* */
static const size_t key_conv_tab_size = G_N_ELEMENTS (key_name_conv_tab) - 1;
/* *INDENT-ON* */

static const key_code_name_t *key_conv_tab_sorted[G_N_ELEMENTS (key_name_conv_tab) - 1];

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
add_selects (fd_set * select_set)
{
    int top_fd = 0;

    if (disabled_channels == 0)
    {
        SelectList *p;

        for (p = select_list; p != NULL; p = p->next)
        {
            FD_SET (p->fd, select_set);
            if (p->fd > top_fd)
                top_fd = p->fd;
        }
    }

    return top_fd;
}

/* --------------------------------------------------------------------------------------------- */

static void
check_selects (fd_set * select_set)
{
    if (disabled_channels == 0)
    {
        gboolean retry;

        do
        {
            SelectList *p;

            retry = FALSE;
            for (p = select_list; p; p = p->next)
                if (FD_ISSET (p->fd, select_set))
                {
                    FD_CLR (p->fd, select_set);
                    (*p->callback) (p->fd, p->info);
                    retry = TRUE;
                    break;
                }
        }
        while (retry);
    }
}

/* --------------------------------------------------------------------------------------------- */
/* If set timeout is set, then we wait 0.1 seconds, else, we block */

static void
try_channels (int set_timeout)
{
    struct timeval time_out;
    static fd_set select_set;
    struct timeval *timeptr;
    int v;
    int maxfdp;

    while (1)
    {
        FD_ZERO (&select_set);
        FD_SET (input_fd, &select_set); /* Add stdin */
        maxfdp = max (add_selects (&select_set), input_fd);

        timeptr = NULL;
        if (set_timeout)
        {
            time_out.tv_sec = 0;
            time_out.tv_usec = 100000;
            timeptr = &time_out;
        }

        v = select (maxfdp + 1, &select_set, NULL, NULL, timeptr);
        if (v > 0)
        {
            check_selects (&select_set);
            if (FD_ISSET (input_fd, &select_set))
                break;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static key_def *
create_sequence (const char *seq, int code, int action)
{
    key_def *base, *p, *attach;

    for (base = attach = NULL; *seq; seq++)
    {
        p = g_new (key_def, 1);
        if (base == NULL)
            base = p;
        if (attach != NULL)
            attach->child = p;

        p->ch = *seq;
        p->code = code;
        p->child = p->next = NULL;
        if (seq[1] == '\0')
            p->action = action;
        else
            p->action = MCKEY_NOACTION;
        attach = p;
    }
    return base;
}

/* --------------------------------------------------------------------------------------------- */

static void
define_sequences (const key_define_t * kd)
{
    int i;

    for (i = 0; kd[i].code != 0; i++)
        define_sequence (kd[i].code, kd[i].seq, kd[i].action);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_TEXTMODE_X11_SUPPORT
static void
init_key_x11 (void)
{
    if (getenv ("DISPLAY") != NULL && !mc_global.tty.disable_x11)
    {
        x11_display = mc_XOpenDisplay (0);

        if (x11_display != NULL)
            x11_window = DefaultRootWindow (x11_display);
    }
}
#endif /* HAVE_TEXTMODE_X11_SUPPORT */

/* --------------------------------------------------------------------------------------------- */
/* Workaround for System V Curses vt100 bug */

static int
getch_with_delay (void)
{
    int c;

    /* This routine could be used on systems without mouse support,
       so we need to do the select check :-( */
    while (1)
    {
        if (pending_keys == NULL)
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

/* --------------------------------------------------------------------------------------------- */

static void
xmouse_get_event (Gpm_Event * ev, gboolean extended)
{
    static struct timeval tv1 = { 0, 0 };       /* Force first click as single */
    static struct timeval tv2;
    static int clicks = 0;
    static int last_btn = 0;
    int btn;

    /* Decode Xterm mouse information to a GPM style event */

    if (!extended)
    {
        /* Variable btn has following meaning: */
        /* 0 = btn1 dn, 1 = btn2 dn, 2 = btn3 dn, 3 = btn up */
        btn = tty_lowlevel_getch () - 32;
        /* Coordinates are 33-based */
        /* Transform them to 1-based */
        ev->x = tty_lowlevel_getch () - 32;
        ev->y = tty_lowlevel_getch () - 32;
    }
    else
    {
        /* SGR 1006 extension (e.g. "\e[<0;12;300M"):
           - Numbers are encoded in decimal to make it ASCII-safe
           and to overcome the limit of 223 columns/rows.
           - Mouse release is encoded by trailing 'm' rather than 'M'
           so that the released button can be reported.
           - Numbers are no longer offset by 32. */
        char c;
        btn = ev->x = ev->y = 0;
        ev->type = 0;           /* In case we return on an invalid sequence */
        while ((c = tty_lowlevel_getch ()) != ';')
        {
            if (c < '0' || c > '9')
                return;
            btn = 10 * btn + (c - '0');
        }
        while ((c = tty_lowlevel_getch ()) != ';')
        {
            if (c < '0' || c > '9')
                return;
            ev->x = 10 * ev->x + (c - '0');
        }
        while ((c = tty_lowlevel_getch ()) != 'M' && c != 'm')
        {
            if (c < '0' || c > '9')
                return;
            ev->y = 10 * ev->y + (c - '0');
        }
        /* Legacy mouse protocol doesn't tell which button was released,
           conveniently all of mc's widgets are written not to rely on this
           information. With the SGR extension the released button becomes
           known, but for the sake of simplicity we just ignore it. */
        if (c == 'm')
            btn = 3;
    }

    /* There seems to be no way of knowing which button was released */
    /* So we assume all the buttons were released */

    if (btn == 3)
    {
        if (last_btn != 0)
        {
            if ((last_btn & (GPM_B_UP | GPM_B_DOWN)) != 0)
            {
                /* FIXME: DIRTY HACK */
                /* don't generate GPM_UP after mouse wheel */
                /* need for menu event handling */
                ev->type = 0;
                tv1.tv_sec = 0;
                tv1.tv_usec = 0;
            }
            else
            {
                ev->type = GPM_UP | (GPM_SINGLE << clicks);
                GET_TIME (tv1);
            }
            ev->buttons = 0;
            last_btn = 0;
            clicks = 0;
        }
        else
        {
            /* Bogus event, maybe mouse wheel */
            ev->type = 0;
        }
    }
    else
    {
        if (btn >= 32 && btn <= 34)
        {
            btn -= 32;
            ev->type = GPM_DRAG;
        }
        else
            ev->type = GPM_DOWN;

        GET_TIME (tv2);
        if (tv1.tv_sec && (DIF_TIME (tv1, tv2) < double_click_speed))
        {
            clicks++;
            clicks %= 3;
        }
        else
            clicks = 0;

        switch (btn)
        {
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
}

/* --------------------------------------------------------------------------------------------- */
/**
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
#endif /* __QNXNTO__ */

#ifdef HAVE_TEXTMODE_X11_SUPPORT
    if (x11_window != 0)
    {
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
#endif /* HAVE_TEXTMODE_X11_SUPPORT */
#ifdef __QNXNTO__

    if (in_photon == 0)
    {
        /* First time here, let's load Photon library and attach
           to Photon */
        in_photon = -1;
        if (getenv ("PHOTON2_PATH") != NULL)
        {
            /* QNX 6.x has no support for RTLD_LAZY */
            void *ph_handle = dlopen ("/usr/lib/libph.so", RTLD_NOW);
            if (ph_handle != NULL)
            {
                ph_attach = (ph_dv_f) dlsym (ph_handle, "PhAttach");
                ph_input_group = (ph_ov_f) dlsym (ph_handle, "PhInputGroup");
                ph_query_cursor = (ph_pqc_f) dlsym (ph_handle, "PhQueryCursor");
                if ((ph_attach != NULL) && (ph_input_group != NULL) && (ph_query_cursor != NULL))
                {
                    if ((*ph_attach) (0, 0))
                    {           /* Attached */
                        ph_ig = (*ph_input_group) (0);
                        in_photon = 1;
                    }
                }
            }
        }
    }
    /* We do not have Photon running. Assume we are in text
       console or xterm */
    if (in_photon == -1)
    {
        if (devctl (fileno (stdin), DCMD_CHR_LINESTATUS, &mod_status, sizeof (int), NULL) == -1)
            return 0;
        shift_ext_status = mod_status & 0xffffff00UL;
        mod_status &= 0x7f;
        if (mod_status & _LINESTATUS_CON_ALT)
            result |= KEY_M_ALT;
        if (mod_status & _LINESTATUS_CON_CTRL)
            result |= KEY_M_CTRL;
        if ((mod_status & _LINESTATUS_CON_SHIFT) || (shift_ext_status & 0x00000800UL))
            result |= KEY_M_SHIFT;
    }
    else
    {
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
#endif /* !__linux__ */
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
push_char (int c)
{
    gboolean ret = FALSE;

    if (seq_append == NULL)
        seq_append = seq_buffer;

    if (seq_append != &(seq_buffer[SEQ_BUFFER_LEN - 2]))
    {
        *(seq_append++) = c;
        *seq_append = 0;
        ret = TRUE;
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/* Apply corrections for the keycode generated in get_key_code() */

static int
correct_key_code (int code)
{
    unsigned int c = code & ~KEY_M_MASK;        /* code without modifier */
    unsigned int mod = code & KEY_M_MASK;       /* modifier */
#ifdef __QNXNTO__
    unsigned int qmod;          /* bunch of the QNX console
                                   modifiers needs unchanged */
#endif /* __QNXNTO__ */

    /*
     * Add key modifiers directly from X11 or OS.
     * Ordinary characters only get modifiers from sequences.
     */
    if (c < 32 || c >= 256)
    {
        mod |= get_modifier ();
    }

    /* This is needed if the newline is reported as carriage return */
    if (c == '\r')
        c = '\n';

    /* This is reported to be useful on AIX */
    if (c == KEY_SCANCEL)
        c = '\t';

    /* Convert Shift+Tab and Ctrl+Tab to Back Tab
     * only if modifiers directly from X11
     */
#ifdef HAVE_TEXTMODE_X11_SUPPORT
    if (x11_window != 0)
#endif /* HAVE_TEXTMODE_X11_SUPPORT */
    {
        if ((c == '\t') && (mod & (KEY_M_SHIFT | KEY_M_CTRL)))
        {
            c = KEY_BTAB;
            mod = 0;
        }
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
    if (c == '\b')
    {
        /* Special case for backspase ('\b' < 32) */
        c = KEY_BACKSPACE;
        mod &= ~KEY_M_CTRL;
    }
    else if (c < 32 && c != ESC_CHAR && c != '\t' && c != '\n')
    {
        mod |= KEY_M_CTRL;
    }

#ifdef __QNXNTO__
    qmod = get_modifier ();

    if ((c == 127) && (mod == 0))
    {                           /* Add Ctrl/Alt/Shift-BackSpace */
        mod |= get_modifier ();
        c = KEY_BACKSPACE;
    }

    if ((c == '0') && (mod == 0))
    {                           /* Add Shift-Insert on key pad */
        if ((qmod & KEY_M_SHIFT) == KEY_M_SHIFT)
        {
            mod = KEY_M_SHIFT;
            c = KEY_IC;
        }
    }

    if ((c == '.') && (mod == 0))
    {                           /* Add Shift-Del on key pad */
        if ((qmod & KEY_M_SHIFT) == KEY_M_SHIFT)
        {
            mod = KEY_M_SHIFT;
            c = KEY_DC;
        }
    }
#endif /* __QNXNTO__ */

    /* Unrecognized 0177 is delete (preserve Ctrl) */
    if (c == 0177)
    {
        c = KEY_BACKSPACE;
    }

#if 0
    /* Unrecognized Ctrl-d is delete */
    if (c == (31 & 'd'))
    {
        c = KEY_DC;
        mod &= ~KEY_M_CTRL;
    }

    /* Unrecognized Ctrl-h is backspace */
    if (c == (31 & 'h'))
    {
        c = KEY_BACKSPACE;
        mod &= ~KEY_M_CTRL;
    }
#endif

    /* Shift+BackSpace is backspace */
    if (c == KEY_BACKSPACE && (mod & KEY_M_SHIFT))
    {
        mod &= ~KEY_M_SHIFT;
    }

    /* Convert Shift+Fn to F(n+10) */
    if (c >= KEY_F (1) && c <= KEY_F (10) && (mod & KEY_M_SHIFT))
    {
        c += 10;
    }

    /* Remove Shift information from function keys */
    if (c >= KEY_F (1) && c <= KEY_F (20))
    {
        mod &= ~KEY_M_SHIFT;
    }

    if (!mc_global.tty.alternate_plus_minus)
        switch (c)
        {
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

/* --------------------------------------------------------------------------------------------- */

static int
xgetch_second (void)
{
    fd_set Read_FD_Set;
    int c;
    struct timeval time_out;

    time_out.tv_sec = old_esc_mode_timeout / 1000000;
    time_out.tv_usec = old_esc_mode_timeout % 1000000;
    tty_nodelay (TRUE);
    FD_ZERO (&Read_FD_Set);
    FD_SET (input_fd, &Read_FD_Set);
    select (input_fd + 1, &Read_FD_Set, NULL, NULL, &time_out);
    c = tty_lowlevel_getch ();
    tty_nodelay (FALSE);
    return c;
}

/* --------------------------------------------------------------------------------------------- */

static void
learn_store_key (char *buffer, char **p, int c)
{
    if (*p - buffer > 253)
        return;
    if (c == ESC_CHAR)
    {
        *(*p)++ = '\\';
        *(*p)++ = 'e';
    }
    else if (c < ' ')
    {
        *(*p)++ = '^';
        *(*p)++ = c + 'a' - 1;
    }
    else if (c == '^')
    {
        *(*p)++ = '^';
        *(*p)++ = '^';
    }
    else
        *(*p)++ = (char) c;
}

/* --------------------------------------------------------------------------------------------- */

static void
k_dispose (key_def * k)
{
    if (k != NULL)
    {
        k_dispose (k->child);
        k_dispose (k->next);
        g_free (k);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
s_dispose (SelectList * sel)
{
    if (sel != NULL)
    {
        s_dispose (sel->next);
        g_free (sel);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
key_code_comparator_by_name (const void *p1, const void *p2)
{
    const key_code_name_t *n1 = *(const key_code_name_t **) p1;
    const key_code_name_t *n2 = *(const key_code_name_t **) p2;

    return g_ascii_strcasecmp (n1->name, n2->name);
}

/* --------------------------------------------------------------------------------------------- */

static int
key_code_comparator_by_code (const void *p1, const void *p2)
{
    const key_code_name_t *n1 = *(const key_code_name_t **) p1;
    const key_code_name_t *n2 = *(const key_code_name_t **) p2;

    return n1->code - n2->code;
}

/* --------------------------------------------------------------------------------------------- */

static inline void
sort_key_conv_tab (enum KeySortType type_sort)
{
    if (has_been_sorted != type_sort)
    {
        size_t i;
        for (i = 0; i < key_conv_tab_size; i++)
            key_conv_tab_sorted[i] = &key_name_conv_tab[i];

        if (type_sort == KEY_SORTBYNAME)
        {
            qsort (key_conv_tab_sorted, key_conv_tab_size, sizeof (key_conv_tab_sorted[0]),
                   &key_code_comparator_by_name);
        }
        else if (type_sort == KEY_SORTBYCODE)
        {
            qsort (key_conv_tab_sorted, key_conv_tab_size, sizeof (key_conv_tab_sorted[0]),
                   &key_code_comparator_by_code);
        }
        has_been_sorted = type_sort;
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
lookup_keyname (const char *name, int *idx)
{
    if (name[0] != '\0')
    {
        const key_code_name_t key = { 0, name, NULL, NULL };
        const key_code_name_t *keyp = &key;
        key_code_name_t **res;

        if (name[1] == '\0')
        {
            *idx = -1;
            return (int) name[0];
        }

        sort_key_conv_tab (KEY_SORTBYNAME);

        res = bsearch (&keyp, key_conv_tab_sorted, key_conv_tab_size,
                       sizeof (key_conv_tab_sorted[0]), key_code_comparator_by_name);

        if (res != NULL)
        {
            *idx = (int) (res - (key_code_name_t **) key_conv_tab_sorted);
            return (*res)->code;
        }
    }

    *idx = -1;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
lookup_keycode (const long code, int *idx)
{
    if (code != 0)
    {
        const key_code_name_t key = { code, NULL, NULL, NULL };
        const key_code_name_t *keyp = &key;
        key_code_name_t **res;

        sort_key_conv_tab (KEY_SORTBYCODE);

        res = bsearch (&keyp, key_conv_tab_sorted, key_conv_tab_size,
                       sizeof (key_conv_tab_sorted[0]), key_code_comparator_by_code);

        if (res != NULL)
        {
            *idx = (int) (res - (key_code_name_t **) key_conv_tab_sorted);
            return TRUE;
        }
    }

    *idx = -1;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/* This has to be called before init_slang or whatever routine
   calls any define_sequence */

void
init_key (void)
{
    const char *term = getenv ("TERM");

    /* This has to be the first define_sequence */
    /* So, we can assume that the first keys member has ESC */
    define_sequences (mc_default_keys);

    /* Terminfo on irix does not have some keys */
    if (mc_global.tty.xterm_flag
        || (term != NULL
            && (strncmp (term, "iris-ansi", 9) == 0
                || strncmp (term, "xterm", 5) == 0
                || strncmp (term, "rxvt", 4) == 0 || strcmp (term, "screen") == 0)))
        define_sequences (xterm_key_defines);

    /* load some additional keys (e.g. direct Alt-? support) */
    load_xtra_key_defines ();

#ifdef __QNX__
    if ((term != NULL) && (strncmp (term, "qnx", 3) == 0))
    {
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
    init_key_x11 ();
#endif

    /* Load the qansi-m key definitions
       if we are running under the qansi-m terminal */
    if (term != NULL && (strncmp (term, "qansi-m", 7) == 0))
        define_sequences (qansi_key_defines);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * This has to be called after SLang_init_tty/slint_init
 */

void
init_key_input_fd (void)
{
#ifdef HAVE_SLANG
    input_fd = SLang_TT_Read_FD;
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
done_key (void)
{
    k_dispose (keys);
    s_dispose (select_list);

#ifdef HAVE_TEXTMODE_X11_SUPPORT
    if (x11_display)
        mc_XCloseDisplay (x11_display);
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
add_select_channel (int fd, select_fn callback, void *info)
{
    SelectList *new;

    new = g_new (SelectList, 1);
    new->fd = fd;
    new->callback = callback;
    new->info = info;
    new->next = select_list;
    select_list = new;
}

/* --------------------------------------------------------------------------------------------- */

void
delete_select_channel (int fd)
{
    SelectList *p = select_list;
    SelectList *p_prev = NULL;
    SelectList *p_next;

    while (p != NULL)
        if (p->fd == fd)
        {
            p_next = p->next;

            if (p_prev != NULL)
                p_prev->next = p_next;
            else
                select_list = p_next;

            g_free (p);
            p = p_next;
        }
        else
        {
            p_prev = p;
            p = p->next;
        }
}

/* --------------------------------------------------------------------------------------------- */

void
channels_up (void)
{
    if (disabled_channels == 0)
        fputs ("Error: channels_up called with disabled_channels = 0\n", stderr);
    disabled_channels--;
}

/* --------------------------------------------------------------------------------------------- */

void
channels_down (void)
{
    disabled_channels++;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Return the code associated with the symbolic name keyname
 */

long
lookup_key (const char *name, char **label)
{
    char **lc_keys, **p;
    int k = -1;
    int key = 0;
    int lc_index = -1;

    int use_meta = -1;
    int use_ctrl = -1;
    int use_shift = -1;

    if (name == NULL)
        return 0;

    name = g_strstrip (g_strdup (name));
    p = lc_keys = g_strsplit_set (name, "-+ ", -1);
    g_free ((char *) name);

    while ((p != NULL) && (*p != NULL))
    {
        if ((*p)[0] != '\0')
        {
            int idx;

            key = lookup_keyname (g_strstrip (*p), &idx);

            if (key == KEY_M_ALT)
                use_meta = idx;
            else if (key == KEY_M_CTRL)
                use_ctrl = idx;
            else if (key == KEY_M_SHIFT)
                use_shift = idx;
            else
            {
                k = key;
                lc_index = idx;
                break;
            }
        }

        p++;
    }

    g_strfreev (lc_keys);

    /* output */
    if (k <= 0)
        return 0;


    if (label != NULL)
    {
        GString *s;

        s = g_string_new ("");

        if (use_meta != -1)
        {
            g_string_append (s, key_conv_tab_sorted[use_meta]->shortcut);
            g_string_append_c (s, '-');
        }
        if (use_ctrl != -1)
        {
            g_string_append (s, key_conv_tab_sorted[use_ctrl]->shortcut);
            g_string_append_c (s, '-');
        }
        if (use_shift != -1)
        {
            if (k < 127)
                g_string_append_c (s, (gchar) g_ascii_toupper ((gchar) k));
            else
            {
                g_string_append (s, key_conv_tab_sorted[use_shift]->shortcut);
                g_string_append_c (s, '-');
                g_string_append (s, key_conv_tab_sorted[lc_index]->shortcut);
            }
        }
        else if (k < 128)
        {
            if ((k >= 'A') || (lc_index < 0) || (key_conv_tab_sorted[lc_index]->shortcut == NULL))
                g_string_append_c (s, (gchar) g_ascii_tolower ((gchar) k));
            else
                g_string_append (s, key_conv_tab_sorted[lc_index]->shortcut);
        }
        else if ((lc_index != -1) && (key_conv_tab_sorted[lc_index]->shortcut != NULL))
            g_string_append (s, key_conv_tab_sorted[lc_index]->shortcut);
        else
            g_string_append_c (s, (gchar) g_ascii_tolower ((gchar) key));

        *label = g_string_free (s, FALSE);
    }

    if (use_shift != -1)
    {
        if (k < 127 && k > 31)
            k = g_ascii_toupper ((gchar) k);
        else
            k |= KEY_M_SHIFT;
    }

    if (use_ctrl != -1)
    {
        if (k < 256)
            k = XCTRL (k);
        else
            k |= KEY_M_CTRL;
    }

    if (use_meta != -1)
        k = ALT (k);

    return (long) k;
}

/* --------------------------------------------------------------------------------------------- */

char *
lookup_key_by_code (const int keycode)
{
    /* code without modifier */
    unsigned int k = keycode & ~KEY_M_MASK;
    /* modifier */
    unsigned int mod = keycode & KEY_M_MASK;

    int use_meta = -1;
    int use_ctrl = -1;
    int use_shift = -1;
    int key_idx = -1;

    GString *s;
    int idx;

    s = g_string_sized_new (8);

    if (lookup_keycode (k, &key_idx) || (k > 0 && k < 256))
    {
        if (mod & KEY_M_ALT)
        {
            if (lookup_keycode (KEY_M_ALT, &idx))
            {
                use_meta = idx;
                g_string_append (s, key_conv_tab_sorted[use_meta]->name);
                g_string_append_c (s, '-');
            }
        }
        if (mod & KEY_M_CTRL)
        {
            /* non printeble chars like a CTRL-[A..Z] */
            if (k < 32)
                k += 64;

            if (lookup_keycode (KEY_M_CTRL, &idx))
            {
                use_ctrl = idx;
                g_string_append (s, key_conv_tab_sorted[use_ctrl]->name);
                g_string_append_c (s, '-');
            }
        }
        if (mod & KEY_M_SHIFT)
        {
            if (lookup_keycode (KEY_M_ALT, &idx))
            {
                use_shift = idx;
                if (k < 127)
                    g_string_append_c (s, (gchar) g_ascii_toupper ((gchar) k));
                else
                {
                    g_string_append (s, key_conv_tab_sorted[use_shift]->name);
                    g_string_append_c (s, '-');
                    g_string_append (s, key_conv_tab_sorted[key_idx]->name);
                }
            }
        }
        else if (k < 128)
        {
            if ((k >= 'A') || (key_idx < 0) || (key_conv_tab_sorted[key_idx]->name == NULL))
                g_string_append_c (s, (gchar) k);
            else
                g_string_append (s, key_conv_tab_sorted[key_idx]->name);
        }
        else if ((key_idx != -1) && (key_conv_tab_sorted[key_idx]->name != NULL))
            g_string_append (s, key_conv_tab_sorted[key_idx]->name);
        else
            g_string_append_c (s, (gchar) keycode);
    }

    return g_string_free (s, s->len == 0);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Return TRUE on success, FALSE on error.
 * An error happens if SEQ is a beginning of an existing longer sequence.
 */

gboolean
define_sequence (int code, const char *seq, int action)
{
    key_def *base;

    if (strlen (seq) > SEQ_BUFFER_LEN - 1)
        return FALSE;

    for (base = keys; (base != NULL) && (*seq != '\0');)
        if (*seq == base->ch)
        {
            if (base->child == 0)
            {
                if (*(seq + 1) != '\0')
                    base->child = create_sequence (seq + 1, code, action);
                else
                {
                    /* The sequence matches an existing one.  */
                    base->code = code;
                    base->action = action;
                }
                return TRUE;
            }

            base = base->child;
            seq++;
        }
        else
        {
            if (base->next)
                base = base->next;
            else
            {
                base->next = create_sequence (seq, code, action);
                return TRUE;
            }
        }

    if (*seq == '\0')
    {
        /* Attempt to redefine a sequence with a shorter sequence.  */
        return FALSE;
    }

    keys = create_sequence (seq, code, action);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Check if we are idle, i.e. there are no pending keyboard or mouse
 * events.  Return 1 is idle, 0 is there are pending events.
 */
gboolean
is_idle (void)
{
    int maxfdp;
    fd_set select_set;
    struct timeval time_out;

    FD_ZERO (&select_set);
    FD_SET (input_fd, &select_set);
    maxfdp = input_fd;
#ifdef HAVE_LIBGPM
    if (mouse_enabled && (use_mouse_p == MOUSE_GPM) && (gpm_fd > 0))
    {
        FD_SET (gpm_fd, &select_set);
        maxfdp = max (maxfdp, gpm_fd);
    }
#endif
    time_out.tv_sec = 0;
    time_out.tv_usec = 0;
    return (select (maxfdp + 1, &select_set, 0, 0, &time_out) <= 0);
}

/* --------------------------------------------------------------------------------------------- */

int
get_key_code (int no_delay)
{
    int c;
    static key_def *this = NULL, *parent;
    static struct timeval esctime = { -1, -1 };
    static int lastnodelay = -1;

    if (no_delay != lastnodelay)
    {
        this = NULL;
        lastnodelay = no_delay;
    }

  pend_send:
    if (pending_keys != NULL)
    {
        int d;

        d = *pending_keys++;
        while (d == ESC_CHAR && *pending_keys != '\0')
            d = ALT (*pending_keys++);

        if (*pending_keys == '\0')
            pending_keys = seq_append = NULL;

        if (d > 127 && d < 256 && use_8th_bit_as_meta)
            d = ALT (d & 0x7f);

        this = NULL;
        return correct_key_code (d);
    }

  nodelay_try_again:
    if (no_delay)
        tty_nodelay (TRUE);

    c = tty_lowlevel_getch ();
#if (defined(USE_NCURSES) || defined(USE_NCURSESW)) && defined(KEY_RESIZE)
    if (c == KEY_RESIZE)
        goto nodelay_try_again;
#endif
    if (no_delay)
    {
        tty_nodelay (FALSE);
        if (c == -1)
        {
            if (this != NULL && parent != NULL && parent->action == MCKEY_ESCAPE && old_esc_mode)
            {
                struct timeval current, time_out;

                if (esctime.tv_sec == -1)
                    return -1;
                GET_TIME (current);
                time_out.tv_sec = old_esc_mode_timeout / 1000000 + esctime.tv_sec;
                time_out.tv_usec = old_esc_mode_timeout % 1000000 + esctime.tv_usec;
                if (time_out.tv_usec > 1000000)
                {
                    time_out.tv_usec -= 1000000;
                    time_out.tv_sec++;
                }
                if (current.tv_sec < time_out.tv_sec)
                    return -1;
                if (current.tv_sec == time_out.tv_sec && current.tv_usec < time_out.tv_usec)
                    return -1;
                this = NULL;
                pending_keys = seq_append = NULL;
                return ESC_CHAR;
            }
            return -1;
        }
    }
    else if (c == -1)
    {
        /* Maybe we got an incomplete match.
           This we do only in delay mode, since otherwise
           tty_lowlevel_getch can return -1 at any time. */
        if (seq_append != NULL)
        {
            pending_keys = seq_buffer;
            goto pend_send;
        }
        this = NULL;
        return -1;
    }

    /* Search the key on the root */
    if (!no_delay || this == NULL)
    {
        this = keys;
        parent = NULL;

        if ((c > 127 && c < 256) && use_8th_bit_as_meta)
        {
            c &= 0x7f;

            /* The first sequence defined starts with esc */
            parent = keys;
            this = keys->child;
        }
    }
    while (this != NULL)
    {
        if (c == this->ch)
        {
            if (this->child)
            {
                if (!push_char (c))
                {
                    pending_keys = seq_buffer;
                    goto pend_send;
                }
                parent = this;
                this = this->child;
                if (parent->action == MCKEY_ESCAPE && old_esc_mode)
                {
                    if (no_delay)
                    {
                        GET_TIME (esctime);
                        if (this == NULL)
                        {
                            /* Shouldn't happen */
                            fputs ("Internal error\n", stderr);
                            exit (EXIT_FAILURE);
                        }
                        goto nodelay_try_again;
                    }
                    esctime.tv_sec = -1;
                    c = xgetch_second ();
                    if (c == -1)
                    {
                        pending_keys = seq_append = NULL;
                        this = NULL;
                        return ESC_CHAR;
                    }
                }
                else
                {
                    if (no_delay)
                        goto nodelay_try_again;
                    c = tty_lowlevel_getch ();
                }
            }
            else
            {
                /* We got a complete match, return and reset search */
                int code;

                pending_keys = seq_append = NULL;
                code = this->code;
                this = NULL;
                return correct_key_code (code);
            }
        }
        else
        {
            if (this->next != NULL)
                this = this->next;
            else
            {
                if ((parent != NULL) && (parent->action == MCKEY_ESCAPE))
                {
                    /* Convert escape-digits to F-keys */
                    if (g_ascii_isdigit (c))
                        c = KEY_F (c - '0');
                    else if (c == ' ')
                        c = ESC_CHAR;
                    else
                        c = ALT (c);

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

/* --------------------------------------------------------------------------------------------- */
/* Returns a character read from stdin with appropriate interpretation */
/* Also takes care of generated mouse events */
/* Returns EV_MOUSE if it is a mouse event */
/* Returns EV_NONE  if non-blocking or interrupt set and nothing was done */

int
tty_get_event (struct Gpm_Event *event, gboolean redo_event, gboolean block)
{
    int c;
    static int flag = 0;        /* Return value from select */
#ifdef HAVE_LIBGPM
    static struct Gpm_Event ev; /* Mouse event */
#endif
    struct timeval time_out;
    struct timeval *time_addr = NULL;
    static int dirty = 3;

    if ((dirty == 3) || is_idle ())
    {
        mc_refresh ();
        dirty = 1;
    }
    else
        dirty++;

    vfs_timeout_handler ();

    /* Ok, we use (event->x < 0) to signal that the event does not contain
       a suitable position for the mouse, so we can't use show_mouse_pointer
       on it.
     */
    if (event->x > 0)
    {
        show_mouse_pointer (event->x, event->y);
        if (!redo_event)
            event->x = -1;
    }

    /* Repeat if using mouse */
    while (pending_keys == NULL)
    {
        int maxfdp;
        fd_set select_set;

        FD_ZERO (&select_set);
        FD_SET (input_fd, &select_set);
        maxfdp = max (add_selects (&select_set), input_fd);

#ifdef HAVE_LIBGPM
        if (mouse_enabled && (use_mouse_p == MOUSE_GPM))
        {
            if (gpm_fd < 0)
            {
                /* Connection to gpm broken, possibly gpm has died */
                mouse_enabled = FALSE;
                use_mouse_p = MOUSE_NONE;
                break;
            }

            FD_SET (gpm_fd, &select_set);
            maxfdp = max (maxfdp, gpm_fd);
        }
#endif

        if (redo_event)
        {
            time_out.tv_usec = mou_auto_repeat * 1000;
            time_out.tv_sec = 0;

            time_addr = &time_out;
        }
        else
        {
            int seconds;

            seconds = vfs_timeouts ();
            time_addr = NULL;

            if (seconds != 0)
            {
                /* the timeout could be improved and actually be
                 * the number of seconds until the next vfs entry
                 * timeouts in the stamp list.
                 */

                time_out.tv_sec = seconds;
                time_out.tv_usec = 0;
                time_addr = &time_out;
            }
        }

        if (!block || mc_global.tty.winch_flag != 0)
        {
            time_addr = &time_out;
            time_out.tv_sec = 0;
            time_out.tv_usec = 0;
        }

        tty_enable_interrupt_key ();
        flag = select (maxfdp + 1, &select_set, NULL, NULL, time_addr);
        tty_disable_interrupt_key ();

        /* select timed out: it could be for any of the following reasons:
         * redo_event -> it was because of the MOU_REPEAT handler
         * !block     -> we did not block in the select call
         * else       -> 10 second timeout to check the vfs status.
         */
        if (flag == 0)
        {
            if (redo_event)
                return EV_MOUSE;
            if (!block || mc_global.tty.winch_flag != 0)
                return EV_NONE;
            vfs_timeout_handler ();
        }
        if (flag == -1 && errno == EINTR)
            return EV_NONE;

        check_selects (&select_set);

        if (FD_ISSET (input_fd, &select_set))
            break;
#ifdef HAVE_LIBGPM
        if (mouse_enabled && use_mouse_p == MOUSE_GPM
            && gpm_fd > 0 && FD_ISSET (gpm_fd, &select_set))
        {
            Gpm_GetEvent (&ev);
            Gpm_FitEvent (&ev);
            *event = ev;
            return EV_MOUSE;
        }
#endif /* !HAVE_LIBGPM */
    }

#ifndef HAVE_SLANG
    flag = is_wintouched (stdscr);
    untouchwin (stdscr);
#endif /* !HAVE_SLANG */
    c = block ? getch_with_delay () : get_key_code (1);

#ifndef HAVE_SLANG
    if (flag > 0)
        tty_touch_screen ();
#endif /* !HAVE_SLANG */

    if (mouse_enabled && (c == MCKEY_MOUSE
#ifdef KEY_MOUSE
                          || c == KEY_MOUSE
#endif /* KEY_MOUSE */
                          || c == MCKEY_EXTENDED_MOUSE))
    {
        /* Mouse event */
        xmouse_get_event (event, c == MCKEY_EXTENDED_MOUSE);
        return (event->type != 0) ? EV_MOUSE : EV_NONE;
    }

    return c;
}

/* --------------------------------------------------------------------------------------------- */
/* Returns a key press, mouse events are discarded */

int
tty_getch (void)
{
    Gpm_Event ev;
    int key;

    ev.x = -1;
    while ((key = tty_get_event (&ev, FALSE, TRUE)) == EV_NONE);
    return key;
}

/* --------------------------------------------------------------------------------------------- */

char *
learn_key (void)
{
    /* LEARN_TIMEOUT in usec */
#define LEARN_TIMEOUT 200000

    fd_set Read_FD_Set;
    struct timeval endtime;
    struct timeval time_out;
    int c;
    char buffer[256];
    char *p = buffer;

    tty_keypad (FALSE);         /* disable intepreting keys by ncurses */
    c = tty_lowlevel_getch ();
    while (c == -1)
        c = tty_lowlevel_getch ();      /* Sanity check, should be unnecessary */
    learn_store_key (buffer, &p, c);
    GET_TIME (endtime);
    endtime.tv_usec += LEARN_TIMEOUT;
    if (endtime.tv_usec > 1000000)
    {
        endtime.tv_usec -= 1000000;
        endtime.tv_sec++;
    }
    tty_nodelay (TRUE);
    while (TRUE)
    {
        while ((c = tty_lowlevel_getch ()) == -1)
        {
            GET_TIME (time_out);
            time_out.tv_usec = endtime.tv_usec - time_out.tv_usec;
            if (time_out.tv_usec < 0)
                time_out.tv_sec++;
            time_out.tv_sec = endtime.tv_sec - time_out.tv_sec;
            if (time_out.tv_sec >= 0 && time_out.tv_usec > 0)
            {
                FD_ZERO (&Read_FD_Set);
                FD_SET (input_fd, &Read_FD_Set);
                select (input_fd + 1, &Read_FD_Set, NULL, NULL, &time_out);
            }
            else
                break;
        }
        if (c == -1)
            break;
        learn_store_key (buffer, &p, c);
    }
    tty_keypad (TRUE);
    tty_nodelay (FALSE);
    *p = '\0';
    return g_strdup (buffer);
#undef LEARN_TIMEOUT
}

/* --------------------------------------------------------------------------------------------- */
/* xterm and linux console only: set keypad to numeric or application
   mode. Only in application keypad mode it's possible to distinguish
   the '+' key and the '+' on the keypad ('*' and '-' ditto) */

void
numeric_keypad_mode (void)
{
    if (mc_global.tty.console_flag != '\0' || mc_global.tty.xterm_flag)
    {
        fputs (ESC_STR ">", stdout);
        fflush (stdout);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
application_keypad_mode (void)
{
    if (mc_global.tty.console_flag != '\0' || mc_global.tty.xterm_flag)
    {
        fputs (ESC_STR "=", stdout);
        fflush (stdout);
    }
}

/* --------------------------------------------------------------------------------------------- */
