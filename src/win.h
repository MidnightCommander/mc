
/** \file win.h
 *  \brief Header: curses utilities
 */

#ifndef MC_WIN_H
#define MC_WIN_H

#include "dialog.h"	/* cb_ret_t */

/* Keys management */
typedef void (*movefn) (void *, int);
cb_ret_t check_movement_keys (int key, int page_size, void *data, movefn backfn,
				movefn forfn, movefn topfn, movefn bottomfn);
int lookup_key (char *keyname);

/* Terminal management */
extern int xterm_flag;
void do_enter_ca_mode (void);
void do_exit_ca_mode (void);

void mc_raw_mode (void);
void mc_noraw_mode (void);

#endif				/* MC_WIN_H */
