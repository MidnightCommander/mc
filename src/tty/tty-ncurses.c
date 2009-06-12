/*
   Interface to the terminal controlling library.
   Ncurses wrapper.

   Copyright (C) 2005, 2006, 2007, 2009 Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2009.

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

#include "../../src/global.h"

#ifndef WANT_TERM_H
#   define WANT_TERM_H
#endif

#include "../../src/tty/tty.h"		/* tty_is_ugly_line_drawing() */
#include "../../src/tty/color-ncurses.h"
#include "../../src/tty/color-internal.h"
#include "../../src/tty/win.h"

#include "../../src/background.h"	/* we_are_background */
#include "../../src/strutil.h"		/* str_term_form */
#include "../../src/util.h"		/* str_unconst */

/* include at last !!! */
#ifdef WANT_TERM_H
#   include <term.h>
#endif			/* WANT_TERM_H*/

/*** global variables **************************************************/

/*** file scope macro definitions **************************************/

/*** global variables **************************************************/

/*** file scope type declarations **************************************/

/*** file scope variables **********************************************/

static const struct {
    int acscode;
    int character;
} acs_approx [] = {
    { 'q',  '-' }, /* ACS_HLINE */
    { 'x',  '|' }, /* ACS_VLINE */
    { 'l',  '+' }, /* ACS_ULCORNER */
    { 'k',  '+' }, /* ACS_URCORNER */
    { 'm',  '+' }, /* ACS_LLCORNER */
    { 'j',  '+' }, /* ACS_LRCORNER */
    { 'a',  '#' }, /* ACS_CKBOARD */
    { 'u',  '+' }, /* ACS_RTEE */
    { 't',  '+' }, /* ACS_LTEE */
    { 'w',  '+' }, /* ACS_TTEE */
    { 'v',  '+' }, /* ACS_BTEE */
    { 'n',  '+' }, /* ACS_PLUS */
    { 0, 0 } };

/*** file scope functions **********************************************/

/*** public functions **************************************************/

void
tty_init_curses (void)
{
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

    do_enter_ca_mode ();
    raw ();
    noecho ();
    keypad (stdscr, TRUE);
    nodelay (stdscr, FALSE);

    if (tty_is_ugly_line_drawing ()) {
	int i;
	for (i = 0; acs_approx[i].acscode != 0; i++)
	    acs_map[acs_approx[i].acscode] = acs_approx[i].character;
    }
}

void
tty_init_slang (void)
{
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
    return baudrate();
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
    if ((y >= 0) && (x >= 0))
	move (y, x);
    hline (ch, len);
}

/* if x < 0 or y < 0, draw line starting from current position */
void
tty_draw_vline (int y, int x, int ch, int len)
{
    if ((y >= 0) && (x >= 0))
	move (y, x);
    vline (ch, len);
}

void
tty_draw_box (int y, int x, int rows, int cols)
{
#define waddc(_y, _x, c) move (_y, _x); addch (c)
    waddc (y, x, ACS_ULCORNER);
    hline (ACS_HLINE, cols - 2);
    waddc (y + rows - 1, x, ACS_LLCORNER);
    hline (ACS_HLINE, cols - 2);

    waddc (y, x + cols - 1, ACS_URCORNER);
    waddc (y + rows - 1, x + cols - 1, ACS_LRCORNER);

    move (y + 1, x);
    vline (ACS_VLINE, rows - 2);

    move (y + 1, x + cols - 1);
    vline (ACS_VLINE, rows - 2);
#undef waddc
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
tty_print_alt_char (int c)
{
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
    return tgetstr (str_unconst (cap), &unused);
}

void
tty_refresh (void)
{
#ifdef WITH_BACKGROUND
    if (!we_are_background)
#endif				/* WITH_BACKGROUND */
	refresh ();
    doupdate ();
}

void
tty_beep (void)
{
    beep ();
}
