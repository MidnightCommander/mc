/* edit_key_translator.c - does key to command translation
   Copyright (C) 1996, 1997 Paul Sheer

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
 */

/*
   This sequence of code takes the integer 'x_state' and the long
   integer 'x_key' and translates them into the integer 'command' or the
   integer 'char_for_insertion'. 'x_key' holds one of the keys in the
   system header file key_sym_def.h (/usr/include/X11/key_sym_def.h on
   my Linux machine) and 'x_state' holds a bitwise inclusive OR of
   Button1Mask, Button2Mask, ShiftMask, LockMask, ControlMask, MyAltMask,
   Mod2Mask, Mod3Mask, Mod4Mask, or Mod5Mask as explained in the
   XKeyEvent man page. The integer 'command' is one of the editor
   commands in the header file editcmddef.h

   Normally you would only be interested in the ShiftMask and
   ControlMask modifiers. The MyAltMask modifier refers to the Alt key
   on my system.

   If the particular 'x_key' is an ordinary character (say XK_a) then
   you must translate it into 'char_for_insertion', and leave 'command'
   untouched.

   So for example, to add the key binding Ctrl-@ for marking text,
   the following piece of code can be used:

   if ((x_state & ControlMask) && x_key == XK_2) {
       command = CK_Mark;
       goto fin:
   }

   For another example, suppose you want the exclamation mark key to
   insert a '1' then,

   if (x_key == XK_exclam) {
       char_for_insertion = '1';
       goto fin:
   }

   However you must not set both 'command' and 'char_for_insertion';
   one or the other only.

   Not every combination of key states and keys will work though,
   and some experimentation may be necessary.

   The code below is an example that may by appended or modified
   by the user. For brevity, it has a lookup table for basic key presses.
 */

#include <config.h>
#include "edit.h"
#include "app_glob.c"
#include "coollocal.h"
#include "editcmddef.h"

int (*user_defined_key_function) (unsigned int state, unsigned int keycode, KeySym keysym) = 0;

int option_interpret_numlock = 0;

void edit_set_user_key_function (int (*user_def_key_func) (unsigned int, unsigned int, KeySym))
{
    user_defined_key_function = user_def_key_func;
}

