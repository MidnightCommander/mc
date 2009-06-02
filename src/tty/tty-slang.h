
#ifndef MC_TTY_SLANG_H
#define MC_TTY_SLANG_H

#ifdef HAVE_SLANG_SLANG_H
#    include <slang/slang.h>
#else
#    include <slang.h>
#endif		/* HAVE_SLANG_SLANG_H */

enum {
    KEY_BACKSPACE = 400,
    KEY_END, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_A1, KEY_C1, KEY_NPAGE, KEY_PPAGE, KEY_IC,
    KEY_ENTER, KEY_DC, KEY_SCANCEL, KEY_BTAB
};

#define KEY_F(x)	(1000 + x)

#define ACS_VLINE		SLSMG_VLINE_CHAR
#define ACS_HLINE		SLSMG_HLINE_CHAR
#define ACS_LTEE		SLSMG_LTEE_CHAR
#define ACS_RTEE		SLSMG_RTEE_CHAR
#define ACS_ULCORNER		SLSMG_ULCORN_CHAR
#define ACS_LLCORNER		SLSMG_LLCORN_CHAR
#define ACS_URCORNER		SLSMG_URCORN_CHAR
#define ACS_LRCORNER		SLSMG_LRCORN_CHAR

#define noecho()

#define ungetch(x)		SLang_ungetkey (x)
#define flushinp()

void init_slang (void);
void init_curses (void);
void hline (int ch, int len);
void vline (int ch, int len);
int getch (void);
void slang_shutdown (void);

#define printw		SLsmg_printf
#define COLS		SLtt_Screen_Cols
#define LINES		SLtt_Screen_Rows
#define endwin()	SLsmg_reset_smg ()

#define SLsmg_draw_double_box(r, c, dr, dc)	SLsmg_draw_box ((r), (c), (dr), (dc))

#endif				/* MC_TTY_SLANG_H */
