/* Keyboard support routines.
	for Windows NT system.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  

  Bugs:
    Have trouble with non-US keyboards, "Alt-gr"+keys (API tells CTRL-ALT is pressed)
   */
#include <config.h>
#ifndef _OS_NT
#error This file is for Win32 systems.
#else

#include <windows.h>
#include <stdio.h>
#include "mouse.h"
#include "global.h"
#include "main.h"
#include "key.h"
#include "../vfs/vfs.h"
#include "tty.h"
#include "util.debug.h"

/*  Global variables */
int old_esc_mode = 0;
HANDLE hConsoleInput;
DWORD  dwSaved_ControlState;
Gpm_Event evSaved_Event;

/* Unused variables */
int double_click_speed;		/* they are here to keep linker happy */
int mou_auto_repeat;
int use_8th_bit_as_meta = 0;

/*  Prototypes */
static int EscapeKey (char* seq);
static int ControlKey (char* seq);
static int AltKey (char *seq);

static int VKtoCurses (int vkcode);
static int correct_key_code (int c);


/* Static Tables */
struct {
    int key_code;
    int vkcode;
} key_table [] = {
    { KEY_F(1),  VK_F1 },
    { KEY_F(2),  VK_F2 },
    { KEY_F(3),  VK_F3 },
    { KEY_F(4),  VK_F4 },
    { KEY_F(5),  VK_F5 },
    { KEY_F(6),  VK_F6 },
    { KEY_F(7),  VK_F7 },
    { KEY_F(8),  VK_F8 },
    { KEY_F(9),  VK_F9 },
    { KEY_F(10), VK_F10 },
    { KEY_F(11), VK_F11 },
    { KEY_F(12), VK_F12 },
    { KEY_F(13), VK_F13 }, 
    { KEY_F(14), VK_F14 },
    { KEY_F(15), VK_F15 },
    { KEY_F(16), VK_F16 },
    { KEY_F(17), VK_F17 },
    { KEY_F(18), VK_F18 },
    { KEY_F(19), VK_F19 },
    { KEY_F(20), VK_F20 },	
    { KEY_IC,    VK_INSERT },		
    { KEY_DC,    VK_DELETE },
    { KEY_BACKSPACE, VK_BACK },
    { '\t',      VK_TAB },
//    { KEY_ENTER, VK_RETURN },
//    { KEY_ENTER, VK_EXECUTE },
    { '\n', VK_RETURN },
    { '\n', VK_EXECUTE },
    { ' ',    	 VK_SPACE },
//    { KEY_PRINT, VK_SNAPSHOT },

    { KEY_PPAGE, VK_PRIOR },		// Movement keys
    { KEY_NPAGE, VK_NEXT },
    { KEY_LEFT,  VK_LEFT },
    { KEY_RIGHT, VK_RIGHT },
    { KEY_UP,    VK_UP },
    { KEY_DOWN,  VK_DOWN },
    { KEY_HOME,  VK_HOME },
    { KEY_END,	 VK_END },

    { KEY_KP_MULTIPLY,  VK_MULTIPLY },		// Numeric pad
    { KEY_KP_ADD,  VK_ADD },
    { KEY_KP_SUBTRACT,  VK_SUBTRACT },

/* Control key codes */
    { 0, VK_CONTROL },			/* Control */
    { 0, VK_MENU },			/* Alt     */
//    { 0, VK_ESCAPE },			/* ESC	   */
    { ESC_CHAR, VK_ESCAPE },			/* ESC	   */

/* Key codes to ignore */
    { -1, VK_SHIFT },			/* Shift when released generates a key event */
    { -1, VK_CAPITAL },			/* Caps-lock */

#if WINVER >= 0x400			/* new Chicago key codes (not in 3.x headers) */
    { -1, VK_APPS },			/* "Application key" */
    { -1, VK_LWIN },			/* Left "Windows" key */
    { -1, VK_RWIN },			/* Right "Windows" key */
#endif
    { 0, 0}
};		

/* Special handlers for control key codes 
	Note that howmany must be less than seq_buffer len
*/
struct {
    int vkcode;
    int (*func_hdlr)(char *);	
    int howmany;
} key_control_table[] = {
    { VK_ESCAPE, EscapeKey, 1 },
    { VK_CONTROL, ControlKey, 1 },
    { VK_MENU, AltKey, 1 },
    { 0, NULL, 0},
};

