
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

#include "../../src/tty/color-ncurses.h"
#include "../../src/tty/tty-ncurses.h"

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

/*** file scope functions **********************************************/

/*** public functions **************************************************/

void
tty_setcolor (int color)
{
    attrset (color);
}

void
tty_lowlevel_setcolor (int color)
{
    attrset (MY_COLOR_PAIR (color));
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
}
