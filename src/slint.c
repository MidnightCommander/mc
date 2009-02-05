/* Slang interface to the Midnight Commander
   This emulates some features of ncurses on top of slang
   
   Copyright (C) 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2007 Free Software Foundation, Inc.

   Author Miguel de Icaza
          Norbert Warmuth

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

/** \file slint.c
 *  \brief Source: slang interface to the Midnight Commander
 *  \warning This module will be removed soon
 */

#include <config.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <unistd.h>

#include "global.h"
#include "tty.h"
#include "color.h"
#include "mouse.h"		/* Gpm_Event is required in key.h */
#include "key.h"		/* define_sequence */
#include "main.h"		/* extern: force_colors */
#include "win.h"		/* do_exit_ca_mode */
#include "setup.h"
#include "background.h"		/* we_are_background */

#ifdef HAVE_SLANG

/* Taken from S-Lang's slutty.c */
#ifdef ultrix   /* Ultrix gets _POSIX_VDISABLE wrong! */
# define NULL_VALUE -1
#else
# ifdef _POSIX_VDISABLE
#  define NULL_VALUE _POSIX_VDISABLE
# else
#  define NULL_VALUE 255
# endif
#endif

#ifndef SA_RESTART
#    define SA_RESTART 0
#endif

/* Various saved termios settings that we control here */
static struct termios boot_mode;
static struct termios new_mode;

/* Controls whether we should wait for input in getch */
static int no_slang_delay;

/* Forward declarations */
static void load_terminfo_keys (void);

#ifndef HAVE_SLANG_PRIVATE
/* Private interfaces have been stripped, so we cannot use them */
#define SLang_getkey2() SLang_getkey()
#define SLang_input_pending2(s) SLang_input_pending(s)
#else
/* Copied from ../slang/slgetkey.c, removed the DEC_8Bit_HACK. */
extern unsigned char SLang_Input_Buffer [];
#if SLANG_VERSION >= 10000
extern unsigned int _SLsys_getkey (void);
extern int _SLsys_input_pending (int);
#else
extern unsigned int SLsys_getkey (void);
extern int SLsys_input_pending (int);
#endif

static unsigned int SLang_getkey2 (void)
{
   unsigned int imax;
   unsigned int ch;
   
   if (SLang_Input_Buffer_Len)
     {
	ch = (unsigned int) *SLang_Input_Buffer;
	SLang_Input_Buffer_Len--;
	imax = SLang_Input_Buffer_Len;
   
	memmove ((char *) SLang_Input_Buffer, 
		(char *) (SLang_Input_Buffer + 1), imax);
	return(ch);
     }
#if SLANG_VERSION >= 10000
   else return(_SLsys_getkey ());
#else
   else return(SLsys_getkey());
#endif
}

static int SLang_input_pending2 (int tsecs)
{
   int n, i;
   unsigned char c;

   if (SLang_Input_Buffer_Len) return (int) SLang_Input_Buffer_Len;
#if SLANG_VERSION >= 10000  
   n = _SLsys_input_pending (tsecs);
#else
   n = SLsys_input_pending (tsecs);
#endif
   if (n <= 0) return 0;
   
   i = SLang_getkey2 ();
   if (i == SLANG_GETKEY_ERROR)
	return 0;  /* don't put crippled error codes into the input buffer */
   c = (unsigned char)i;
   SLang_ungetkey_string (&c, 1);
   
   return n;
}
#endif /* HAVE_SLANG_PRIVATE */

/* Only done the first time */
void
slang_init (void)
{
    SLtt_get_terminfo ();

   /*
    * If the terminal in not in terminfo but begins with a well-known
    * string such as "linux" or "xterm" S-Lang will go on, but the
    * terminal size and several other variables won't be initialized
    * (as of S-Lang 1.4.4). Detect it and abort. Also detect extremely
    * small, large and negative screen dimensions.
    */
    if ((COLS < 10) || (LINES < 5) ||
    (SLang_Version < 10407 && (COLS > 255 || LINES > 255)) ||
    (SLang_Version >= 10407 && (COLS > 512 || LINES > 512))) {

	fprintf (stderr,
		 _("Screen size %dx%d is not supported.\n"
		   "Check the TERM environment variable.\n"),
		 COLS, LINES);
	exit (1);
    }

    tcgetattr (fileno (stdin), &boot_mode);
    /* 255 = ignore abort char; XCTRL('g') for abort char = ^g */
    SLang_init_tty (XCTRL('c'), 1, 0);	
    
    /* If SLang uses fileno(stderr) for terminal input MC will hang
       if we call SLang_getkey between calls to open_error_pipe and
       close_error_pipe, e.g. when we do a growing view of an gzipped
       file. */
    if (SLang_TT_Read_FD == fileno (stderr))
	SLang_TT_Read_FD = fileno (stdin);

    if (force_ugly_line_drawing)
	SLtt_Has_Alt_Charset = 0;
    if (tcgetattr (SLang_TT_Read_FD, &new_mode) == 0) {
#ifdef VDSUSP
	new_mode.c_cc[VDSUSP] = NULL_VALUE;   /* to ignore ^Y */
#endif
#ifdef VLNEXT
	new_mode.c_cc[VLNEXT] = NULL_VALUE;   /* to ignore ^V */
#endif
	tcsetattr (SLang_TT_Read_FD, TCSADRAIN, &new_mode);
    }
    slang_prog_mode ();
    load_terminfo_keys ();
    SLtt_Blink_Mode = 0;
}

