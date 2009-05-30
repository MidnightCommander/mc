
#ifndef MC_TTY_SLANG_H
#define MC_TTY_SLANG_H

#ifdef HAVE_SLANG_SLANG_H
#    include <slang/slang.h>
#else
#    include <slang.h>
#endif		/* HAVE_SLANG_SLANG_H */

#include "../../src/util.h"		/* str_unconst*/

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

#define acs()		SLsmg_set_char_set (1)
#define noacs()		SLsmg_set_char_set (0)
#define baudrate()	SLang_TT_Baud_Rate

#ifndef TRUE
#    define TRUE 1
#    define FALSE 0
#endif

#define doupdate()
#define nodelay(x, val)		set_slang_delay (val)
#define noecho()
#define beep()			SLtt_beep ()
#define keypad(scr, value)	slang_keypad (value)

#define ungetch(x)		SLang_ungetkey (x)
#define touchwin(x)		SLsmg_touch_lines (0, LINES)
#define reset_shell_mode()	slang_shell_mode ()
#define reset_prog_mode()	slang_prog_mode ()
#define flushinp()

void set_slang_delay (int);
void init_slang (void);
void init_curses (void);
void slang_prog_mode (void);
void hline (int ch, int len);
void vline (int ch, int len);
int getch (void);
void slang_keypad (int set);
void slang_shell_mode (void);
void slang_shutdown (void);

#define printw		SLsmg_printf
#define COLS		SLtt_Screen_Cols
#define LINES		SLtt_Screen_Rows
#define standend()	SLsmg_normal_video ()
#define endwin()	SLsmg_reset_smg ()

#define SLsmg_draw_double_box(r, c, dr, dc)	SLsmg_draw_box ((r), (c), (dr), (dc))

#endif				/* MC_TTY_SLANG_H */
