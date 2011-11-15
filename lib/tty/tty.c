/*
   Interface to the terminal controlling library.

   Copyright (C) 2005, 2006, 2007, 2009, 2011
   The Free Software Foundation, Inc.

   Written by:
   Roland Illig <roland.illig@gmx.de>, 2005.
   Andrew Borodin <aborodin@vmail.ru>, 2009.

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file tty.c
 *  \brief Source: %interface to the terminal controlling library
 */

#include <config.h>

#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>             /* exit() */

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include "lib/global.h"
#include "lib/strutil.h"

#include "tty.h"
#include "tty-internal.h"
#include "mouse.h"              /* use_mouse_p */
#include "win.h"

/*** global variables ****************************************************************************/

/* If true program softkeys (HP terminals only) on startup and after every
   command ran in the subshell to the description found in the termcap/terminfo
   database */
int reset_hp_softkeys = 0;

int mc_tty_frm[MC_TTY_FRM_MAX];

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static volatile sig_atomic_t got_interrupt = 0;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
sigintr_handler (int signo)
{
    (void) &signo;
    got_interrupt = 1;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Check terminal type. If $TERM is not set or value is empty, mc finishes with EXIT_FAILURE.
 *
 * @param force_xterm Set forced the XTerm type
 *
 * @return true if @param force_xterm is true or value of $TERM is one of term*, konsole*
 *              rxvt*, Eterm or dtterm
 */
gboolean
tty_check_term (gboolean force_xterm)
{
    const char *termvalue;

    termvalue = getenv ("TERM");
    if (termvalue == NULL || *termvalue == '\0')
    {
        fputs (_("The TERM environment variable is unset!\n"), stderr);
        exit (EXIT_FAILURE);
    }

    return force_xterm || strncmp (termvalue, "xterm", 5) == 0
        || strncmp (termvalue, "konsole", 7) == 0
        || strncmp (termvalue, "rxvt", 4) == 0
        || strcmp (termvalue, "Eterm") == 0 || strcmp (termvalue, "dtterm") == 0;
}

/* --------------------------------------------------------------------------------------------- */

extern void
tty_start_interrupt_key (void)
{
    struct sigaction act;

    act.sa_handler = sigintr_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = SA_RESTART;
    sigaction (SIGINT, &act, NULL);
}

/* --------------------------------------------------------------------------------------------- */

extern void
tty_enable_interrupt_key (void)
{
    struct sigaction act;

    act.sa_handler = sigintr_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction (SIGINT, &act, NULL);
    got_interrupt = 0;
}

/* --------------------------------------------------------------------------------------------- */

extern void
tty_disable_interrupt_key (void)
{
    struct sigaction act;

    act.sa_handler = SIG_IGN;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction (SIGINT, &act, NULL);
}

/* --------------------------------------------------------------------------------------------- */

extern gboolean
tty_got_interrupt (void)
{
    gboolean rv;

    rv = (got_interrupt != 0);
    got_interrupt = 0;
    return rv;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_print_one_hline (gboolean single)
{
    tty_print_alt_char (ACS_HLINE, single);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_print_one_vline (gboolean single)
{
    tty_print_alt_char (ACS_VLINE, single);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_draw_box (int y, int x, int ys, int xs, gboolean single)
{
    int y2, x2;

    ys--;
    xs--;

    y2 = y + ys;
    x2 = x + xs;

    tty_draw_vline (y, x, mc_tty_frm[single ? MC_TTY_FRM_VERT : MC_TTY_FRM_DVERT], ys);
    tty_draw_vline (y, x2, mc_tty_frm[single ? MC_TTY_FRM_VERT : MC_TTY_FRM_DVERT], ys);
    tty_draw_hline (y, x, mc_tty_frm[single ? MC_TTY_FRM_HORIZ : MC_TTY_FRM_DHORIZ], xs);
    tty_draw_hline (y2, x, mc_tty_frm[single ? MC_TTY_FRM_HORIZ : MC_TTY_FRM_DHORIZ], xs);
    tty_gotoyx (y, x);
    tty_print_alt_char (ACS_ULCORNER, single);
    tty_gotoyx (y2, x);
    tty_print_alt_char (ACS_LLCORNER, single);
    tty_gotoyx (y, x2);
    tty_print_alt_char (ACS_URCORNER, single);
    tty_gotoyx (y2, x2);
    tty_print_alt_char (ACS_LRCORNER, single);
}

/* --------------------------------------------------------------------------------------------- */

char *
mc_tty_normalize_from_utf8 (const char *str)
{
    GIConv conv;
    GString *buffer;
    const char *_system_codepage = str_detect_termencoding ();

    if (str_isutf8 (_system_codepage))
        return g_strdup (str);

    conv = g_iconv_open (_system_codepage, "UTF-8");
    if (conv == INVALID_CONV)
        return g_strdup (str);

    buffer = g_string_new ("");

    if (str_convert (conv, str, buffer) == ESTR_FAILURE)
    {
        g_string_free (buffer, TRUE);
        str_close_conv (conv);
        return g_strdup (str);
    }
    str_close_conv (conv);

    return g_string_free (buffer, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/** Resize given terminal using TIOCSWINSZ, return ioctl() result */
int
tty_resize (int fd)
{
#if defined TIOCSWINSZ
    struct winsize tty_size;

    tty_size.ws_row = LINES;
    tty_size.ws_col = COLS;
    tty_size.ws_xpixel = tty_size.ws_ypixel = 0;

    return ioctl (fd, TIOCSWINSZ, &tty_size);
#else
    return 0;
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
tty_change_screen_size (void)
{
#if defined(HAVE_SLANG) || NCURSES_VERSION_MAJOR >= 4
#if defined TIOCGWINSZ
    struct winsize winsz;

    winsz.ws_col = winsz.ws_row = 0;
    /* Ioctl on the STDIN_FILENO */
    ioctl (0, TIOCGWINSZ, &winsz);
    if (winsz.ws_col && winsz.ws_row)
    {
#if defined(NCURSES_VERSION) && defined(HAVE_RESIZETERM)
        resizeterm (winsz.ws_row, winsz.ws_col);
        clearok (stdscr, TRUE); /* sigwinch's should use a semaphore! */
#else
        COLS = winsz.ws_col;
        LINES = winsz.ws_row;
#endif
#ifdef HAVE_SUBSHELL_SUPPORT
        if (!mc_global.tty.use_subshell)
            return;

        tty_resize (mc_global.tty.subshell_pty);
#endif
    }
#endif /* TIOCGWINSZ */
#endif /* defined(HAVE_SLANG) || NCURSES_VERSION_MAJOR >= 4 */
}

/* --------------------------------------------------------------------------------------------- */

void
tty_init_xterm_support (gboolean is_xterm)
{
    const char *termvalue;

    termvalue = getenv ("TERM");

    /* Check mouse and ca capabilities */
    /* terminfo/termcap structures have been already initialized,
       in slang_init() or/and init_curses()  */
    /* Check terminfo at first, then check termcap */
    xmouse_seq = tty_tgetstr ("kmous");
    if (xmouse_seq == NULL)
        xmouse_seq = tty_tgetstr ("Km");
    smcup = tty_tgetstr ("smcup");
    if (smcup == NULL)
        smcup = tty_tgetstr ("ti");
    rmcup = tty_tgetstr ("rmcup");
    if (rmcup == NULL)
        rmcup = tty_tgetstr ("te");

    if (strcmp (termvalue, "cygwin") == 0)
    {
        is_xterm = TRUE;
        use_mouse_p = MOUSE_DISABLED;
    }

    if (is_xterm)
    {
        /* Default to the standard xterm sequence */
        if (xmouse_seq == NULL)
            xmouse_seq = ESC_STR "[M";

        /* Enable mouse unless explicitly disabled by --nomouse */
        if (use_mouse_p != MOUSE_DISABLED)
        {
            if (mc_global.tty.old_mouse)
                use_mouse_p = MOUSE_XTERM_NORMAL_TRACKING;
            else
            {
                /* FIXME: this dirty hack to set supported type of tracking the mouse */
                const char *color_term = getenv ("COLORTERM");
                if (strncmp (termvalue, "rxvt", 4) == 0 ||
                    (color_term != NULL && strncmp (color_term, "rxvt", 4) == 0) ||
                    strcmp (termvalue, "Eterm") == 0)
                    use_mouse_p = MOUSE_XTERM_NORMAL_TRACKING;
                else
                    use_mouse_p = MOUSE_XTERM_BUTTON_EVENT_TRACKING;
            }
        }
    }
}

/* --------------------------------------------------------------------------------------------- */
