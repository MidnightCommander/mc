/* gtkeditkey.c - key defs for gtk

   Copyright (C) 1996, 1997 the Free Software Foundation

   Authors: 1996, 1997 Paul Sheer

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <config.h>
#include "edit.h"
#include "editcmddef.h"
#include <gdk/gdkkeysyms.h>

int mod_type_key (guint x);

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
    {GDK_BackSpace, CK_BackSpace, GDK_Delete, CK_Delete, GDK_Return, CK_Enter, GDK_Page_Up, CK_Page_Up,
     GDK_Page_Down, CK_Page_Down, GDK_Left, CK_Left, GDK_Right, CK_Right, GDK_Up, CK_Up, GDK_Down, CK_Down,
     GDK_Home, CK_Home, GDK_End, CK_End, GDK_Tab, CK_Tab, GDK_Undo, CK_Undo, GDK_Insert, CK_Toggle_Insert,
     GDK_F3, CK_Mark, GDK_F5, CK_Copy, GDK_F6, CK_Move, GDK_F8, CK_Remove, GDK_F2, CK_Save, GDK_F12, CK_Save_As,
     GDK_F10, CK_Exit, GDK_Escape, CK_Cancel, GDK_F9, CK_Menu,
     GDK_F4, CK_Replace, GDK_F4, CK_Replace_Again, GDK_F17, CK_Find_Again, GDK_F7, CK_Find, GDK_F15, CK_Insert_File, 0, 0};

    static long key_pad_map[10] =
    {GDK_Insert, GDK_End, GDK_Down, GDK_Page_Down, GDK_Left,
     GDK_Down, GDK_Right, GDK_Home, GDK_Up, GDK_Page_Up};


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
/* *** */
#if 0
		get_international_character (0);
#endif
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
/* *** */
#if 0
	    extern int compose_key_pressed;
