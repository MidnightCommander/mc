#ifndef MC_SELCODEPAGE_H
#define MC_SELCODEPAGE_H

#ifdef HAVE_CHARSET
int select_charset (int delta_x, int delta_y, int current_charset, int seldisplay);
int do_select_codepage (void);
#endif				/* HAVE_CHARSET */

#endif
