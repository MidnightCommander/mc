/*
   Interface to the terminal controlling library.
   Slang wrapper.

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
 *  \brief Source: S-Lang-based tty layer of Midnight Commander
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <sys/types.h>			/* size_t */
#include <unistd.h>
#include <signal.h>

#include "../../src/global.h"

#include "../../src/tty/tty-internal.h"		/* slow_tty */
#include "../../src/tty/tty.h"			/* tty_draw_box_slow */
#include "../../src/tty/color-slang.h"
#include "../../src/tty/color-internal.h"
#include "../../src/tty/mouse.h"	/* Gpm_Event is required in key.h */
#include "../../src/tty/key.h"		/* define_sequence */
#include "../../src/tty/win.h"

#include "../../src/strutil.h"		/* str_term_form */

/*** global variables **************************************************/
extern int reset_hp_softkeys;

/*** file scope macro definitions **************************************/

/* Taken from S-Lang's slutty.c */
#ifdef ultrix   /* Ultrix gets _POSIX_VDISABLE wrong! */
#   define NULL_VALUE -1
#else
#   ifdef _POSIX_VDISABLE
#	define NULL_VALUE _POSIX_VDISABLE
#   else
#	define NULL_VALUE 255
#   endif
#endif				/* ultrix */

#ifndef SA_RESTART
#   define SA_RESTART 0
#endif

#ifndef SLTT_MAX_SCREEN_COLS
#define SLTT_MAX_SCREEN_COLS 512
#endif

#ifndef SLTT_MAX_SCREEN_ROWS
#define SLTT_MAX_SCREEN_ROWS 512
#endif


/*** file scope type declarations **************************************/

/*** file scope variables **********************************************/

/* Various saved termios settings that we control here */
static struct termios boot_mode;
static struct termios new_mode;

/* Controls whether we should wait for input in tty_lowlevel_getch */
static gboolean no_slang_delay;

/* This table describes which capabilities we want and which values we
 * assign to them.
 */
static const struct {
    int  key_code;
    const char *key_name;
} key_table [] = {
    { KEY_F(0),      "k0" },
    { KEY_F(1),      "k1" },
    { KEY_F(2),      "k2" },
    { KEY_F(3),      "k3" },
    { KEY_F(4),      "k4" },
    { KEY_F(5),      "k5" },
    { KEY_F(6),      "k6" },
    { KEY_F(7),      "k7" },
    { KEY_F(8),      "k8" },
    { KEY_F(9),      "k9" },
    { KEY_F(10),     "k;" },
    { KEY_F(11),     "F1" },
    { KEY_F(12),     "F2" },
    { KEY_F(13),     "F3" },
    { KEY_F(14),     "F4" },
    { KEY_F(15),     "F5" },
    { KEY_F(16),     "F6" },
    { KEY_F(17),     "F7" },
    { KEY_F(18),     "F8" },
    { KEY_F(19),     "F9" },
    { KEY_F(20),     "FA" },
    { KEY_IC,        "kI" },
    { KEY_NPAGE,     "kN" },
    { KEY_PPAGE,     "kP" },
    { KEY_LEFT,      "kl" },
    { KEY_RIGHT,     "kr" },
    { KEY_UP,        "ku" },
    { KEY_DOWN,      "kd" },
    { KEY_DC,        "kD" },
    { KEY_BACKSPACE, "kb" },
    { KEY_HOME,      "kh" },
    { KEY_END,       "@7" },
    { 0,             NULL }
};

/*** file scope functions **********************************************/

/* HP Terminals have capabilities (pfkey, pfloc, pfx) to program function keys.
   elm 2.4pl15 invoked with the -K option utilizes these softkeys and the
   consequence is that function keys don't work in MC sometimes...
   Unfortunately I don't now the one and only escape sequence to turn off.
   softkeys (elm uses three different capabilities to turn on softkeys and two.
   capabilities to turn them off)..
   Among other things elm uses the pair we already use in slang_keypad. That's.
   the reason why I call slang_reset_softkeys from slang_keypad. In lack of
   something better the softkeys are programmed to their defaults from the
   termcap/terminfo database.
   The escape sequence to program the softkeys is taken from elm and it is.
   hardcoded because neither slang nor ncurses 4.1 know how to 'printf' this.
   sequence. -- Norbert
 */

static void
slang_reset_softkeys (void)
{
    int key;
    char *send;
    static const char display[] = "                ";
    char tmp[BUF_SMALL];

    for (key = 1; key < 9; key++) {
	g_snprintf (tmp, sizeof (tmp), "k%d", key);
	send = (char *) SLtt_tgetstr (tmp);
	if (send != NULL) {
	    g_snprintf (tmp, sizeof (tmp), "\033&f%dk%dd%dL%s%s", key,
			(int) (sizeof (display) - 1), (int) strlen (send),
			display, send);
	    SLtt_write_string (tmp);
	}
    }
}

