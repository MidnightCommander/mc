/*
   Interface to the terminal controlling library.

   Copyright (C) 2005, 2006, 2007, 2009 Free Software Foundation, Inc.

   Written by:
   Roland Illig <roland.illig@gmx.de>, 2005.
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

/** \file tty.c
 *  \brief Source: %interface to the terminal controlling library
 */

#include <config.h>

#include <signal.h>
#include <stdarg.h>

#include "../../src/global.h"

#include "../../src/tty/tty.h"
#include "../../src/tty/tty-internal.h"

#include "../../src/strutil.h"

/*** global variables **************************************************/

/* If true lines are drown by spaces */
gboolean slow_tty = FALSE;

/* If true use +, -, | for line drawing */
gboolean ugly_line_drawing = FALSE;

int ugly_frm_thinvert = '|';
int ugly_frm_thinhoriz = '-';
int ugly_frm_vert = '|';
int ugly_frm_horiz = '-';
int ugly_frm_lefttop = '+';
int ugly_frm_righttop = '+';
int ugly_frm_leftbottom = '+';
int ugly_frm_rightbottom = '+';
int ugly_frm_leftmiddle = '+';
int ugly_frm_rightmiddle = '+';

/*** file scope macro definitions **************************************/

/*** file scope type declarations **************************************/

/*** file scope variables **********************************************/

static volatile sig_atomic_t got_interrupt = 0;

/*** file scope functions **********************************************/

static void
sigintr_handler(int signo)
{
    (void) &signo;
    got_interrupt = 1;
}

/*** public functions **************************************************/

extern gboolean
tty_is_slow (void)
{
    return slow_tty;
}

extern void
tty_start_interrupt_key(void)
{
    struct sigaction act;

    act.sa_handler = sigintr_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = SA_RESTART;
    sigaction (SIGINT, &act, NULL);
}

extern void
tty_enable_interrupt_key(void)
{
    struct sigaction act;

    act.sa_handler = sigintr_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction (SIGINT, &act, NULL);
    got_interrupt = 0;
}

extern void
tty_disable_interrupt_key(void)
{
    struct sigaction act;

    act.sa_handler = SIG_IGN;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction (SIGINT, &act, NULL);
}

extern gboolean
tty_got_interrupt(void)
{
    gboolean rv;

    rv = (got_interrupt != 0);
    got_interrupt = 0;
    return rv;
}

void
tty_print_one_hline (void)
{
    if (ugly_line_drawing || slow_tty)
	tty_print_char (ugly_frm_thinhoriz);
    else
	tty_print_alt_char (ACS_HLINE);
}

void
tty_print_one_vline (void)
{
    if (ugly_line_drawing || slow_tty)
	tty_print_char (ugly_frm_thinvert);
    else
	tty_print_alt_char (ACS_VLINE);
}

void
tty_draw_box_slow (int y, int x, int ys, int xs)
{
    tty_draw_vline (y, x, ugly_frm_vert, ys);
    tty_draw_vline (y, x + xs - 1, ugly_frm_vert, ys);
    tty_draw_hline (y, x, ugly_frm_horiz, xs);
    tty_draw_hline (y + ys - 1, x, ugly_frm_horiz, xs);
    tty_gotoyx (y, x);
    tty_print_char (ugly_frm_lefttop);
    tty_gotoyx (y + ys - 1, x);
    tty_print_char (ugly_frm_leftbottom);
    tty_gotoyx (y, x + xs - 1);
    tty_print_char (ugly_frm_righttop);
    tty_gotoyx (y + ys - 1, x + xs - 1);
    tty_print_char (ugly_frm_rightbottom);
}
