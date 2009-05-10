/*
   Interface to the terminal controlling library.

   Copyright (C) 2005, 2006, 2007 Free Software Foundation, Inc.

   Written by:
   Roland Illig <roland.illig@gmx.de>, 2005.

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
#include "../../src/tty/color.h"

#include "../../src/main.h"		/* for slow_terminal */
#include "../../src/strutil.h"
#include "../../src/background.h"	/* we_are_background */

/*** global variables **************************************************/

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

extern void
tty_setcolor(int c)
{
    attrset(c);
}

extern void
tty_print_one_hline(void)
{
    if (slow_terminal)
	tty_print_char(' ');
    else
	tty_print_alt_char(ACS_HLINE);
}

extern void
tty_print_one_vline(void)
{
    if (slow_terminal)
	tty_print_char(' ');
    else
	tty_print_alt_char(ACS_VLINE);
}

extern void
tty_print_hline(int top, int left, int length)
{
    int i;

    tty_gotoyx(top, left);
    for (i = 0; i < length; i++)
	tty_print_one_hline();
}

extern void
tty_print_vline(int top, int left, int length)
{
    int i;

    tty_gotoyx(top, left);
    for (i = 0; i < length; i++) {
	tty_gotoyx(top + i, left);
	tty_print_one_vline();
    }
}
