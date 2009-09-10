
/** \file color-ncurses.h
 *  \brief Header: NCurses-specific color setup
 */

#ifndef MC_COLOR_NCURSES_H
#define MC_COLOR_NCURSES_H

#include "../../src/tty/tty-ncurses.h"	/* NCurses headers */

gboolean tty_use_colors ();


//#define MY_COLOR_PAIR(x) (COLOR_PAIR (x) | attr_pairs [x])
#define MY_COLOR_PAIR(x) COLOR_PAIR (x)

/*
#define IF_COLOR(co, bw) (tty_use_colors () ? MY_COLOR_PAIR (co) : bw)
#define MARKED_SELECTED_COLOR IF_COLOR (4, A_REVERSE | A_BOLD)
*/

#endif				/* MC_COLOR_NCURSES_H */
