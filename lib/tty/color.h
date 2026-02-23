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

#include "lib/global.h"  // glib.h

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

/*
 * Color values below this number refer directly to the ncurses/slang color pair id.
 *
 * Color values beginning with this number represent a role, for which the ncurses/slang color pair
 * id is looked up runtime from tty_color_role_to_pair which is initialized when loading the skin.
 *
 * This way the numbers representing skinnable colors remain the same across skin changes.
 *
 * (Note: Another approach could be to allocate a new ncurses/slang color pair id for every role,
 * even if they use the same colors in the skin. The problem with this is that for 8-color terminal
 * descriptors (like TERM=linux and TERM=xterm) ncurses only allows 64 color pairs, so we can't
 * afford to allocate new ids for duplicates.)
 *
 * In src/editor/editdraw.c these values are shifted by 16 bits to the left and stored in an int.
 * Keep this number safely below 65536 to avoid overflow.
 */
#define TTY_COLOR_MAP_OFFSET 4096

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

extern int *tty_color_role_to_pair;

/*** declarations of public functions ************************************************************/

void tty_init_colors (gboolean disable, gboolean force, int color_map_size);
void tty_colors_done (void);

gboolean tty_use_colors (void);
int tty_try_alloc_color_pair (const tty_color_pair_t *color, gboolean is_temp);

void tty_color_free_temp (void);
void tty_color_free_all (void);

void tty_setcolor (int color);
void tty_set_normal_attrs (void);

extern gboolean tty_use_256colors (GError **error);
extern gboolean tty_use_truecolors (GError **error);

/*** inline functions ****************************************************************************/

#endif
