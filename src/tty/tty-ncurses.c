/*
   Interface to the terminal controlling library.
   Ncurses wrapper.

   Copyright (C) 2005, 2006, 2007, 2009 Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2009.
   Ilia Maslakov <il.smind@gmail.com>, 2009.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

/** \file
 *  \brief Source: NCurses-based tty layer of Midnight-commander
 */

#include <config.h>

#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>

#include "../../src/global.h"

#ifndef WANT_TERM_H
#   define WANT_TERM_H
#endif

#include "../../src/tty/tty-internal.h" /* slow_tty */
#include "../../src/tty/tty.h"
#include "../../src/tty/color-internal.h"
#include "../../src/tty/win.h"

#include "../../src/strutil.h"  /* str_term_form */

/* include at last !!! */
#ifdef WANT_TERM_H
#   include <term.h>
#endif /* WANT_TERM_H */

/*** global variables **************************************************/

/*** file scope macro definitions **************************************/

/*** global variables **************************************************/

/*** file scope type declarations **************************************/

/*** file scope variables **********************************************/

/*** file scope functions **********************************************/

/* --------------------------------------------------------------------------------------------- */
/*** public functions **************************************************/
/* --------------------------------------------------------------------------------------------- */

int
mc_tty_normalize_lines_char (const char *ch)
{
    char *str2;
    int res;

    struct mc_tty_lines_struct {
        const char *line;
        int line_code;
    } const lines_codes[] = {
        {"\342\224\214", ACS_LRCORNER}, /* ┌ */
        {"\342\224\220", ACS_LLCORNER}, /* ┐ */
        {"\342\224\224", ACS_URCORNER}, /* └ */
        {"\342\224\230", ACS_ULCORNER}, /* ┘ */
        {"\342\224\234", ACS_LTEE}, /* ├ */
        {"\342\224\244", ACS_RTEE}, /* ┤ */
        {"\342\224\254", ACS_TTEE}, /* ┬ */
        {"\342\224\264", ACS_BTEE}, /* ┴ */
        {"\342\224\200", ACS_HLINE}, /* ─ */
        {"\342\224\202", ACS_VLINE}, /* │ */
        {"\342\224\274", ACS_PLUS}, /* ┼ */

        {"\342\225\235", ACS_LRCORNER | A_BOLD}, /* ╔ */
        {"\342\225\232", ACS_LLCORNER | A_BOLD}, /* ╗ */
        {"\342\225\227", ACS_URCORNER | A_BOLD}, /* ╚ */
        {"\342\225\224", ACS_ULCORNER | A_BOLD}, /* ╝ */
        {"\342\225\237", ACS_LTEE | A_BOLD}, /* ╟ */
        {"\342\225\242", ACS_RTEE | A_BOLD}, /* ╢ */
        {"\342\225\244", ACS_TTEE | A_BOLD}, /* ╤ */
        {"\342\225\247", ACS_BTEE | A_BOLD}, /* ╧ */
        {"\342\225\220", ACS_HLINE | A_BOLD}, /* ═ */
        {"\342\225\221", ACS_VLINE | A_BOLD}, /* ║ */

        {NULL, 0}
    };

    if (ch == NULL)
        return (int) ' ';

    for (res = 0; lines_codes[res].line; res++) {
        if (strcmp (ch, lines_codes[res].line) == 0)
            return lines_codes[res].line_code;
    }

    str2 = mc_tty_normalize_from_utf8 (ch);
    res = g_utf8_get_char_validated (str2, -1);

    if (res < 0)
        res = (unsigned char) str2[0];
    g_free (str2);

    return res;
}

/* --------------------------------------------------------------------------------------------- */


void
tty_init (gboolean slow, gboolean ugly_lines)
{
    slow_tty = slow;
    (void) ugly_lines;

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
#endif /* HAVE_ESCDELAY */

    tty_start_interrupt_key ();

    do_enter_ca_mode ();
    raw ();
    noecho ();
    keypad (stdscr, TRUE);
    nodelay (stdscr, FALSE);

}

void
tty_shutdown (void)
{
    endwin ();
}

void
tty_reset_prog_mode (void)
{
    reset_prog_mode ();
}

void
tty_reset_shell_mode (void)
{
    reset_shell_mode ();
}

void
tty_raw_mode (void)
{
    raw ();
}

void
tty_noraw_mode (void)
{
    noraw ();
}

void
tty_noecho (void)
{
    noecho ();
}

int
tty_flush_input (void)
{
    return flushinp ();
}

