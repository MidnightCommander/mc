
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

extern void tty_init (gboolean slow, gboolean ugly_lines);
extern void tty_shutdown (void);

extern gboolean tty_is_slow (void);

extern void tty_start_interrupt_key(void);
extern void tty_enable_interrupt_key(void);
extern void tty_disable_interrupt_key(void);
extern gboolean tty_got_interrupt(void);

extern void tty_reset_prog_mode (void);
extern void tty_reset_shell_mode (void);

extern void tty_raw_mode (void);
extern void tty_noraw_mode (void);

extern void tty_noecho (void);
extern int tty_flush_input (void);

extern void tty_keypad (gboolean set);
extern void tty_nodelay (gboolean set);
extern int tty_baudrate (void);

extern int tty_lowlevel_getch (void);

/* {{{ Output }}} */

/*
    The output functions do not check themselves for screen overflows,
    so make sure that you never write more than what fits on the screen.
    While SLang provides such a feature, ncurses does not.
 */

extern int tty_reset_screen (void);
extern void tty_touch_screen (void);

extern void tty_gotoyx(int y, int x);
extern void tty_getyx(int *py, int *px);

extern void tty_set_alt_charset (gboolean alt_charset);

extern void tty_display_8bit (gboolean what);
extern void tty_print_char(int c);
extern void tty_print_alt_char(int c);
extern void tty_print_anychar(int c);
extern void tty_print_string(const char *s);
extern void tty_printf(const char *s, ...);

extern void tty_print_one_vline (void);
extern void tty_print_one_hline (void);
extern void tty_draw_hline (int y, int x, int ch, int len);
extern void tty_draw_vline (int y, int x, int ch, int len);
extern void tty_draw_box (int y, int x, int rows, int cols);
extern void tty_fill_region (int y, int x, int rows, int cols, unsigned char ch);

extern int mc_tty_ugly_frm[];

typedef enum{
    MC_TTY_FRM_thinvert,
    MC_TTY_FRM_thinhoriz,
    MC_TTY_FRM_vert,
    MC_TTY_FRM_horiz,
    MC_TTY_FRM_lefttop,
    MC_TTY_FRM_righttop,
    MC_TTY_FRM_leftbottom,
    MC_TTY_FRM_rightbottom,
    MC_TTY_FRM_centertop,
    MC_TTY_FRM_centerbottom,
    MC_TTY_FRM_leftmiddle,
    MC_TTY_FRM_rightmiddle,
    MC_TTY_FRM_centermiddle,
    MC_TTY_FRM_MAX
} mc_tty_frm_t;

extern char *tty_tgetstr (const char *name);

extern void tty_beep (void);

#define KEY_KP_ADD	4001
#define KEY_KP_SUBTRACT	4002
#define KEY_KP_MULTIPLY	4003

extern void tty_refresh (void);
extern void tty_setup_sigwinch (void (*handler) (int));

extern int  mc_tty_normalize_lines_char(const char *);

#endif			/* MC_TTY_H */
