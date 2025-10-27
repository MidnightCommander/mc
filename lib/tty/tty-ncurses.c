/*
   Interface to the terminal controlling library.
   Ncurses wrapper.

   Copyright (C) 2005-2025
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2009.
   Ilia Maslakov <il.smind@gmail.com>, 2009.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file
 *  \brief Source: NCurses-based tty layer of Midnight-commander
 */

#include <config.h>

#include <limits.h>  // MB_LEN_MAX
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <termios.h>

#include "lib/global.h"
#include "lib/strutil.h"  // str_term_form
#include "lib/util.h"

#include "tty-internal.h"  // mc_tty_normalize_from_utf8()
#include "tty.h"
#include "color.h"  // tty_setcolor
#include "color-internal.h"
#include "key.h"
#include "mouse.h"
#include "win.h"

/* include at last !!! */
#ifdef HAVE_NCURSESW_TERM_H
#include <ncursesw/term.h>
#elif defined HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#else
#include <term.h>
#endif

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#if !defined(CTRL)
#define CTRL(x) ((x) & 0x1f)
#endif

#define yx_in_screen(y, x) (y >= 0 && y < LINES && x >= 0 && x < COLS)

/*** global variables ****************************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* ncurses supports cursor positions only within window */
/* We use our own cursor coordinates to support partially visible widgets */
static int mc_curs_row, mc_curs_col;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
tty_setup_sigwinch (void (*handler) (int))
{
#if (NCURSES_VERSION_MAJOR >= 4) && defined(SIGWINCH)
    struct sigaction act, oact;

    memset (&act, 0, sizeof (act));
    act.sa_handler = handler;
    sigemptyset (&act.sa_mask);
#ifdef SA_RESTART
    act.sa_flags = SA_RESTART;
#endif
    my_sigaction (SIGWINCH, &act, &oact);
#endif

    tty_create_winch_pipe ();
}

/* --------------------------------------------------------------------------------------------- */