#endif
	    if (compose_key_pressed) {
		int c;
		c = (x_key >= 'A' && x_key <= 'Z') ? x_key - 'A' + 'a' : x_key;
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
	if (x_key == GDK_Return)
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
	case GDK_Left:
	case GDK_KP_Left:
	    command = CK_Delete_Word_Left;
	    goto fin;
	case GDK_Right:
	case GDK_KP_Right:
	    command = CK_Delete_Word_Right;
	    goto fin;
	case GDK_l:
	case GDK_L:
	    command = CK_Goto;
	    goto fin;
	case GDK_Insert:
	case GDK_KP_Insert:
	    command = CK_Selection_History;
	    goto fin;
	case GDK_Up:
	case GDK_KP_Up:
	    command = CK_Scroll_Up;
	    goto fin;
	case GDK_Down:
	case GDK_KP_Down:
	    command = CK_Scroll_Down;
	    goto fin;
	case GDK_Delete:
	case GDK_KP_Delete:
	    command = CK_Delete_To_Line_End;
	    goto fin;
	case GDK_BackSpace:
	    command = CK_Delete_To_Line_Begin;
	    goto fin;
	case GDK_m:
	case GDK_M:
	    command = CK_Mail;
	    goto fin;
	case GDK_x:
	case GDK_X:
	    command = CK_Save_And_Quit;
	    goto fin;
	case GDK_p:
	case GDK_P:
	    command = CK_Paragraph_Format;
	    goto fin;
	}
    }
    if ((x_state & MyAltMask) && (x_state & ShiftMask)) {
	switch ((int) x_key) {
	case GDK_Up:
	case GDK_KP_Up:
	    command = CK_Scroll_Up_Highlight;
	    goto fin;
	case GDK_Down:
	case GDK_KP_Down:
	    command = CK_Scroll_Down_Highlight;
	    goto fin;
	}
    }
    if (!(x_state & MyAltMask)) {

	if ((x_key == GDK_a || x_key == GDK_A) && (x_state & ControlMask)) {
#if 0
	    command = CK_Macro (CKeySymMod (CRawkeyQuery (0, 0, 0, " Execute Macro ", " Press macro hotkey: ")));
	    if (command == CK_Macro (0))
#endif
		command = -1;
	    goto fin;
	}
	if (x_key == GDK_Num_Lock && option_interpret_numlock) {
	    num_lock = 1 - num_lock;
	    goto fin;
	}
	switch ((int) x_key) {
	case GDK_KP_Home:
	    x_key = GDK_Home;
	    break;
	case GDK_KP_End:
	    x_key = GDK_End;
	    break;
	case GDK_KP_Page_Up:
	    x_key = GDK_Page_Up;
	    break;
	case GDK_KP_Page_Down:
	    x_key = GDK_Page_Down;
	    break;
	case GDK_KP_Up:
	    x_key = GDK_Up;
	    break;
	case GDK_KP_Down:
	    x_key = GDK_Down;
	    break;
	case GDK_KP_Left:
	    x_key = GDK_Left;
	    break;
	case GDK_KP_Right:
	    x_key = GDK_Right;
	    break;
	case GDK_KP_Insert:
	    x_key = GDK_Insert;
	    break;
	case GDK_KP_Delete:
	    x_key = GDK_Delete;
	    break;
	case GDK_KP_Enter:
	    x_key = GDK_Return;
	    break;
	case GDK_KP_Add:
	    x_key = GDK_plus;
	    break;
	case GDK_KP_Subtract:
	    x_key = GDK_minus;
	    break;
	}

/* first translate the key-pad */
	if (num_lock) {
	    if (x_key >= GDK_R1 && x_key <= GDK_R9) {
		x_key = key_pad_map[x_key - GDK_R1 + 1];
	    } else if (x_key >= GDK_KP_0 && x_key <= GDK_KP_9) {
		x_key = key_pad_map[x_key - GDK_KP_0];
	    } else if (x_key == GDK_KP_Decimal) {
		x_key = GDK_Delete;
	    }
	} else {
	    if (x_key >= GDK_KP_0 && x_key <= GDK_KP_9) {
		x_key += GDK_0 - GDK_KP_0;
	    }
	    if (x_key == GDK_KP_Decimal) {
		x_key = GDK_period;
	    }
	}

	if ((x_state & ShiftMask) && (x_state & ControlMask)) {
	    switch ((int) x_key) {
	    case GDK_Page_Up:
		command = CK_Beginning_Of_Text_Highlight;
		goto fin;
	    case GDK_Page_Down:
		command = CK_End_Of_Text_Highlight;
		goto fin;
	    case GDK_Left:
		command = CK_Word_Left_Highlight;
		goto fin;
	    case GDK_Right:
		command = CK_Word_Right_Highlight;
		goto fin;
	    case GDK_Up:
		command = CK_Paragraph_Up_Highlight;
		goto fin;
	    case GDK_Down:
		command = CK_Paragraph_Down_Highlight;
		goto fin;
	    case GDK_Home:
		command = CK_Begin_Page_Highlight;
		goto fin;
	    case GDK_End:
		command = CK_End_Page_Highlight;
		goto fin;
	    }
	}
	if ((x_state & ShiftMask) && !(x_state & ControlMask)) {
	    switch ((int) x_key) {
	    case GDK_Page_Up:
		command = CK_Page_Up_Highlight;
		goto fin;
	    case GDK_Page_Down:
		command = CK_Page_Down_Highlight;
		goto fin;
	    case GDK_Left:
		command = CK_Left_Highlight;
		goto fin;
	    case GDK_Right:
		command = CK_Right_Highlight;
		goto fin;
	    case GDK_Up:
		command = CK_Up_Highlight;
		goto fin;
	    case GDK_Down:
		command = CK_Down_Highlight;
		goto fin;
	    case GDK_Home:
		command = CK_Home_Highlight;
		goto fin;
	    case GDK_End:
		command = CK_End_Highlight;
		goto fin;
	    case GDK_Insert:
		command = CK_XPaste;
		goto fin;
	    case GDK_Delete:
		command = CK_XCut;
		goto fin;
	    case GDK_Return:
		command = CK_Return;
		goto fin;
/* this parallel F12, F19, F15, and F17 for some systems */
	    case GDK_F2:
		command = CK_Save_As;
		goto fin;
	    case GDK_F5:
		command = CK_Insert_File;
		goto fin;
	    case GDK_F7:
		command = CK_Find_Again;
		goto fin;
	    case GDK_F4:
		command = CK_Replace_Again;
		goto fin;
	    case GDK_F3:
		command = CK_Run_Another;
		goto fin;
	    }
	}
/* things that need a control key */
	if (x_state & ControlMask) {
	    switch ((int) x_key) {
	    case GDK_F1:
		command = CK_Man_Page;
		goto fin;
	    case GDK_U:
	    case GDK_u:
	    case GDK_BackSpace:
		command = CK_Undo;
		goto fin;
	    case GDK_Page_Up:
		command = CK_Beginning_Of_Text;
		goto fin;
	    case GDK_Page_Down:
		command = CK_End_Of_Text;
		goto fin;
	    case GDK_Up:
		command = CK_Paragraph_Up;
		goto fin;
	    case GDK_Down:
		command = CK_Paragraph_Down;
		goto fin;
	    case GDK_Left:
		command = CK_Word_Left;
		goto fin;
	    case GDK_Right:
		command = CK_Word_Right;
		goto fin;
	    case GDK_Home:
		command = CK_Begin_Page;
		goto fin;
	    case GDK_End:
		command = CK_End_Page;
		goto fin;
	    case GDK_N:
	    case GDK_n:
		command = CK_New;
		goto fin;
	    case GDK_O:
	    case GDK_o:
		command = CK_Load;
		goto fin;
	    case GDK_D:
	    case GDK_d:
		command = CK_Date;
		goto fin;
	    case GDK_Q:
	    case GDK_q:
		raw = 1;
		decimal = 0;
		hex = 0;
		goto fin;
	    case GDK_F:
	    case GDK_f:
		command = CK_Save_Block;
		goto fin;
	    case GDK_F5:
	    case GDK_F15:
		command = CK_Insert_File;
		goto fin;
	    case GDK_Insert:
		command = CK_XStore;
		goto fin;
	    case GDK_y:
	    case GDK_Y:
		command = CK_Delete_Line;
		goto fin;
	    case GDK_Delete:
		command = CK_Remove;
		goto fin;
	    case GDK_F2:
		command = CK_Save_Desktop;
		goto fin;
	    case GDK_F3:
		command = CK_New_Window;
		goto fin;
	    case GDK_F6:
		command = CK_Cycle;
		goto fin;
	    case GDK_F10:
		command = CK_Check_Save_And_Quit;
		goto fin;
	    case GDK_Tab:
	    case GDK_KP_Tab:
		command = CK_Complete;
		goto fin;
	    case GDK_b:
		command = CK_Column_Mark;
		goto fin;
	    }
	}
/* an ordinary ascii character or international character */
	if (!(x_state & MyAltMask)) {
	    if (!(x_state & ControlMask)) {
		if ((x_key >= GDK_space && x_key <= GDK_asciitilde) || ((x_key >= 160 && x_key < 256) && option_international_characters)) {
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

