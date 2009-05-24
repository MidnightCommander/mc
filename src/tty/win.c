/* Curses utilities
   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/** \file win.c
 *  \brief Source: curses utilities
 */

#include <config.h>

#include <stdio.h>
#include <string.h>

#include "../../src/tty/win.h"

/* This flag is set by xterm detection routine in function main() */
/* It is used by function view_other_cmd() */
int xterm_flag = 0;

/* The following routines only work on xterm terminals */

void
do_enter_ca_mode (void)
{
    if (!xterm_flag)
	return;
    fprintf (stdout, /* ESC_STR ")0" */ ESC_STR "7" ESC_STR "[?47h");
    fflush (stdout);
}

void
do_exit_ca_mode (void)
{
    if (!xterm_flag)
	return;
    fprintf (stdout, ESC_STR "[?47l" ESC_STR "8" ESC_STR "[m");
    fflush (stdout);
}
