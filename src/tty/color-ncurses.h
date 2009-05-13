
/** \file color-ncurses.h
 *  \brief Header: NCurses-specific color setup
 */

#ifndef MC_COLOR_NCURSES_H
#define MC_COLOR_NCURSES_H

#include "../../src/tty/tty-ncurses.h"	/* NCurses headers */

#define MAX_PAIRS 64
extern int attr_pairs [MAX_PAIRS];

#define MY_COLOR_PAIR(x) (COLOR_PAIR (x) | attr_pairs [x])
#define IF_COLOR(co, bw) (use_colors ? MY_COLOR_PAIR (co) : bw)

#define MARKED_SELECTED_COLOR IF_COLOR (4, A_REVERSE | A_BOLD)

void mc_init_pair (int index, int foreground, int background);

#endif				/* MC_COLOR_NCURSES_H */
