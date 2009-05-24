
/** \file win.h
 *  \brief Header: curses utilities
 */

#ifndef MC_WIN_H
#define MC_WIN_H

#include "../../src/global.h"		/* <glib.h> */

/* Terminal management */
extern int xterm_flag;
void do_enter_ca_mode (void);
void do_exit_ca_mode (void);

void show_rxvt_contents (int starty, unsigned char y1, unsigned char y2);
gboolean look_for_rxvt_extensions (void);

#endif				/* MC_WIN_H */
