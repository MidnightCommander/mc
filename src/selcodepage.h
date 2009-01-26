#ifndef MC_SELCODEPAGE_H
#define MC_SELCODEPAGE_H

#ifdef HAVE_CHARSET
int select_charset (int current_charset, int seldisplay, const char *title);
int do_select_codepage (const char *title);
#endif				/* HAVE_CHARSET */

#endif
