
/** \file color.h
 *  \brief Header: color setup
 *
 * PLEASE FORGOT ABOUT tty/color.h!
 * Use skin engine for getting needed color pairs.
 *
 * edit/syntax.c may use this file directly, I'm agree. :)
 *
 */

#ifndef MC_COLOR_H
#define MC_COLOR_H

#include "../../src/global.h"   /* glib.h */

#ifdef HAVE_SLANG
#   include "color-slang.h"
#else
#   include "tty-ncurses.h"
#endif

extern char *command_line_colors;

void tty_init_colors (gboolean disable, gboolean force);
void tty_colors_done (void);

gboolean tty_use_colors (void);
int tty_try_alloc_color_pair (const char *, const char *);
int tty_try_alloc_color_pair2 (const char *, const char *, gboolean);

void tty_color_free_all_tmp (void);
void tty_color_free_all_non_tmp (void);

void tty_setcolor (int color);
void tty_lowlevel_setcolor (int color);
void tty_set_normal_attrs (void);

void tty_color_set_defaults (const char *, const char *);

#define ALLOC_COLOR_PAIR_INDEX 1

#endif /* MC_COLOR_H */