/*  init_key  - Called in main.c to initialize ourselves
		Get handle to console input
*/
void init_key (void)
{
    win32APICALL_HANDLE (hConsoleInput, /* = */ GetStdHandle (STD_INPUT_HANDLE));
}

/* The maximum sequence length (32 + null terminator) */
static int seq_buffer[33];
static int *seq_append = 0;

static int push_char (int c)
{
    if (!seq_append)
	seq_append = seq_buffer;
    
    if (seq_append == &(seq_buffer [sizeof (seq_buffer)-2]))
	return 0;
    *(seq_append++) = c;
    *seq_append = 0;
    return 1;
}

void define_sequence (int code, char* vkcode, int action)
{
}

static int *pending_keys;

int get_key_code (int no_delay)
{
    INPUT_RECORD ir;				/* Input record */
    DWORD		 dw;				/* number of records actually read */
    int			 ch, vkcode, j;

    if (no_delay) {
        /* Check if any input pending, otherwise return */
	nodelay (stdscr, TRUE);
	win32APICALL(PeekConsoleInput(hConsoleInput, &ir, 1, &dw));
	if (!dw)
	    return 0;
    }
 

pend_send:
    if (pending_keys) {
	int d = *pending_keys++;
	if (!*pending_keys){
	    pending_keys = 0;
	    seq_append = 0;
	}
	return d;
    }


    do {
	win32APICALL(ReadConsoleInput(hConsoleInput, &ir, 1, &dw));
	switch (ir.EventType) {
 	    case KEY_EVENT:
		if (!ir.Event.KeyEvent.bKeyDown)		/* Process key just once: when pressed */
		    break;

		vkcode = ir.Event.KeyEvent.wVirtualKeyCode;
		ch = ir.Event.KeyEvent.uChar.AsciiChar;
		dwSaved_ControlState = ir.Event.KeyEvent.dwControlKeyState;

		j = VKtoCurses(vkcode);
		return j ? j : ch;

	case MOUSE_EVENT:
		// Save event as a GPM-like event
		evSaved_Event.x = ir.Event.MouseEvent.dwMousePosition.X;
		evSaved_Event.y = ir.Event.MouseEvent.dwMousePosition.Y+1;
		evSaved_Event.buttons = ir.Event.MouseEvent.dwButtonState;
		switch (ir.Event.MouseEvent.dwEventFlags) {
		    case 0:
			evSaved_Event.type = GPM_DOWN | GPM_SINGLE;
			break;
		    case MOUSE_MOVED:
			evSaved_Event.type = GPM_MOVE;
			break;
		    case DOUBLE_CLICK:
			evSaved_Event.type = GPM_DOWN | GPM_DOUBLE;
			break;
		};
		return 0;	
	}
    } while (!no_delay);
    return 0;
}

static int VKtoCurses (int a_vkc)
{
    int i;

    // Replace key code with that in table
    for (i=0;  key_table[i].vkcode != 0 || key_table[i].key_code != 0; i++) 
	if (a_vkc == key_table[i].vkcode) {
		if (key_table[i].key_code)
			return correct_key_code (key_table[i].key_code);
	}
	return 0;
}

static int correct_key_code (int c)
{
    /* Check Control State */
    if (ctrl_pressed())
	if (c == '\t')
	/* Make Ctrl-Tab same as reserved Alt-Tab */
	    c = ALT('\t');
	else
	    c = XCTRL(c);

    if (alt_pressed())
	c = ALT(c);

    if (shift_pressed() && (c >= KEY_F(1)) && (c <= KEY_F(10)))
		c += 10;
    if (alt_pressed() && (c >= KEY_F(1)) && (c <= KEY_F(2)))
		c += 10;
    if (alt_pressed() && (c == KEY_F(7)))
		c = ALT('?');
    switch (c) {
        case KEY_KP_ADD: 
        	c = alternate_plus_minus ? ALT('+') : '+'; 
        	break;
        case KEY_KP_SUBTRACT: 
        	c = alternate_plus_minus ? ALT('-') : '-'; 
        	break;
        case KEY_KP_MULTIPLY: 
        	c = alternate_plus_minus ? ALT('*') : '*'; 
        	break;
	case -1:
		c = 0;
		break;
	default:
		break;
    }

    return c;
}