static void
sigwinch_handler (int dummy)
{
    ssize_t n = 0;

    (void) dummy;

    n = write (sigwinch_pipe[1], "", 1);
    (void) n;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Get visible part of area.
 *
 * @return TRUE if any part of area is in screen bounds, FALSE otherwise.
 */
static gboolean
tty_clip (int *y, int *x, int *rows, int *cols)
{
    if (*y < 0)
    {
        *rows += *y;

        if (*rows <= 0)
            return FALSE;

        *y = 0;
    }

    if (*x < 0)
    {
        *cols += *x;

        if (*cols <= 0)
            return FALSE;

        *x = 0;
    }

    if (*y + *rows > LINES)
        *rows = LINES - *y;

    if (*rows <= 0)
        return FALSE;

    if (*x + *cols > COLS)
        *cols = COLS - *x;

    if (*cols <= 0)
        return FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_NCURSES_WIDECHAR
static const cchar_t *
get_maybe_wacs (mc_tty_char_t ch)
{
    wchar_t wch[2] = { 0, 0 };
    static cchar_t ctext;  // static so we can return its address

    switch (ch)
    {
    case MC_ACS_HLINE:
        return WACS_HLINE;
    case MC_ACS_VLINE:
        return WACS_VLINE;
    case MC_ACS_ULCORNER:
        return WACS_ULCORNER;
    case MC_ACS_URCORNER:
        return WACS_URCORNER;
    case MC_ACS_LLCORNER:
        return WACS_LLCORNER;
    case MC_ACS_LRCORNER:
        return WACS_LRCORNER;
    case MC_ACS_LTEE:
        return WACS_LTEE;
    case MC_ACS_RTEE:
        return WACS_RTEE;
    case MC_ACS_TTEE:
        return WACS_TTEE;
    case MC_ACS_BTEE:
        return WACS_BTEE;
    case MC_ACS_PLUS:
        return WACS_PLUS;

    case MC_ACS_DBL_HLINE:
        wch[0] = 0x2550;  // ═
        break;
    case MC_ACS_DBL_VLINE:
        wch[0] = 0x2551;  // ║
        break;
    case MC_ACS_DBL_ULCORNER:
        wch[0] = 0x2554;  // ╔
        break;
    case MC_ACS_DBL_URCORNER:
        wch[0] = 0x2557;  // ╗
        break;
    case MC_ACS_DBL_LLCORNER:
        wch[0] = 0x255A;  // ╚
        break;
    case MC_ACS_DBL_LRCORNER:
        wch[0] = 0x255D;  // ╝
        break;
    case MC_ACS_DBL_LTEE:
        wch[0] = 0x255F;  // ╟
        break;
    case MC_ACS_DBL_RTEE:
        wch[0] = 0x2562;  // ╢
        break;
    case MC_ACS_DBL_TTEE:
        wch[0] = 0x2564;  // ╤
        break;
    case MC_ACS_DBL_BTEE:
        wch[0] = 0x2567;  // ╧
        break;

    default:
        wch[0] = ch;
    }

    setcchar (&ctext, wch, 0, 0, NULL);
    return &ctext;
}
#endif

/* --------------------------------------------------------------------------------------------- */

static chtype
get_maybe_acs (mc_tty_char_t ch)
{
    switch (ch)
    {
    case MC_ACS_HLINE:
        return ACS_HLINE;
    case MC_ACS_VLINE:
        return ACS_VLINE;
    case MC_ACS_ULCORNER:
        return ACS_ULCORNER;
    case MC_ACS_URCORNER:
        return ACS_URCORNER;
    case MC_ACS_LLCORNER:
        return ACS_LLCORNER;
    case MC_ACS_LRCORNER:
        return ACS_LRCORNER;
    case MC_ACS_LTEE:
        return ACS_LTEE;
    case MC_ACS_RTEE:
        return ACS_RTEE;
    case MC_ACS_TTEE:
        return ACS_TTEE;
    case MC_ACS_BTEE:
        return ACS_BTEE;
    case MC_ACS_PLUS:
        return ACS_PLUS;

    // map double frame to bold single frame
    case MC_ACS_DBL_HLINE:
        return ACS_HLINE | A_BOLD;
    case MC_ACS_DBL_VLINE:
        return ACS_VLINE | A_BOLD;
    case MC_ACS_DBL_ULCORNER:
        return ACS_ULCORNER | A_BOLD;
    case MC_ACS_DBL_URCORNER:
        return ACS_URCORNER | A_BOLD;
    case MC_ACS_DBL_LLCORNER:
        return ACS_LLCORNER | A_BOLD;
    case MC_ACS_DBL_LRCORNER:
        return ACS_LRCORNER | A_BOLD;
    case MC_ACS_DBL_LTEE:
        return ACS_LTEE | A_BOLD;
    case MC_ACS_DBL_RTEE:
        return ACS_RTEE | A_BOLD;
    case MC_ACS_DBL_TTEE:
        return ACS_TTEE | A_BOLD;
    case MC_ACS_DBL_BTEE:
        return ACS_BTEE | A_BOLD;

    default:
        return ch;
    }
}

/* --------------------------------------------------------------------------------------------- */

static inline void
maybe_widechar_addch (mc_tty_char_t ch)
{
#ifdef HAVE_NCURSES_WIDECHAR
    if (mc_global.utf8_display)
        add_wch (get_maybe_wacs (ch));
    else
#endif
        addch (get_maybe_acs (ch));
}

/* --------------------------------------------------------------------------------------------- */

static inline void
maybe_widechar_hline (mc_tty_char_t ch, int len)
{
#ifdef HAVE_NCURSES_WIDECHAR
    if (mc_global.utf8_display)
        hline_set (get_maybe_wacs (ch), len);
    else
#endif
        hline (get_maybe_acs (ch), len);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
maybe_widechar_vline (mc_tty_char_t ch, int len)
{
#ifdef HAVE_NCURSES_WIDECHAR
    if (mc_global.utf8_display)
        vline_set (get_maybe_wacs (ch), len);
    else
#endif
        vline (get_maybe_acs (ch), len);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
tty_init (gboolean mouse_enable, gboolean is_xterm)
{
    struct termios mode;

    initscr ();

#ifdef HAVE_ESCDELAY
    /*
     * If ncurses exports the ESCDELAY variable, it should be set to
     * a low value, or you'll experience a delay in processing escape
     * sequences that are recognized by mc (e.g. Esc-Esc).  On the other
     * hand, making ESCDELAY too small can result in some sequences
     * (e.g. cursor arrows) being reported as separate keys under heavy
     * processor load, and this can be a problem if mc hasn't learned
     * them in the "Learn Keys" dialog.  The value is in milliseconds.
     */
    ESCDELAY = 200;
#endif

    tcgetattr (STDIN_FILENO, &mode);
    // use Ctrl-g to generate SIGINT
    mode.c_cc[VINTR] = CTRL ('g');  // ^g
    // disable SIGQUIT to allow use Ctrl-\ key
    mode.c_cc[VQUIT] = NULL_VALUE;
    tcsetattr (STDIN_FILENO, TCSANOW, &mode);

    // curses remembers the "in-program" modes after this call
    def_prog_mode ();

    tty_start_interrupt_key ();

    if (!mouse_enable)
        use_mouse_p = MOUSE_DISABLED;
    tty_init_xterm_support (is_xterm);  // do it before tty_enter_ca_mode() call
    tty_enter_ca_mode ();
    tty_raw_mode ();
    noecho ();
    keypad (stdscr, TRUE);
    nodelay (stdscr, FALSE);

    tty_setup_sigwinch (sigwinch_handler);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_shutdown (void)
{
    tty_destroy_winch_pipe ();
    tty_reset_shell_mode ();
    tty_noraw_mode ();
    tty_keypad (FALSE);
    tty_reset_screen ();
    tty_exit_ca_mode ();
}

/* --------------------------------------------------------------------------------------------- */

void
tty_enter_ca_mode (void)
{
    if (mc_global.tty.xterm_flag && smcup != NULL)
    {
        fprintf (stdout, ESC_STR "7" ESC_STR "[?47h");
        fflush (stdout);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
tty_exit_ca_mode (void)
{
    if (mc_global.tty.xterm_flag && rmcup != NULL)
    {
        fprintf (stdout, ESC_STR "[?47l" ESC_STR "8" ESC_STR "[m");
        fflush (stdout);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
tty_change_screen_size (void)
{
#if defined(TIOCGWINSZ) && NCURSES_VERSION_MAJOR >= 4
    struct winsize winsz;

    winsz.ws_col = winsz.ws_row = 0;

#ifndef NCURSES_VERSION
    tty_noraw_mode ();
    tty_reset_screen ();
#endif

    // Ioctl on the STDIN_FILENO
    ioctl (fileno (stdout), TIOCGWINSZ, &winsz);
    if (winsz.ws_col != 0 && winsz.ws_row != 0)
    {
#if defined(NCURSES_VERSION) && defined(HAVE_RESIZETERM)
        resizeterm (winsz.ws_row, winsz.ws_col);
        clearok (stdscr, TRUE);  // sigwinch's should use a semaphore!
#else
        COLS = winsz.ws_col;
        LINES = winsz.ws_row;
#endif
    }
#endif

#ifdef ENABLE_SUBSHELL
    if (mc_global.tty.use_subshell)
        tty_resize (mc_global.tty.subshell_pty);
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
tty_reset_prog_mode (void)
{
    reset_prog_mode ();
}

/* --------------------------------------------------------------------------------------------- */

void
tty_reset_shell_mode (void)
{
    reset_shell_mode ();
}

/* --------------------------------------------------------------------------------------------- */

void
tty_raw_mode (void)
{
    raw ();  // FIXME: unneeded?
    cbreak ();
}

/* --------------------------------------------------------------------------------------------- */

void
tty_noraw_mode (void)
{
    nocbreak ();  // FIXME: unneeded?
    noraw ();
}

/* --------------------------------------------------------------------------------------------- */

void
tty_noecho (void)
{
    noecho ();
}

/* --------------------------------------------------------------------------------------------- */

int
tty_flush_input (void)
{
    return flushinp ();
}

/* --------------------------------------------------------------------------------------------- */

void
tty_keypad (gboolean set)
{
    keypad (stdscr, (bool) set);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_nodelay (gboolean set)
{
    nodelay (stdscr, (bool) set);
}

/* --------------------------------------------------------------------------------------------- */

int
tty_baudrate (void)
{
    return baudrate ();
}

/* --------------------------------------------------------------------------------------------- */

int
tty_lowlevel_getch (void)
{
    return getch ();
}

/* --------------------------------------------------------------------------------------------- */

int
tty_reset_screen (void)
{
    return endwin ();
}

/* --------------------------------------------------------------------------------------------- */

void
tty_touch_screen (void)
{
    touchwin (stdscr);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_gotoyx (int y, int x)
{
    mc_curs_row = y;
    mc_curs_col = x;

    if (y < 0)
        y = 0;
    if (y >= LINES)
        y = LINES - 1;

    if (x < 0)
        x = 0;
    if (x >= COLS)
        x = COLS - 1;

    move (y, x);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_getyx (int *py, int *px)
{
    *py = mc_curs_row;
    *px = mc_curs_col;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_draw_hline (int y, int x, mc_tty_char_t ch, int len)
{
    int x1;

    if (y < 0 || y >= LINES || x >= COLS)
        return;

    x1 = x;

    if (x < 0)
    {
        len += x;
        if (len <= 0)
            return;
        x = 0;
    }

    move (y, x);
    maybe_widechar_hline (ch, len);
    move (y, x1);

    mc_curs_row = y;
    mc_curs_col = x1;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_draw_vline (int y, int x, mc_tty_char_t ch, int len)
{
    int y1;

    if (x < 0 || x >= COLS || y >= LINES)
        return;

    y1 = y;

    if (y < 0)
    {
        len += y;
        if (len <= 0)
            return;
        y = 0;
    }

    move (y, x);
    maybe_widechar_vline (ch, len);
    move (y1, x);

    mc_curs_row = y1;
    mc_curs_col = x;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_fill_region (int y, int x, int rows, int cols, unsigned char ch)
{
    int i;

    if (!tty_clip (&y, &x, &rows, &cols))
        return;

    for (i = 0; i < rows; i++)
    {
        move (y + i, x);
        hline (ch, cols);
    }

    move (y, x);

    mc_curs_row = y;
    mc_curs_col = x;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_colorize_area (int y, int x, int rows, int cols, int color)
{
#ifdef ENABLE_SHADOWS
    cchar_t *ctext;
    wchar_t wch[10];  // TODO not sure if the length is correct
    attr_t attrs;
    short color_pair;

    if (!use_colors || !tty_clip (&y, &x, &rows, &cols))
        return;

    tty_setcolor (color);
    ctext = g_malloc (sizeof (cchar_t) * (cols + 1));

    for (int row = 0; row < rows; row++)
    {
        mvin_wchnstr (y + row, x, ctext, cols);

        for (int col = 0; col < cols; col++)
        {
            getcchar (&ctext[col], wch, &attrs, &color_pair, NULL);
            setcchar (&ctext[col], wch, attrs, color, NULL);
        }

        mvadd_wchnstr (y + row, x, ctext, cols);
    }

    g_free (ctext);
#else
    (void) y;
    (void) x;
    (void) rows;
    (void) cols;
    (void) color;
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
tty_display_8bit (gboolean what)
{
    meta (stdscr, (int) what);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_print_char (mc_tty_char_t c)
{
    if (yx_in_screen (mc_curs_row, mc_curs_col))
        maybe_widechar_addch (c);
    mc_curs_col++;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_print_anychar (mc_tty_char_t c)
{
    if (mc_global.utf8_display || c > 255)
    {
        int res;
        unsigned char str[MB_LEN_MAX + 1];

        res = g_unichar_to_utf8 (c, (char *) str);
        if (res == 0)
        {
            if (yx_in_screen (mc_curs_row, mc_curs_col))
                maybe_widechar_addch ('.');
            mc_curs_col++;
        }
        else
        {
            const char *s;

            str[res] = '\0';
            s = str_term_form ((char *) str);

            if (yx_in_screen (mc_curs_row, mc_curs_col))
                addstr (s);

            if (g_unichar_iswide (c))
                mc_curs_col += 2;
            else if (!g_unichar_iszerowidth (c))
                mc_curs_col++;
        }
    }
    else
    {
        if (yx_in_screen (mc_curs_row, mc_curs_col))
            maybe_widechar_addch (c);
        mc_curs_col++;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
tty_print_string (const char *s)
{
    int len;
    int start = 0;

    s = str_term_form (s);
    len = str_term_width1 (s);

    // line is upper or below the screen or entire line is before or after screen
    if (mc_curs_row < 0 || mc_curs_row >= LINES || mc_curs_col + len <= 0 || mc_curs_col >= COLS)
    {
        mc_curs_col += len;
        return;
    }

    // skip invisible left part
    if (mc_curs_col < 0)
    {
        start = -mc_curs_col;
        len += mc_curs_col;
        mc_curs_col = 0;
    }

    mc_curs_col += len;
    if (mc_curs_col >= COLS)
        len = COLS - (mc_curs_col - len);

    addstr (str_term_substring (s, start, len));
}

/* --------------------------------------------------------------------------------------------- */

void
tty_printf (const char *fmt, ...)
{
    va_list args;
    char buf[BUF_1K];  // FIXME: is it enough?

    va_start (args, fmt);
    g_vsnprintf (buf, sizeof (buf), fmt, args);
    va_end (args);
    tty_print_string (buf);
}

/* --------------------------------------------------------------------------------------------- */

char *
tty_tgetstr (const char *cap)
{
    char *unused = NULL;

    return tgetstr ((NCURSES_CONST char *) cap, &unused);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_refresh (void)
{
    refresh ();
    doupdate ();
}

/* --------------------------------------------------------------------------------------------- */

void
tty_beep (void)
{
    beep ();
}

/* --------------------------------------------------------------------------------------------- */
