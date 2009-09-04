
/** \file color-slang.h
 *  \brief Header: S-Lang-specific color setup
 */

#ifndef MC_COLOR_SLANG_H
#define MC_COLOR_SLANG_H

#include "../../src/tty/tty-slang.h"	/* S-Lang headers */

enum {
    COLOR_BLACK = 0,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_BLUE,
    COLOR_MAGENTA,
    COLOR_CYAN,
    COLOR_WHITE
};

/* When using Slang with color, we have all the indexes free but
 * those defined here (A_BOLD, A_UNDERLINE, A_REVERSE, A_BOLD_REVERSE)
 */
#define A_BOLD		0x40
#define A_UNDERLINE	0x40
#define A_REVERSE	0x20
#define A_BOLD_REVERSE	0x21

#ifndef A_NORMAL
#    define A_NORMAL	0x00
#endif

gboolean tty_use_colors ();

#define COLOR_PAIR(x) x

#define IF_COLOR(co, bw) (tty_use_colors () ? COLOR_PAIR (co) : bw)

/*
#define MARKED_SELECTED_COLOR IF_COLOR (4, (SLtt_Use_Ansi_Colors ? A_BOLD_REVERSE : A_REVERSE | A_BOLD))
*/

#endif				/* MC_COLOR_SLANG_H */