static int getch_with_delay (void)
{
    int c;

    while (1) {
	/* Try to get a character */
	c = get_key_code (0);
	if (c != ERR)
	    break;
    }
    /* Success -> return the character */
    return c;
}


extern int max_dirt_limit;

/* Returns a character read from stdin with appropriate interpretation */
int get_event (Gpm_Event *event, int redo_event, int block)
{
    int c;
    static int flag;			/* Return value from select */
    static int dirty = 3;

    if ((dirty == 1) || is_idle ()){
	refresh ();
	doupdate ();
	dirty = 1;
    } else
	dirty++;

    vfs_timeout_handler ();
    
    /* Repeat if using mouse */
#ifdef HAVE_SLANG
    while ((xmouse_flag) && !pending_keys)
    {
//	SLms_GetEvent (event);
	*event = evSaved_Event;
    }
#endif


    c = block ? getch_with_delay () : get_key_code (1);
    if (!c) { 				/* Code is 0, so this is a Control key or mouse event */
#ifdef HAVE_SLANG
//	SLms_GetEvent (event);
	*event = evSaved_Event;
#else
	ms_GetEvent (event);	/* my curses */
#endif
        return EV_NONE; /* FIXME: when should we return EV_MOUSE ? */
    }

    return c;
}

/* Returns a key press, mouse events are discarded */
int mi_getch ()
{
    Gpm_Event ev;
    int       key;
    
    while ((key = get_event (&ev, 0, 1)) == 0)
	;
    return key;
}

/* 
  Special handling of ESC key when old_esc_mode:
	ESC+ a-z,\n\t\v\r! = ALT(c)
	ESC + ' '|ESC = ESC
	ESC+0-9 = F(c-'0')
*/
static int EscapeKey (char* seq)
{
    int c = *seq;

    if (old_esc_mode) {
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
	  || (c == '\n') || (c == '\t') || (c == XCTRL('h'))
	  || (c == KEY_BACKSPACE) || (c == '!') || (c == '\r')
	  || c == 127 || c == '+' || c == '-' || c == '\\' 
	  || c == '?')
	    c = ALT(c);
	else if (isdigit(c))
	    c = KEY_F (c-'0');
	else if (c == ' ')
 	    c = ESC_CHAR;
	return c;
    }
    else
    	return 0;	/* i.e. return esc then c */
}

/* Control and Alt
 */
static int ControlKey (char* seq)
{
    return XCTRL(*seq);
}
static int AltKey (char *seq)
{
    return ALT(*seq);
}

/* 
   is_idle -    A function to check if we're idle.
		It checks for any waiting event  (that can be a Key, Mouse event, 
   		and other internal events like focus or menu) 
*/
int is_idle (void)
{
    DWORD dw;
    if (GetNumberOfConsoleInputEvents (hConsoleInput, &dw))
	if (dw > 15)
 	    return 0;
    return 1;
}

/* get_modifier  */
int get_modifier()
{
    int  retval = 0;

    if (dwSaved_ControlState & LEFT_ALT_PRESSED)        /* code is not clean, because we return Linux-like bitcodes*/
	retval |= ALTL_PRESSED;
    if (dwSaved_ControlState & RIGHT_ALT_PRESSED)
	retval |= ALTR_PRESSED;

    if (dwSaved_ControlState & RIGHT_CTRL_PRESSED ||
	dwSaved_ControlState & LEFT_CTRL_PRESSED)
	retval |= CONTROL_PRESSED;

    if (dwSaved_ControlState & SHIFT_PRESSED)
	retval |= SHIFT_PRESSED;

    return retval;
}

int ctrl_pressed ()
{
    return dwSaved_ControlState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED);
}

int shift_pressed ()
{
    return dwSaved_ControlState & SHIFT_PRESSED;
}

int alt_pressed ()
{
    return dwSaved_ControlState & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED);
}

/* void functions for UNIX copatibility */
void try_channels (int set_timeout)
{
}
void channels_up()
{
}
void channels_down()
{
}
void learn_keys()
{
	message (1, "Learn Keys", "Sorry, no learn keys on Win32");
}
void init_key_input_fd (void)
{
}
#endif /* _OS_NT */
