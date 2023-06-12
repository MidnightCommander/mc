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

typedef struct
{
    char *fg;
    char *bg;
    char *attrs;
    size_t pair_index;
} tty_color_pair_t;

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void tty_init_colors (gboolean disable, gboolean force);
void tty_colors_done (void);

gboolean tty_use_colors (void);
int tty_try_alloc_color_pair (const tty_color_pair_t * color, gboolean is_temp);

void tty_color_free_temp (void);
void tty_color_free_all (void);

void tty_setcolor (int color);
void tty_lowlevel_setcolor (int color);
void tty_set_normal_attrs (void);

void tty_color_set_defaults (const tty_color_pair_t * color);

extern gboolean tty_use_256colors (GError ** error);
extern gboolean tty_use_truecolors (GError ** error);

/*** inline functions ****************************************************************************/

#endif /* MC__COLOR_H */