void
tty_keypad (gboolean set)
{
    keypad (stdscr, (bool) set);
}

void
tty_nodelay (gboolean set)
{
    nodelay (stdscr, (bool) set);
}

int
tty_baudrate (void)
{
    return baudrate ();
}

int
tty_lowlevel_getch (void)
{
    return getch ();
}

int
tty_reset_screen (void)
{
    return endwin ();
}

void
tty_touch_screen (void)
{
    touchwin (stdscr);
}

void
tty_gotoyx (int y, int x)
{
    move (y, x);
}

void
tty_getyx (int *py, int *px)
{
    getyx (stdscr, *py, *px);
}

/* if x < 0 or y < 0, draw line starting from current position */
void
tty_draw_hline (int y, int x, int ch, int len)
{
    if ((chtype) ch == ACS_HLINE)
        ch = mc_tty_ugly_frm[MC_TTY_FRM_thinhoriz];

    if ((y >= 0) && (x >= 0))
        move (y, x);
    hline (ch, len);
}

/* if x < 0 or y < 0, draw line starting from current position */
void
tty_draw_vline (int y, int x, int ch, int len)
{
    if ((chtype) ch == ACS_VLINE)
        ch = mc_tty_ugly_frm[MC_TTY_FRM_thinvert];

    if ((y >= 0) && (x >= 0))
        move (y, x);
    vline (ch, len);
}

void
tty_fill_region (int y, int x, int rows, int cols, unsigned char ch)
{
    int i;

    for (i = 0; i < rows; i++) {
        move (y + i, x);
        hline (ch, cols);
    }

    move (y, x);
}

void
tty_set_alt_charset (gboolean alt_charset)
{
    (void) alt_charset;
}

void
tty_display_8bit (gboolean what)
{
    meta (stdscr, (int) what);
}

void
tty_print_char (int c)
{
    addch (c);
}

void
tty_print_anychar (int c)
{
    unsigned char str[6 + 1];

    if (c > 255) {
        int res = g_unichar_to_utf8 (c, (char *) str);
        if (res == 0) {
            str[0] = '.';
            str[1] = '\0';
        } else {
            str[res] = '\0';
        }
        addstr (str_term_form ((char *) str));
    } else {
        addch (c);
    }
}

void
tty_print_alt_char (int c)
{
    if ((chtype) c == ACS_VLINE)
        c = mc_tty_ugly_frm[MC_TTY_FRM_thinvert];
    else if ((chtype) c == ACS_HLINE)
        c = mc_tty_ugly_frm[MC_TTY_FRM_thinhoriz];
    else if ((chtype) c == ACS_LTEE)
        c = mc_tty_ugly_frm[MC_TTY_FRM_leftmiddle];
    else if ((chtype) c == ACS_RTEE)
        c = mc_tty_ugly_frm[MC_TTY_FRM_rightmiddle];
    else if ((chtype) c == ACS_ULCORNER)
        c = mc_tty_ugly_frm[MC_TTY_FRM_lefttop];
    else if ((chtype) c == ACS_LLCORNER)
        c = mc_tty_ugly_frm[MC_TTY_FRM_leftbottom];
    else if ((chtype) c == ACS_URCORNER)
        c = mc_tty_ugly_frm[MC_TTY_FRM_righttop];
    else if ((chtype) c == ACS_LRCORNER)
        c = mc_tty_ugly_frm[MC_TTY_FRM_rightbottom];
    else if ((chtype) c == ACS_PLUS)
        c = mc_tty_ugly_frm[MC_TTY_FRM_centermiddle];

    addch (c);
}

void
tty_print_string (const char *s)
{
    addstr (str_term_form (s));
}

void
tty_printf (const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    vw_printw (stdscr, fmt, args);
    va_end (args);
}

char *
tty_tgetstr (const char *cap)
{
    char *unused = NULL;
    return tgetstr ((char *) cap, &unused);
}

void
tty_refresh (void)
{
    refresh ();
    doupdate ();
}

void
tty_setup_sigwinch (void (*handler) (int))
{
#if (NCURSES_VERSION_MAJOR >= 4) && defined (SIGWINCH)
    struct sigaction act, oact;
    act.sa_handler = handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
#ifdef SA_RESTART
    act.sa_flags |= SA_RESTART;
#endif /* SA_RESTART */
    sigaction (SIGWINCH, &act, &oact);
#endif /* SIGWINCH */
}

void
tty_beep (void)
{
    beep ();
}