void
slang_set_raw_mode (void)
{
    tcsetattr (SLang_TT_Read_FD, TCSANOW, &new_mode);
}

/* Done each time we come back from done mode */
void
slang_prog_mode (void)
{
    tcsetattr (SLang_TT_Read_FD, TCSANOW, &new_mode);
    SLsmg_init_smg ();
    SLsmg_touch_lines (0, LINES);
}

/* Called each time we want to shutdown slang screen manager */
void
slang_shell_mode (void)
{
    tcsetattr (SLang_TT_Read_FD, TCSANOW, &boot_mode);
}

void
slang_shutdown (void)
{
    char *op_cap;
    
    slang_shell_mode ();
    do_exit_ca_mode ();
    SLang_reset_tty ();

    /* Load the op capability to reset the colors to those that were
     * active when the program was started up
     */
    op_cap = SLtt_tgetstr ("op");
    if (op_cap){
	fprintf (stdout, "%s", op_cap);
	fflush (stdout);
    }
}

/* HP Terminals have capabilities (pfkey, pfloc, pfx) to program function keys.
   elm 2.4pl15 invoked with the -K option utilizes these softkeys and the
   consequence is that function keys don't work in MC sometimes. 
   Unfortunately I don't now the one and only escape sequence to turn off 
   softkeys (elm uses three different capabilities to turn on softkeys and two 
   capabilities to turn them off). 
   Among other things elm uses the pair we already use in slang_keypad. That's 
   the reason why I call slang_reset_softkeys from slang_keypad. In lack of
   something better the softkeys are programmed to their defaults from the
   termcap/terminfo database.
   The escape sequence to program the softkeys is taken from elm and it is 
   hardcoded because neither slang nor ncurses 4.1 know how to 'printf' this 
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
	if (send) {
	    g_snprintf (tmp, sizeof (tmp), "\033&f%dk%dd%dL%s%s", key,
			(int) (sizeof (display) - 1), (int) strlen (send),
			display, send);
	    SLtt_write_string (tmp);
	}
    }
}

/* keypad routines */
void
slang_keypad (int set)
{
    char *keypad_string;
    extern int reset_hp_softkeys;
    
    keypad_string = (char *) SLtt_tgetstr (set ? "ks" : "ke");
    if (keypad_string)
	SLtt_write_string (keypad_string);
    if (set && reset_hp_softkeys)
	slang_reset_softkeys ();
}

void
set_slang_delay (int v)
{
    no_slang_delay = v;
}

void
hline (int ch, int len)
{
    int last_x, last_y;

    last_x = SLsmg_get_column ();
    last_y = SLsmg_get_row ();
    
    if (ch == 0)
	ch = ACS_HLINE;

    if (ch == ACS_HLINE){
	SLsmg_draw_hline (len);
    } else {
	while (len--)
	    addch (ch);
    }
    move (last_y, last_x);
}

void
vline (int character, int len)
{
    if (!slow_terminal){
	SLsmg_draw_vline (len);
    } else {
	int last_x, last_y, pos = 0;

	last_x = SLsmg_get_column ();
	last_y = SLsmg_get_row ();

	while (len--){
	    move (last_y + pos++, last_x);
	    addch (' ');
	}
	move (last_x, last_y);
    }
}

