
/** \file myslang.h
 *  \brief Header: slang wrapper module
 */

#ifndef MC_MYSLANG_H
#define MC_MYSLANG_H

#ifdef HAVE_SLANG_SLANG_H
#    include <slang/slang.h>
#else
#    include <slang.h>
#endif	/* HAVE_SLANG_SLANG_H */

enum {
    KEY_BACKSPACE = 400,
    KEY_END, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_A1, KEY_C1, KEY_NPAGE, KEY_PPAGE, KEY_IC,
    KEY_ENTER, KEY_DC, KEY_SCANCEL, KEY_BTAB
};

#define KEY_F(x) (1000+x)

#define ACS_VLINE SLSMG_VLINE_CHAR
#define ACS_HLINE SLSMG_HLINE_CHAR
#define ACS_ULCORNER SLSMG_ULCORN_CHAR
#define ACS_LLCORNER SLSMG_LLCORN_CHAR
#define ACS_URCORNER SLSMG_URCORN_CHAR
#define ACS_LRCORNER SLSMG_LRCORN_CHAR

#define ACS_LTEE 't'
#define acs()   SLsmg_set_char_set(1)
#define noacs() SLsmg_set_char_set (0)
#define baudrate() SLang_TT_Baud_Rate

enum {
    COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW, COLOR_BLUE,
    COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
};

/* When using Slang with color, we have all the indexes free but
 * those defined here (A_BOLD, A_UNDERLINE, A_REVERSE, A_BOLD_REVERSE)
 */
#define A_BOLD      0x40
#define A_UNDERLINE 0x40
#define A_REVERSE   0x20
#define A_BOLD_REVERSE 0x21

#ifndef A_NORMAL
#    define A_NORMAL    0x00
#endif

#define COLOR_PAIR(x) x

#ifndef TRUE
#    define TRUE 1
#    define FALSE 0
#endif

void slang_set_raw_mode (void);

#define doupdate()
#define raw() slang_set_raw_mode()
#define noraw()
#define nodelay(x,val) set_slang_delay(val)
#define noecho()
#define beep() SLtt_beep ()
#define keypad(scr,value) slang_keypad (value)

#define ungetch(x) SLang_ungetkey(x)
#define start_color()
#define touchwin(x) SLsmg_touch_lines(0, LINES)
#define reset_shell_mode() slang_shell_mode()
#define reset_prog_mode() slang_prog_mode()
#define flushinp()

void slint_goto (int y, int x);
void attrset (int color);
void set_slang_delay (int);
void slang_init (void);
void slang_prog_mode (void);
void hline (int ch, int len);
void vline (int ch, int len);
int getch (void);
void slang_keypad (int set);
void slang_shell_mode (void);
void slang_shutdown (void);
int has_colors (void);

#define move(x, y) SLsmg_gotorc(x, y)
#define getyx(stdscr, row, col) \
    do { \
	row = SLsmg_get_row(); \
	col = SLsmg_get_column(); \
    } while (0)
#define printw SLsmg_printf
#define COLS SLtt_Screen_Cols
#define LINES SLtt_Screen_Rows
#define standend() SLsmg_normal_video()

#ifdef UTF8
/*
 * Patched S-Lang in Red Hat 8.0 expects wchar_t as the argument to addch()
 * Avoid conversion by using SLsmg_write_nchars(), which takes char*
 */
#undef addch
static inline void
mc_addch (char c)
{
    SLsmg_write_nchars (&c, 1);
}
#define addch(c) mc_addch(c)
#else
#define addch(c) SLsmg_write_char(c)
#endif

#define addstr(s) SLsmg_write_string(str_unconst(s))
#define refresh() SLsmg_refresh()
#define endwin() SLsmg_reset_smg()

#define SLsmg_draw_double_box(r,c,dr,dc) SLsmg_draw_box ((r), (c), (dr), (dc))

#endif