static void
do_define_key (int code, const char *strcap)
{
    char *seq;

    seq = (char *) SLtt_tgetstr ((char *) strcap);
    if (seq != NULL)
	define_sequence (code, seq, MCKEY_NOACTION);
}

static void
load_terminfo_keys (void)
{
    int i;

    for (i = 0; key_table [i].key_code; i++)
	do_define_key (key_table [i].key_code, key_table [i].key_name);
}

/*** public functions **************************************************/

void
tty_init (gboolean slow, gboolean ugly_lines)
{
    slow_tty = slow;
    ugly_line_drawing = ugly_lines;

    SLtt_get_terminfo ();
    SLutf8_enable (-1);
   /*
    * If the terminal in not in terminfo but begins with a well-known
    * string such as "linux" or "xterm" S-Lang will go on, but the
    * terminal size and several other variables won't be initialized
    * (as of S-Lang 1.4.4). Detect it and abort. Also detect extremely
    * small, large and negative screen dimensions.
    */
    if ((COLS < 10) || (LINES < 5)
	|| (COLS > SLTT_MAX_SCREEN_COLS) || (LINES > SLTT_MAX_SCREEN_ROWS)) {
	fprintf (stderr,
		_("Screen size %dx%d is not supported.\n"
		    "Check the TERM environment variable.\n"),
		    COLS, LINES);
	exit (1);
    }

    tcgetattr (fileno (stdin), &boot_mode);
    /* 255 = ignore abort char; XCTRL('g') for abort char = ^g */
    SLang_init_tty (XCTRL('c'), 1, 0);

    if (ugly_lines)
	SLtt_Has_Alt_Charset = 0;

    /* If SLang uses fileno(stderr) for terminal input MC will hang
       if we call SLang_getkey between calls to open_error_pipe and
       close_error_pipe, e.g. when we do a growing view of an gzipped
       file. */
    if (SLang_TT_Read_FD == fileno (stderr))
	SLang_TT_Read_FD = fileno (stdin);

    if (tcgetattr (SLang_TT_Read_FD, &new_mode) == 0) {
#ifdef VDSUSP
	new_mode.c_cc[VDSUSP] = NULL_VALUE;	/* to ignore ^Y */
#endif
#ifdef VLNEXT
	new_mode.c_cc[VLNEXT] = NULL_VALUE;	/* to ignore ^V */
#endif
	tcsetattr (SLang_TT_Read_FD, TCSADRAIN, &new_mode);
    }

    tty_reset_prog_mode ();
    load_terminfo_keys ();
    SLtt_Blink_Mode = 0;

    tty_start_interrupt_key ();

    /* It's the small part from the previous init_key() */
    init_key_input_fd ();

    SLsmg_init_smg ();
    do_enter_ca_mode ();
    tty_keypad (TRUE);
    tty_nodelay (FALSE);
}

void
tty_shutdown (void)
{
    char *op_cap;

    SLsmg_reset_smg ();
    tty_reset_shell_mode ();
    do_exit_ca_mode ();
    SLang_reset_tty ();

    /* Load the op capability to reset the colors to those that were 
     * active when the program was started up 
     */
    op_cap = SLtt_tgetstr ((char *) "op");
    if (op_cap != NULL){
	fputs (op_cap, stdout);
	fflush (stdout);
    }
}

/* Done each time we come back from done mode */
void
tty_reset_prog_mode (void)
{
    tcsetattr (SLang_TT_Read_FD, TCSANOW, &new_mode);
    SLsmg_init_smg ();
    SLsmg_touch_lines (0, LINES);
}

/* Called each time we want to shutdown slang screen manager */
void
tty_reset_shell_mode (void)
{
    tcsetattr (SLang_TT_Read_FD, TCSANOW, &boot_mode);
}

void
tty_raw_mode (void)
{
    tcsetattr (SLang_TT_Read_FD, TCSANOW, &new_mode);
}

void
tty_noraw_mode (void)
{
}

void
tty_noecho (void)
{
}

int
tty_flush_input (void)
{
    return 0; /* OK */
}

void
tty_keypad (gboolean set)
{
    char *keypad_string;

    keypad_string = (char *) SLtt_tgetstr ((char *) (set ? "ks" : "ke"));
    if (keypad_string != NULL)
	SLtt_write_string (keypad_string);
    if (set && reset_hp_softkeys)
	slang_reset_softkeys ();
}

void
tty_nodelay (gboolean set)
{
    no_slang_delay = set;
}

int
tty_baudrate (void)
{
    return SLang_TT_Baud_Rate;
}