int has_colors (void)
{
    const char *terminal = getenv ("TERM");
    char *cts = color_terminal_string, *s;
    size_t i;

    if (force_colors)
       SLtt_Use_Ansi_Colors = 1;
    if (NULL != getenv ("COLORTERM"))
	SLtt_Use_Ansi_Colors = 1;

    /* We want to allow overwriding */
    if (!disable_colors){
      /* check color_terminal_string */
      if (*cts)
      {
        while (*cts)
        {
          while (*cts == ' ' || *cts == '\t') cts++;

          s = cts;
          i = 0;

          while (*cts && *cts != ',')
          {
            cts++;
            i++;
          }
          if (i && i == strlen(terminal) && strncmp(s, terminal, i) == 0)
            SLtt_Use_Ansi_Colors = 1;

          if (*cts == ',') cts++;
        }
      }
    }
    
    /* Setup emulated colors */
    if (SLtt_Use_Ansi_Colors){
        if (use_colors){
	    mc_init_pair (A_REVERSE, "black", "white");
	    mc_init_pair (A_BOLD, "white", "black");
	} else {
	    mc_init_pair (A_REVERSE, "black", "lightgray");
	    mc_init_pair (A_BOLD, "white", "black");
	    mc_init_pair (A_BOLD_REVERSE, "white", "lightgray");
	} 
    } else {
	SLtt_set_mono (A_BOLD,    NULL, SLTT_BOLD_MASK);
	SLtt_set_mono (A_REVERSE, NULL, SLTT_REV_MASK);
	SLtt_set_mono (A_BOLD|A_REVERSE, NULL, SLTT_BOLD_MASK | SLTT_REV_MASK);
    }
    return SLtt_Use_Ansi_Colors;
}

void
attrset (int color)
{
    if (!SLtt_Use_Ansi_Colors){
	SLsmg_set_color (color);
	return;
    }
    
    if (color & A_BOLD){
	if (color == A_BOLD)
	    SLsmg_set_color (A_BOLD);
	else
	    SLsmg_set_color ((color & (~A_BOLD)) + 8);
	return;
    }

    if (color == A_REVERSE)
	SLsmg_set_color (A_REVERSE);
    else
	SLsmg_set_color (color);
}

/* This table describes which capabilities we want and which values we
 * assign to them.
 */
static const struct {
    int  key_code;
    const char *key_name;
} key_table [] = {
    { KEY_F(0),  "k0" },
    { KEY_F(1),  "k1" },
    { KEY_F(2),  "k2" },
    { KEY_F(3),  "k3" },
    { KEY_F(4),  "k4" },
    { KEY_F(5),  "k5" },
    { KEY_F(6),  "k6" },
    { KEY_F(7),  "k7" },
    { KEY_F(8),  "k8" },
    { KEY_F(9),  "k9" },
    { KEY_F(10), "k;" },
    { KEY_F(11), "F1" },
    { KEY_F(12), "F2" },
    { KEY_F(13), "F3" },
    { KEY_F(14), "F4" },
    { KEY_F(15), "F5" },
    { KEY_F(16), "F6" },
    { KEY_F(17), "F7" },
    { KEY_F(18), "F8" },
    { KEY_F(19), "F9" },
    { KEY_F(20), "FA" },	
    { KEY_IC,    "kI" },
    { KEY_NPAGE, "kN" },
    { KEY_PPAGE, "kP" },
    { KEY_LEFT,  "kl" },
    { KEY_RIGHT, "kr" },
    { KEY_UP,    "ku" },
    { KEY_DOWN,  "kd" },
    { KEY_DC,    "kD" },
    { KEY_BACKSPACE, "kb" },
    { KEY_HOME,  "kh" },
    { KEY_END,	 "@7" },
    { 0, 0}
};
	
static void
do_define_key (int code, const char *strcap)
{
    char    *seq;

    seq = (char *) SLtt_tgetstr ((char*) strcap);
    if (seq)
	define_sequence (code, seq, MCKEY_NOACTION);
}

static void
load_terminfo_keys (void)
{
    int i;

    for (i = 0; key_table [i].key_code; i++)
	do_define_key (key_table [i].key_code, key_table [i].key_name);
}

int
getch (void)
{
    int c;
    if (no_slang_delay)
	if (SLang_input_pending2 (0) == 0)
	    return -1;

    c = SLang_getkey2 ();
    if (c == SLANG_GETKEY_ERROR) {
	fprintf (stderr,
		 "SLang_getkey returned SLANG_GETKEY_ERROR\n"
		 "Assuming EOF on stdin and exiting\n");
	exit (1);
    }
    return c;
}
#endif /* HAVE_SLANG */

void
mc_refresh (void)
{
#ifdef WITH_BACKGROUND
    if (!we_are_background)
#endif				/* WITH_BACKGROUND */
	refresh ();
}
