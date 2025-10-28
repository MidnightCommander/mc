
/** \file tty.h
 *  \brief Header: %interface to the terminal controlling library
 *
 *  This file is the %interface to the terminal controlling library:
 *  slang or ncurses. It provides an additional layer of abstraction
 *  above the "real" libraries to keep the number of ifdefs in the other
 *  files small.
 */

#ifndef MC__TTY_H
#define MC__TTY_H

#include "lib/global.h"  // include <glib.h>

#ifdef HAVE_SLANG
#include "tty-slang.h"
#else
#include "tty-ncurses.h"
#endif

/*** typedefs(not structures) and defined constants **********************************************/

#define KEY_KP_ADD      4001
#define KEY_KP_SUBTRACT 4002
#define KEY_KP_MULTIPLY 4003

// For frame characters (single or double) it's one of the MC_ACS_* special values.
// Otherwise the character code in the locale's encoding.
typedef unsigned int mc_tty_char_t;

/*** enums ***************************************************************************************/

// These values always represent the given line drawing characters, in a common way regardless of
// the underlying screen library.
// These values really have the type 'mc_tty_char_t', but enum cannot denote it.
enum
{
    // Start the numbers above the highest Unicode value.
    MC_ACS_HLINE = 0x110000,  // ─
    MC_ACS_VLINE,             // │
    MC_ACS_ULCORNER,          // ┌
    MC_ACS_URCORNER,          // ┐
    MC_ACS_LLCORNER,          // └
    MC_ACS_LRCORNER,          // ┘
    MC_ACS_TTEE,              // ┬
    MC_ACS_BTEE,              // ┴
    MC_ACS_LTEE,              // ├
    MC_ACS_RTEE,              // ┤
    MC_ACS_PLUS,              // ┼

    // These don't have ACS_ counterparts in ncurses or slang, these aren't available as DEC line
    // drawing chars.
    MC_ACS_DBL_HLINE,     // ═
    MC_ACS_DBL_VLINE,     // ║
    MC_ACS_DBL_ULCORNER,  // ╔
    MC_ACS_DBL_URCORNER,  // ╗
    MC_ACS_DBL_LLCORNER,  // ╚
    MC_ACS_DBL_LRCORNER,  // ╝
    MC_ACS_DBL_TTEE,      // ╤
    MC_ACS_DBL_BTEE,      // ╧
    MC_ACS_DBL_LTEE,      // ╟
    MC_ACS_DBL_RTEE,      // ╢
};

// These refer to the roles, the given positions of more prominent and less prominent boxes.
// The actual characters, taken from the skin, may or may not match the role name.
typedef enum
{
    // single lines
    MC_TTY_FRM_HORIZ,
    MC_TTY_FRM_VERT,
    MC_TTY_FRM_LEFTTOP,
    MC_TTY_FRM_RIGHTTOP,
    MC_TTY_FRM_LEFTBOTTOM,
    MC_TTY_FRM_RIGHTBOTTOM,
    MC_TTY_FRM_TOPMIDDLE,
    MC_TTY_FRM_BOTTOMMIDDLE,
    MC_TTY_FRM_LEFTMIDDLE,
    MC_TTY_FRM_RIGHTMIDDLE,
    MC_TTY_FRM_CROSS,

    // double lines
    MC_TTY_FRM_DHORIZ,
    MC_TTY_FRM_DVERT,
    MC_TTY_FRM_DLEFTTOP,
    MC_TTY_FRM_DRIGHTTOP,
    MC_TTY_FRM_DLEFTBOTTOM,
    MC_TTY_FRM_DRIGHTBOTTOM,
    MC_TTY_FRM_DTOPMIDDLE,
    MC_TTY_FRM_DBOTTOMMIDDLE,
    MC_TTY_FRM_DLEFTMIDDLE,
    MC_TTY_FRM_DRIGHTMIDDLE,

    MC_TTY_FRM_MAX
} mc_tty_frm_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

// The actual characters used for frame drawing, as taken from the skin. Indexed by MC_TTY_FRM_*,
// the values are either one of MCS_ACS_* or a character in the current locale.
extern mc_tty_char_t mc_tty_frm[];

extern char *tty_tgetstr (const char *name);

/*** declarations of public functions ************************************************************/

extern void tty_beep (void);

/* {{{ Input }}} */

extern gboolean tty_check_xterm_compat (gboolean force_xterm);
extern void tty_init (gboolean mouse_enable, gboolean is_xterm);
extern void tty_shutdown (void);

extern void tty_start_interrupt_key (void);
extern void tty_enable_interrupt_key (void);
extern void tty_disable_interrupt_key (void);
extern gboolean tty_got_interrupt (void);

extern gboolean tty_got_winch (void);
extern gboolean tty_flush_winch (void);

extern void tty_reset_prog_mode (void);
extern void tty_reset_shell_mode (void);

extern void tty_raw_mode (void);
extern void tty_noraw_mode (void);

extern void tty_noecho (void);
extern int tty_flush_input (void);

extern void tty_keypad (gboolean set);
extern void tty_nodelay (gboolean set);
extern int tty_baudrate (void);

/* {{{ Output }}} */

/*
   The output functions do not check themselves for screen overflows,
   so make sure that you never write more than what fits on the screen.
   While SLang provides such a feature, ncurses does not.
 */

extern int tty_reset_screen (void);
extern void tty_touch_screen (void);

extern void tty_gotoyx (int y, int x);
extern void tty_getyx (int *py, int *px);

extern void tty_display_8bit (gboolean what);
extern void tty_print_char (mc_tty_char_t c);
extern void tty_print_anychar (mc_tty_char_t c);
extern void tty_print_string (const char *s);
extern void tty_printf (const char *s, ...) G_GNUC_PRINTF (1, 2);

extern void tty_print_one_vline (gboolean single);
extern void tty_print_one_hline (gboolean single);
extern void tty_draw_hline (int y, int x, mc_tty_char_t ch, int len);
extern void tty_draw_vline (int y, int x, mc_tty_char_t ch, int len);
extern void tty_draw_box (int y, int x, int rows, int cols, gboolean single);
extern void tty_draw_box_shadow (int y, int x, int rows, int cols, int shadow_color);
extern void tty_fill_region (int y, int x, int rows, int cols, unsigned char ch);

extern mc_tty_char_t tty_unicode_to_mc_acs (gunichar c);

extern int tty_resize (int fd);
extern void tty_refresh (void);
extern void tty_change_screen_size (void);

/* Clear screen */
extern void tty_clear_screen (void);

extern void tty_enter_ca_mode (void);
extern void tty_exit_ca_mode (void);

/*** inline functions ****************************************************************************/

#endif
