#ifdef HAVE_CHARSET
#ifndef __SELCODEPAGE_H__
#define __SELCODEPAGE_H__

int select_charset (int current_charset, int seldisplay);
int do_select_codepage (void);

#endif				/* __SELCODEPAGE_H__ */
#endif				/* HAVE_CHARSET */
