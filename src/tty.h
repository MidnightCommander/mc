#ifndef MC_TTY_H
#define MC_TTY_H

/*
    This file is the interface to the terminal controlling library---
    ncurses, slang or the built-in slang. It provides an additional
    layer of abstraction above the "real" libraries to keep the number
    of ifdefs in the other files small.
 */

#ifdef HAVE_SLANG
#   include "myslang.h"
#endif

#ifdef USE_NCURSES
#    ifdef HAVE_NCURSES_CURSES_H
#        include <ncurses/curses.h>
#    elif HAVE_NCURSES_H
#        include <ncurses.h>
#    else
#        include <curses.h>
#    endif
#endif /* USE_NCURSES */

/* {{{ Input }}} */

extern void tty_enable_interrupt_key(void);
extern void tty_disable_interrupt_key(void);
extern gboolean tty_got_interrupt(void);

/* {{{ Output }}} */

/*
    The output functions do not check themselves for screen overflows,
    so make sure that you never write more than what fits on the screen.
    While SLang provides such a feature, ncurses does not.
 */

extern void tty_gotoyx(int, int);
extern void tty_getyx(int *, int *);
extern void tty_print_char(int);
extern void tty_print_alt_char(int);
extern void tty_print_string(const char *);
extern void tty_print_one_vline(void);
extern void tty_print_one_hline(void);
extern void tty_print_vline(int top, int left, int length);
extern void tty_print_hline(int top, int left, int length);

/* legacy interface */

#define enable_interrupt_key()	tty_enable_interrupt_key()
#define disable_interrupt_key()	tty_disable_interrupt_key()
#define got_interrupt()		tty_got_interrupt()
#define one_hline()		tty_print_one_hline()
#define one_vline()		tty_print_one_vline()

#ifndef HAVE_SLANG
#   define acs()
#   define noacs()
#endif

#define KEY_KP_ADD	4001
#define KEY_KP_SUBTRACT	4002
#define KEY_KP_MULTIPLY	4003

void mc_refresh (void);

/* print S left-aligned, adjusted to exactly LEN characters */
extern void printwstr (const char *s, int len);

#endif
