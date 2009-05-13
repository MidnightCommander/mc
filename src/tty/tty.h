
/** \file tty.h
 *  \brief Header: %interface to the terminal controlling library
 *
 *  This file is the %interface to the terminal controlling library:
 *  slang or ncurses. It provides an additional layer of abstraction
 *  above the "real" libraries to keep the number of ifdefs in the other
 *  files small.
 */

#ifndef MC_TTY_H
#define MC_TTY_H

#include "../../src/global.h"		/* include <glib.h> */

#ifdef HAVE_SLANG
#   include "../../src/tty/tty-slang.h"
#else
#   include "../../src/tty/tty-ncurses.h"
#endif

/* {{{ Input }}} */

extern void tty_start_interrupt_key(void);
extern void tty_enable_interrupt_key(void);
extern void tty_disable_interrupt_key(void);
extern gboolean tty_got_interrupt(void);


/* {{{ Output }}} */

/*
    The output functions do not check themselves for screen overflows,
    so make sure that you never write more than what fits on the screen.
    While SLang provides such a feature, ncurses does not.
 */

extern void tty_gotoyx(int y, int x);
extern void tty_getyx(int *py, int *px);

extern void tty_disable_colors (gboolean disable, gboolean force);
extern void tty_setcolor(int color);
extern void tty_lowlevel_setcolor(int color);

extern void tty_print_char(int c);
extern void tty_print_alt_char(int c);
extern void tty_print_string(const char *s);
extern void tty_printf(const char *s, ...);

extern void tty_set_ugly_line_drawing (gboolean do_ugly);
extern gboolean tty_is_ugly_line_drawing (void);
extern void tty_print_one_vline(void);
extern void tty_print_one_hline(void);
extern void tty_print_vline(int top, int left, int length);
extern void tty_print_hline(int top, int left, int length);
extern void tty_draw_box (int y, int x, int rows, int cols);


extern char *tty_tgetstr (const char *name);

/* legacy interface */

#define start_interrupt_key()	tty_start_interrupt_key()
#define enable_interrupt_key()	tty_enable_interrupt_key()
#define disable_interrupt_key()	tty_disable_interrupt_key()
#define got_interrupt()		tty_got_interrupt()
#define one_hline()		tty_print_one_hline()
#define one_vline()		tty_print_one_vline()

#define KEY_KP_ADD	4001
#define KEY_KP_SUBTRACT	4002
#define KEY_KP_MULTIPLY	4003

void tty_refresh (void);

#endif			/* MC_TTY_H */