int edit_translate_key (unsigned int x_keycode, long x_key, int x_state, int *cmd, int *ch)
{
    int command = -1;
    int char_for_insertion = -1;

    static long key_map[128] =
    {XK_BackSpace, CK_BackSpace, XK_Delete, CK_Delete, XK_Return, CK_Enter, XK_Page_Up, CK_Page_Up,
     XK_Page_Down, CK_Page_Down, XK_Left, CK_Left, XK_Right, CK_Right, XK_Up, CK_Up, XK_Down, CK_Down,
     XK_Home, CK_Home, XK_End, CK_End, XK_Tab, CK_Tab, XK_Undo, CK_Undo, XK_Insert, CK_Toggle_Insert,
     XK_F3, CK_Mark, XK_F5, CK_Copy, XK_F6, CK_Move, XK_F8, CK_Remove, XK_F2, CK_Save, XK_F12, CK_Save_As,
     XK_F10, CK_Exit, XK_Escape, CK_Cancel, XK_F9, CK_Menu,
     XK_F4, CK_Replace, XK_F4, CK_Replace_Again, XK_F17, CK_Find_Again, XK_F7, CK_Find, XK_F15, CK_Insert_File,
     XK_F12, CK_Save_As, XK_F15, CK_Insert_File, XK_F14, CK_Replace_Again, XK_F13, CK_Run_Another, 0, 0};

    static long key_pad_map[10] =
    {XK_Insert, XK_End, XK_Down, XK_Page_Down, XK_Left,
     XK_Down, XK_Right, XK_Home, XK_Up, XK_Page_Up};


#define DEFAULT_NUM_LOCK        1

    static int num_lock = DEFAULT_NUM_LOCK;
    static int raw = 0;
    static int compose = 0;
    static int decimal = 0;
    static int hex = 0;
    int i = 0;
    int h;

    if (compose) {
	if (mod_type_key (x_key)) {
	    goto fin;
	} else {
	    int c;
	    compose = 0;
	    c = get_international_character (x_key);
	    if (c == 1) {
		get_international_character (0);
		goto fin;
	    } else if (c) {
		char_for_insertion = c;
		goto fin;
	    }
	    goto fin;
	}
    }
    if (option_international_characters) {
	if (x_key >= ' ' && x_key <= '~') {
	    extern int compose_key_pressed;
	    if (compose_key_pressed) {
		int c;
		c = my_lower_case (x_key);
		c = get_international_character ((x_state & ShiftMask) ?
			     ((c >= 'a' && c <= 'z') ? c + 'A' - 'a' : c)
						 : c);
		if (c == 1) {
		    compose = 1;
		    goto fin;
		}
		compose = 0;
		if (c)
		    char_for_insertion = c;
		else
		    goto fin;
	    }
	}
    }
    if (x_key <= 0 || mod_type_key (x_key))
	goto fin;

    if (raw) {
	if (!x_state) {
	    if (strchr ("0123456789abcdefh", x_key)) {
		char u[2] =
		{0, 0};
		if (raw == 3) {
		    if (x_key == 'h') {
			char_for_insertion = hex;
			raw = 0;
			goto fin;
		    } else {
			if (x_key > '9') {
			    raw = 0;
			    goto fin;
			}
		    }
		}
		decimal += (x_key - '0') * ((int) ("d\n\001")[raw - 1]);
		u[0] = x_key;
		hex += (strcspn ("0123456789abcdef", u) << (4 * (2 - raw)));
		if (raw == 3) {
		    char_for_insertion = decimal;
		    raw = 0;
		    goto fin;
		}
		raw++;
		goto fin;
	    }
	}
	if (raw > 1) {
	    raw = 0;
	    goto fin;
	}
	raw = 0;
	if (x_key == XK_Return)
	    char_for_insertion = '\n';
	else
	    char_for_insertion = x_key;

	if (x_state & ControlMask)
	    char_for_insertion &= 31;
	if (x_state & (MyAltMask))
	    char_for_insertion |= 128;
	goto fin;
    }

    if (user_defined_key_function)
	if ((h = (*(user_defined_key_function)) (x_state, x_keycode, x_key))) {
	    command = h;
	    goto fin;
	}

    if ((x_state & MyAltMask)) {
	switch ((int) x_key) {
	case XK_Left:
	case XK_KP_Left:
	    command = CK_Delete_Word_Left;
	    goto fin;
	case XK_Right:
	case XK_KP_Right:
	    command = CK_Delete_Word_Right;
	    goto fin;
	case XK_l:
	case XK_L:
	    command = CK_Goto;
	    goto fin;
	case XK_Insert:
	case XK_KP_Insert:
	    command = CK_Selection_History;
	    goto fin;
	case XK_Up:
	case XK_KP_Up:
	    command = CK_Scroll_Up;
	    goto fin;
	case XK_Down:
	case XK_KP_Down:
	    command = CK_Scroll_Down;
	    goto fin;
	case XK_Delete:
	case XK_KP_Delete:
	    command = CK_Delete_To_Line_End;
	    goto fin;
	case XK_BackSpace:
	    command = CK_Delete_To_Line_Begin;
	    goto fin;
	case XK_m:
	case XK_M:
	    command = CK_Mail;
	    goto fin;
	case XK_x:
	case XK_X:
	    command = CK_Save_And_Quit;
	    goto fin;
	case XK_p:
	case XK_P:
	    command = CK_Paragraph_Format;
	    goto fin;
	case XK_F6:
	    command = CK_Maximise;
	    goto fin;
	}
    }
    if ((x_state & MyAltMask) && (x_state & ShiftMask)) {
	switch ((int) x_key) {
	case XK_Up:
	case XK_KP_Up:
	    command = CK_Scroll_Up_Highlight;
	    goto fin;
	case XK_Down:
	case XK_KP_Down:
	    command = CK_Scroll_Down_Highlight;
	    goto fin;
	}
    }
    if (!(x_state & MyAltMask)) {

	if ((x_key == XK_a || x_key == XK_A) && (x_state & ControlMask)) {
	    command = CK_Macro (CKeySymMod (CRawkeyQuery (0, 0, 0, " Execute Macro ", " Press macro hotkey: ")));
	    if (command == CK_Macro (0))
		command = -1;
	    goto fin;
	}
	if (x_key == XK_Num_Lock && option_interpret_numlock) {
	    num_lock = 1 - num_lock;
	    goto fin;
	}
	switch ((int) x_key) {
	case XK_KP_Home:
	    x_key = XK_Home;
	    break;
	case XK_KP_End:
	    x_key = XK_End;
	    break;
	case XK_KP_Page_Up:
	    x_key = XK_Page_Up;
	    break;
	case XK_KP_Page_Down:
	    x_key = XK_Page_Down;
	    break;
	case XK_KP_Up:
	    x_key = XK_Up;
	    break;
	case XK_KP_Down:
	    x_key = XK_Down;
	    break;
	case XK_KP_Left:
	    x_key = XK_Left;
	    break;
	case XK_KP_Right:
	    x_key = XK_Right;
	    break;
	case XK_KP_Insert:
	    x_key = XK_Insert;
	    break;
	case XK_KP_Delete:
	    x_key = XK_Delete;
	    break;
	case XK_KP_Enter:
	    x_key = XK_Return;
	    break;
	case XK_KP_Add:
	    x_key = XK_plus;
	    break;
	case XK_KP_Subtract:
	    x_key = XK_minus;
	    break;
	}

/* first translate the key-pad */
	if (num_lock) {
	    if (x_key >= XK_R1 && x_key <= XK_R9) {
		x_key = key_pad_map[x_key - XK_R1 + 1];
	    } else if (x_key >= XK_KP_0 && x_key <= XK_KP_9) {
		x_key = key_pad_map[x_key - XK_KP_0];
	    } else if (x_key == XK_KP_Decimal) {
		x_key = XK_Delete;
	    }
	} else {
	    if (x_key >= XK_KP_0 && x_key <= XK_KP_9) {
		x_key += XK_0 - XK_KP_0;
	    }
	    if (x_key == XK_KP_Decimal) {
		x_key = XK_period;
	    }
	}

	if ((x_state & ShiftMask) && (x_state & ControlMask)) {
	    switch ((int) x_key) {
	    case XK_Page_Up:
		command = CK_Beginning_Of_Text_Highlight;
		goto fin;
	    case XK_Page_Down:
		command = CK_End_Of_Text_Highlight;
		goto fin;
	    case XK_Left:
		command = CK_Word_Left_Highlight;
		goto fin;
	    case XK_Right:
		command = CK_Word_Right_Highlight;
		goto fin;
	    case XK_Up:
		command = CK_Paragraph_Up_Highlight;
		goto fin;
	    case XK_Down:
		command = CK_Paragraph_Down_Highlight;
		goto fin;
	    case XK_Home:
		command = CK_Begin_Page_Highlight;
		goto fin;
	    case XK_End:
		command = CK_End_Page_Highlight;
		goto fin;
	    }
	}
	if ((x_state & ShiftMask) && !(x_state & ControlMask)) {
	    switch ((int) x_key) {
	    case XK_Page_Up:
		command = CK_Page_Up_Highlight;
		goto fin;
	    case XK_Page_Down:
		command = CK_Page_Down_Highlight;
		goto fin;
	    case XK_Left:
		command = CK_Left_Highlight;
		goto fin;
	    case XK_Right:
		command = CK_Right_Highlight;
		goto fin;
	    case XK_Up:
		command = CK_Up_Highlight;
		goto fin;
	    case XK_Down:
		command = CK_Down_Highlight;
		goto fin;
	    case XK_Home:
		command = CK_Home_Highlight;
		goto fin;
	    case XK_End:
		command = CK_End_Highlight;
		goto fin;
	    case XK_Insert:
		command = CK_XPaste;
		goto fin;
	    case XK_Delete:
		command = CK_XCut;
		goto fin;
	    case XK_Return:
		command = CK_Return;
		goto fin;
/* this parallel F12, F19, F15, and F17 for some systems */
	    case XK_F2:
		command = CK_Save_As;
		goto fin;
	    case XK_F5:
		command = CK_Insert_File;
		goto fin;
	    case XK_F7:
		command = CK_Find_Again;
		goto fin;
	    case XK_F4:
		command = CK_Replace_Again;
		goto fin;
	    case XK_F3:
		command = CK_Run_Another;
		goto fin;
	    }
	}
/* things that need a control key */
	if (x_state & ControlMask) {
	    switch ((int) x_key) {
	    case XK_F1:
		command = CK_Man_Page;
		goto fin;
	    case XK_U:
	    case XK_u:
	    case XK_BackSpace:
		command = CK_Undo;
		goto fin;
	    case XK_Page_Up:
		command = CK_Beginning_Of_Text;
		goto fin;
	    case XK_Page_Down:
		command = CK_End_Of_Text;
		goto fin;
	    case XK_Up:
		command = CK_Paragraph_Up;
		goto fin;
	    case XK_Down:
		command = CK_Paragraph_Down;
		goto fin;
	    case XK_Left:
		command = CK_Word_Left;
		goto fin;
	    case XK_Right:
		command = CK_Word_Right;
		goto fin;
	    case XK_Home:
		command = CK_Begin_Page;
		goto fin;
	    case XK_End:
		command = CK_End_Page;
		goto fin;
	    case XK_N:
	    case XK_n:
		command = CK_New;
		goto fin;
	    case XK_O:
	    case XK_o:
		command = CK_Load;
		goto fin;
	    case XK_D:
	    case XK_d:
		command = CK_Date;
		goto fin;
	    case XK_S:
	    case XK_s:
		command = CK_Save;
		goto fin;
	    case XK_Q:
	    case XK_q:
		raw = 1;
		decimal = 0;
		hex = 0;
		goto fin;
	    case XK_F:
	    case XK_f:
		command = CK_Save_Block;
		goto fin;
	    case XK_F5:
	    case XK_F15:
		command = CK_Insert_File;
		goto fin;
	    case XK_Insert:
		command = CK_XStore;
		goto fin;
	    case XK_y:
	    case XK_Y:
		command = CK_Delete_Line;
		goto fin;
	    case XK_Delete:
		command = CK_Remove;
		goto fin;
	    case XK_F2:
		command = CK_Save_Desktop;
		goto fin;
	    case XK_F3:
		command = CK_New_Window;
		goto fin;
	    case XK_F6:
		command = CK_Cycle;
		goto fin;
	    case XK_F10:
		command = CK_Check_Save_And_Quit;
		goto fin;
	    case XK_Tab:
	    case XK_KP_Tab:
		command = CK_Complete;
		goto fin;
	    case XK_b:
	    case XK_B:
		command = CK_Column_Mark;
		goto fin;
	    case XK_l:
	    case XK_L:
		command = CK_Refresh;
		goto fin;
	    }
	}
/* an ordinary ascii character or international character */
	if (!(x_state & MyAltMask)) {
	    if (!(x_state & ControlMask)) {
		if ((x_key >= XK_space && x_key <= XK_asciitilde) || ((x_key >= 160 && x_key < 256) && option_international_characters)) {
		    char_for_insertion = x_key;
		    goto fin;
		}
/* other commands */
		if (!(x_state & ShiftMask)) {
		    i = 0;
		    while (key_map[i] != x_key && key_map[i])
			i += 2;
		    command = key_map[i + 1];
		    if (command)
			goto fin;
		}
	    }
	}
    }
  fin:

    *cmd = command;
    *ch = char_for_insertion;

    if ((command == -1 || command == 0) && char_for_insertion == -1)	/* unchanged, key has no function here */
	return 0;
    return 1;
}

