
/** \file tty-internal.h
 *  \brief Header: internal suff of the terminal controlling library
 */

#ifndef MC_TTY_INTERNAL_H
#define MC_TTY_INTERNAL_H

#include "../../src/global.h"		/* include <glib.h> */

/* If true lines are shown by spaces */
extern gboolean slow_tty;

static inline void
tty_draw_box_slow (int y, int x, int ys, int xs)
{
    tty_draw_hline (y, x, ' ', xs);
    tty_draw_vline (y, x, ' ', ys);
    tty_draw_vline (y, x + xs - 1, ' ', ys);
    tty_draw_hline (y + ys - 1, x, ' ', xs);
}

#endif			/* MC_TTY_INTERNAL_H */
