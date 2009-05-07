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

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#include "global.h"
#include "tty.h"
#include "color.h"
#include "mouse.h"
#include "dialog.h"
#include "widget.h"
#include "win.h"
#include "key.h"		/* XCTRL and ALT macros  */
#include "layout.h"
#include "strutil.h"

/*
 * Common handler for standard movement keys in a text area.  Provided
 * functions are called with the "data" argument.  backfn and forfn also
 * get an argument indicating how many lines to scroll.  Return 1 if
 * the key was handled, 0 otherwise.
 */
int
check_movement_keys (int key, int page_size, void *data, movefn backfn,
		     movefn forfn, movefn topfn, movefn bottomfn)
{
    switch (key) {
    case KEY_UP:
    case XCTRL ('p'):
	(*backfn) (data, 1);
	break;

    case KEY_DOWN:
    case XCTRL ('n'):
	(*forfn) (data, 1);
	break;

    case KEY_PPAGE:
    case ALT ('v'):
	(*backfn) (data, page_size - 1);
	break;

    case KEY_NPAGE:
    case XCTRL ('v'):
	(*forfn) (data, page_size - 1);
	break;

    case KEY_HOME:
    case KEY_M_CTRL | KEY_HOME:
    case KEY_M_CTRL | KEY_PPAGE:
    case KEY_A1:
    case ALT ('<'):
	(*topfn) (data, 0);
	break;

    case KEY_END:
    case KEY_M_CTRL | KEY_END:
    case KEY_M_CTRL | KEY_NPAGE:
    case KEY_C1:
    case ALT ('>'):
	(*bottomfn) (data, 0);
	break;

    case 'b':
    case KEY_BACKSPACE:
	(*backfn) (data, page_size - 1);
	break;

    case ' ':
	(*forfn) (data, page_size - 1);
	break;

    case 'u':
	(*backfn) (data, page_size / 2);
	break;

    case 'd':
	(*forfn) (data, page_size / 2);
	break;

    case 'g':
	(*topfn) (data, 0);
	break;

    case 'G':
	(*bottomfn) (data, 0);
	break;

    default:
	return MSG_NOT_HANDLED;
    }
    return MSG_HANDLED;
}

/* Classification routines */
int is_abort_char (int c)
{
    return (c == XCTRL('c') || c == XCTRL('g') || c == ESC_CHAR ||
	    c == KEY_F(10));
}

void mc_raw_mode (void)
{
    raw ();
}

void mc_noraw_mode (void)
{
    noraw ();
}

/* This flag is set by xterm detection routine in function main() */
/* It is used by function view_other_cmd() */
int xterm_flag = 0;

/* The following routines only work on xterm terminals */

void do_enter_ca_mode (void)
{
    if (!xterm_flag)
	return;
    fprintf (stdout, /* ESC_STR ")0" */ ESC_STR "7" ESC_STR "[?47h");
    fflush (stdout);
}

void do_exit_ca_mode (void)
{
    if (!xterm_flag)
	return;
    fprintf (stdout, ESC_STR "[?47l" ESC_STR "8" ESC_STR "[m");
    fflush (stdout);
}

/* This table is a mapping between names and the constants we use
 * We use this to allow users to define alternate definitions for
 * certain keys that may be missing from the terminal database
 */
key_code_name_t key_name_conv_tab [] = {
/* KEY_F(0) is not here, since we are mapping it to f10, so there is no reason
   to define f0 as well. Also, it makes Learn keys a bunch of problems :( */
    { KEY_F(1),      "f1",         N_("Function key 1") },
    { KEY_F(2),      "f2",         N_("Function key 2") },
    { KEY_F(3),      "f3",         N_("Function key 3") },
    { KEY_F(4),      "f4",         N_("Function key 4") },
    { KEY_F(5),      "f5",         N_("Function key 5") },
    { KEY_F(6),      "f6",         N_("Function key 6") },
    { KEY_F(7),      "f7",         N_("Function key 7") },
    { KEY_F(8),      "f8",         N_("Function key 8") },
    { KEY_F(9),      "f9",         N_("Function key 9") },
    { KEY_F(10),     "f10",        N_("Function key 10") },
    { KEY_F(11),     "f11",        N_("Function key 11") },
    { KEY_F(12),     "f12",        N_("Function key 12") },
    { KEY_F(13),     "f13",        N_("Function key 13") },
    { KEY_F(14),     "f14",        N_("Function key 14") },
    { KEY_F(15),     "f15",        N_("Function key 15") },
    { KEY_F(16),     "f16",        N_("Function key 16") },
    { KEY_F(17),     "f17",        N_("Function key 17") },
    { KEY_F(18),     "f18",        N_("Function key 18") },
    { KEY_F(19),     "f19",        N_("Function key 19") },
    { KEY_F(20),     "f20",        N_("Function key 20") },
    { KEY_BACKSPACE, "bs",         N_("Backspace key") },
    { KEY_END,       "end",        N_("End key") },
    { KEY_UP,        "up",         N_("Up arrow key") },
    { KEY_DOWN,      "down",       N_("Down arrow key") },
    { KEY_LEFT,      "left",       N_("Left arrow key") },
    { KEY_RIGHT,     "right",      N_("Right arrow key") },
    { KEY_HOME,      "home",   	   N_("Home key") },
    { KEY_NPAGE,     "pgdn",   	   N_("Page Down key") },
    { KEY_PPAGE,     "pgup",   	   N_("Page Up key") },
    { KEY_IC,        "insert",     N_("Insert key") },
    { KEY_DC,        "delete",     N_("Delete key") },
    { ALT('\t'),     "complete",   N_("Completion/M-tab") },
    { KEY_KP_ADD,    "kpplus",     N_("+ on keypad") },
    { KEY_KP_SUBTRACT,"kpminus",   N_("- on keypad") },
    { KEY_KP_MULTIPLY,"kpasterix", N_("* on keypad") },
/* From here on, these won't be shown in Learn keys (no space) */    
    { KEY_LEFT,	     "kpleft",     N_("Left arrow keypad") },
    { KEY_RIGHT,     "kpright",    N_("Right arrow keypad") },
    { KEY_UP,	     "kpup",       N_("Up arrow keypad") },
    { KEY_DOWN,	     "kpdown",     N_("Down arrow keypad") },
    { KEY_HOME,	     "kphome",     N_("Home on keypad") },
    { KEY_END,	     "kpend",      N_("End on keypad") },
    { KEY_NPAGE,     "kpnpage",    N_("Page Down keypad") },
    { KEY_PPAGE,     "kpppage",    N_("Page Up keypad") },
    { KEY_IC,        "kpinsert",   N_("Insert on keypad") },
    { KEY_DC,        "kpdelete",   N_("Delete on keypad") },
    { (int) '\n',    "kpenter",    N_("Enter on keypad") },
    { (int) '/',     "kpslash",    N_("Slash on keypad") },
    { (int) '#',     "kpnumlock",  N_("NumLock on keypad") },
    { 0, 0, 0 }
    };

/* Return the code associated with the symbolic name keyname */
int lookup_key (char *keyname)
{
    int i;

    for (i = 0; key_name_conv_tab [i].code; i++){
	if (str_casecmp (key_name_conv_tab [i].name, keyname))
	    continue;
	return key_name_conv_tab [i].code;
    }
    return 0;
}