int
tty_lowlevel_getch (void)
{
    int c;

    if (no_slang_delay && (SLang_input_pending (0) == 0))
	return -1;

    c = SLang_getkey ();
    if (c == SLANG_GETKEY_ERROR) {
	fprintf (stderr,
		    "SLang_getkey returned SLANG_GETKEY_ERROR\n"
		    "Assuming EOF on stdin and exiting\n");
	exit (1);
    }

    return c;
}

int
tty_reset_screen (void)
{
    SLsmg_reset_smg ();
    return 0; /* OK */
}

void
tty_touch_screen (void)
{
    SLsmg_touch_lines (0, LINES);
}

void
tty_gotoyx (int y, int x)
{
    SLsmg_gotorc (y, x);
}

void
tty_getyx (int *py, int *px)
{
    *py = SLsmg_get_row ();
    *px = SLsmg_get_column ();
}

/* if x < 0 or y < 0, draw line staring from current position */
void
tty_draw_hline (int y, int x, int ch, int len)
{
    if (ch == ACS_HLINE && (ugly_line_drawing || slow_tty)) {
        ch = mc_tty_ugly_frm[MC_TTY_FRM_thinhoriz];
    }

    if ((y < 0) || (x < 0)) {
	y = SLsmg_get_row ();
	x = SLsmg_get_column ();
    } else
	SLsmg_gotorc (y, x);

    if (ch == 0)
	ch = ACS_HLINE;

    if (ch == ACS_HLINE)
	SLsmg_draw_hline (len);
    else
	while (len-- != 0)
	    tty_print_char (ch);

    SLsmg_gotorc (y, x);
}

/* if x < 0 or y < 0, draw line staring from current position */
void
tty_draw_vline (int y, int x, int ch, int len)
{
    if ((y < 0) || (x < 0)) {
	y = SLsmg_get_row ();
	x = SLsmg_get_column ();
    } else
	SLsmg_gotorc (y, x);

    if (ch == 0)
	ch = ACS_VLINE;

    if (ch == ACS_VLINE)
	SLsmg_draw_vline (len);
    else {
	int pos = 0;

	while (len-- != 0) {
	    SLsmg_gotorc (y + pos, x);
	    tty_print_char (ch);
	    pos++;
	}
    }

    SLsmg_gotorc (y, x);
}

void
tty_draw_box (int y, int x, int rows, int cols)
{
    /* this fix slang drawing stickchars bug */
    if (ugly_line_drawing || slow_tty)
	tty_draw_box_slow (y, x, rows, cols);
    else
	SLsmg_draw_box (y, x, rows, cols);
}

void
tty_fill_region (int y, int x, int rows, int cols, unsigned char ch)
{
    SLsmg_fill_region (y, x, rows, cols, ch);
}

void
tty_set_alt_charset (gboolean alt_charset)
{
    SLsmg_set_char_set ((int) alt_charset);
}

void
tty_display_8bit (gboolean what)
{
    SLsmg_Display_Eight_Bit = what ? 128 : 160;
}

void
tty_print_char (int c)
{
    SLsmg_write_char ((SLwchar_Type) ((unsigned int) c));
}

void
tty_print_alt_char (int c)
{
    if (c == ACS_RTEE && (ugly_line_drawing || slow_tty)) {
        c = mc_tty_ugly_frm[MC_TTY_FRM_rightmiddle];
    }

    if (c == ACS_LTEE && (ugly_line_drawing || slow_tty)) {
        c = mc_tty_ugly_frm[MC_TTY_FRM_leftmiddle];
    }
    if (ugly_line_drawing || slow_tty) {
        tty_print_char (c);
    } else {
        SLsmg_draw_object (SLsmg_get_row(), SLsmg_get_column(), c);
    }
}

void
tty_print_anychar (int c)
{
    char str[6 + 1];

    if ( c > 255 ) {
        int res = g_unichar_to_utf8 (c, str);
        if ( res == 0 ) {
            str[0] = '.';
            str[1] = '\0';
        } else {
            str[res] = '\0';
        }
        SLsmg_write_string ((char *) str_term_form (str));
    } else {
        SLsmg_write_char ((SLwchar_Type) ((unsigned int) c));
    }
}

void
tty_print_string (const char *s)
{
    SLsmg_write_string ((char *) str_term_form (s));
}

void
tty_printf (const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    SLsmg_vprintf ((char *) fmt, args);
    va_end (args);
}

char *
tty_tgetstr (const char *cap)
{
    return SLtt_tgetstr ((char *) cap);
}

void
tty_refresh (void)
{
    SLsmg_refresh ();
}

void
tty_setup_sigwinch (void (*handler) (int))
{
#ifdef SIGWINCH
    struct sigaction act, oact;
    act.sa_handler = handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
#ifdef SA_RESTART
    act.sa_flags |= SA_RESTART;
#endif		/* SA_RESTART */
    sigaction (SIGWINCH, &act, &oact);
#endif		/* SIGWINCH */
}

void
tty_beep (void)
{
    SLtt_beep ();
}
