/** \file color.h
 *  \brief Header: color setup
 *
 * PLEASE FORGOT ABOUT tty/color.h!
 * Use skin engine for getting needed color pairs.
 *
 * edit/syntax.c may use this file directly, I'm agree. :)
 *
 */

#ifndef MC__COLOR_H
#define MC__COLOR_H

#include "lib/global.h"         /* glib.h */

#ifdef HAVE_SLANG
#include "color-slang.h"
#else
#include "tty-ncurses.h"
#endif

/*** typedefs(not structures) and defined constants **********************************************/

#define ALLOC_COLOR_PAIR_INDEX 1

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void tty_init_colors (gboolean disable, gboolean force);
void tty_colors_done (void);

gboolean tty_use_colors (void);
int tty_try_alloc_color_pair (const char *, const char *, const char *);
int tty_try_alloc_color_pair2 (const char *, const char *, const char *, gboolean);

void tty_color_free_all_tmp (void);
void tty_color_free_all_non_tmp (void);

void tty_setcolor (int color);
void tty_lowlevel_setcolor (int color);
void tty_set_normal_attrs (void);

void tty_color_set_defaults (const char *, const char *, const char *);

extern gboolean tty_use_256colors (void);
extern gboolean tty_use_truecolors (GError **);

/*** inline functions ****************************************************************************/
#endif /* MC_COLOR_H */
