/* Editor key translation.

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.
*/

#include <config.h>
#include "edit.h"
#include "edit-widget.h"	/* edit->macro_i */
#include "editcmddef.h"		/* list of commands */
#include "../src/key.h"		/* KEY_M_SHIFT */
#include "../src/tty.h"		/* keys */
#include "../src/charsets.h"	/* convert_from_input_c() */
#include "../src/selcodepage.h"	/* do_select_codepage() */

/*
 * Ordinary translations.  Note that the keys listed first take priority
 * when the key is assigned to more than one command.
 */
static const long cooledit_key_map[] = {
    ALT ('b'), CK_Match_Bracket,
    ALT ('m'), CK_Mail,
    XCTRL ('f'), CK_Save_Block,
    XCTRL ('n'), CK_New,
    XCTRL ('p'), CK_Pipe_Block (1),	/* spell check */
    XCTRL ('x'), CK_Word_Right,
    XCTRL ('y'), CK_Delete_Line,
    XCTRL ('z'), CK_Word_Left,
    0, 0
};

static long const emacs_key_map[] = {
    ALT ('$'), CK_Pipe_Block (1),	/* spell check */
    ALT ('b'), CK_Word_Left,
    ALT ('f'), CK_Word_Right,
    ALT ('v'), CK_Page_Up,
    ALT ('w'), CK_XStore,
    XCTRL ('@'), CK_Mark,
    XCTRL ('a'), CK_Home,
    XCTRL ('b'), CK_Left,
    XCTRL ('e'), CK_End,
    XCTRL ('f'), CK_Right,
    XCTRL ('n'), CK_Down,
    XCTRL ('p'), CK_Up,
    XCTRL ('s'), CK_Find,
    XCTRL ('v'), CK_Page_Down,
    XCTRL ('w'), CK_XCut,
    XCTRL ('y'), CK_XPaste,
    0, 0
};

static long const common_key_map[] = {
    '\n', CK_Enter,
    '\t', CK_Tab,

    ESC_CHAR, CK_Exit,
    KEY_BACKSPACE, CK_BackSpace,
    KEY_DC, CK_Delete,
    KEY_DOWN, CK_Down,
    KEY_END, CK_End,
    KEY_HOME, CK_Home,
    KEY_IC, CK_Toggle_Insert,
    KEY_LEFT, CK_Left,
    KEY_NPAGE, CK_Page_Down,
    KEY_PPAGE, CK_Page_Up,
    KEY_RIGHT, CK_Right,
    KEY_UP, CK_Up,

    ALT ('\n'), CK_Return,
    ALT ('\t'), CK_Complete_Word,
    ALT ('l'), CK_Goto,
    ALT ('L'), CK_Goto,
    ALT ('p'), CK_Paragraph_Format,
    ALT ('t'), CK_Sort,
    ALT ('u'), CK_ExtCmd,
    ALT ('<'), CK_Beginning_Of_Text,
    ALT ('>'), CK_End_Of_Text,
    ALT (KEY_BACKSPACE), CK_Delete_Word_Left,

    XCTRL ('k'), CK_Delete_To_Line_End,
    XCTRL ('l'), CK_Refresh,
    XCTRL ('o'), CK_Shell,
    XCTRL ('u'), CK_Undo,

    KEY_F (1), CK_Help,
    KEY_F (2), CK_Save,
    KEY_F (3), CK_Mark,
    KEY_F (4), CK_Replace,
    KEY_F (5), CK_Copy,
    KEY_F (6), CK_Move,
    KEY_F (7), CK_Find,
    KEY_F (8), CK_Remove,
    KEY_F (10), CK_Exit,
    KEY_F (11), CK_User_Menu,	/* edit user menu */
    KEY_F (12), CK_Save_As,
    KEY_F (13), CK_Column_Mark,
    KEY_F (14), CK_Replace_Again,
    KEY_F (15), CK_Insert_File,
    KEY_F (17), CK_Find_Again,
    KEY_F (19), CK_Pipe_Block (0),	/* C formatter */

    /* Shift */
    KEY_M_SHIFT | KEY_PPAGE, CK_Page_Up_Highlight,
    KEY_M_SHIFT | KEY_NPAGE, CK_Page_Down_Highlight,
    KEY_M_SHIFT | KEY_LEFT, CK_Left_Highlight,
    KEY_M_SHIFT | KEY_RIGHT, CK_Right_Highlight,
    KEY_M_SHIFT | KEY_UP, CK_Up_Highlight,
    KEY_M_SHIFT | KEY_DOWN, CK_Down_Highlight,
    KEY_M_SHIFT | KEY_HOME, CK_Home_Highlight,
    KEY_M_SHIFT | KEY_END, CK_End_Highlight,
    KEY_M_SHIFT | KEY_IC, CK_XPaste,
    KEY_M_SHIFT | KEY_DC, CK_XCut,
    KEY_M_SHIFT | '\n', CK_Return,	/* useful for pasting multiline text */

    /* Ctrl */
    KEY_M_CTRL | (KEY_F (2)), CK_Save_As,
    KEY_M_CTRL | (KEY_F (4)), CK_Replace_Again,
    KEY_M_CTRL | (KEY_F (7)), CK_Find_Again,
    KEY_M_CTRL | KEY_BACKSPACE, CK_Undo,
    KEY_M_CTRL | KEY_PPAGE, CK_Beginning_Of_Text,
    KEY_M_CTRL | KEY_NPAGE, CK_End_Of_Text,
    KEY_M_CTRL | KEY_HOME, CK_Beginning_Of_Text,
    KEY_M_CTRL | KEY_END, CK_End_Of_Text,
    KEY_M_CTRL | KEY_UP, CK_Scroll_Up,
    KEY_M_CTRL | KEY_DOWN, CK_Scroll_Down,
    KEY_M_CTRL | KEY_LEFT, CK_Word_Left,
    KEY_M_CTRL | KEY_RIGHT, CK_Word_Right,
    KEY_M_CTRL | KEY_IC, CK_XStore,
    KEY_M_CTRL | KEY_DC, CK_Remove,

    /* Ctrl + Shift */
    KEY_M_SHIFT | KEY_M_CTRL | KEY_PPAGE, CK_Beginning_Of_Text_Highlight,
    KEY_M_SHIFT | KEY_M_CTRL | KEY_NPAGE, CK_End_Of_Text_Highlight,
    KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT, CK_Word_Left_Highlight,
    KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, CK_Word_Right_Highlight,
    KEY_M_SHIFT | KEY_M_CTRL | KEY_UP, CK_Scroll_Up_Highlight,
    KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN, CK_Scroll_Down_Highlight,

    0, 0
};


/*
 * Translate the keycode into either 'command' or 'char_for_insertion'.
 * 'command' is one of the editor commands from editcmddef.h.
 */
int
edit_translate_key (WEdit *edit, long x_key, int *cmd, int *ch)
{
    int command = -1;
    int char_for_insertion = -1;
    int i = 0;
    static const long *key_map;

    switch (edit_key_emulation) {
    case EDIT_KEY_EMULATION_NORMAL:
	key_map = cooledit_key_map;
	break;
    case EDIT_KEY_EMULATION_EMACS:
	key_map = emacs_key_map;
	if (x_key == XCTRL ('x')) {
	    int ext_key;
	    ext_key =
		edit_raw_key_query (" Ctrl-X ", _(" Emacs key: "), 0);
	    switch (ext_key) {
	    case 's':
		command = CK_Save;
		goto fin;
	    case 'x':
		command = CK_Exit;
		goto fin;
	    case 'k':
		command = CK_New;
		goto fin;
	    case 'e':
		command =
		    CK_Macro (edit_raw_key_query
			      (_(" Execute Macro "),
			       _(" Press macro hotkey: "), 1));
		if (command == CK_Macro (0))
		    command = -1;
		goto fin;
	    }
	    goto fin;
	}
	break;
    }

#ifdef HAVE_CHARSET
    if (x_key == XCTRL ('t')) {
	do_select_codepage ();

	edit->force = REDRAW_COMPLETELY;
	command = CK_Refresh;
	goto fin;
    }
#endif

    if (x_key == XCTRL ('q')) {
	char_for_insertion =
	    edit_raw_key_query (_(" Insert Literal "),
				_(" Press any key: "), 0);
	goto fin;
    }
    if (x_key == XCTRL ('a')
	&& edit_key_emulation != EDIT_KEY_EMULATION_EMACS) {
	command =
	    CK_Macro (edit_raw_key_query
		      (_(" Execute Macro "), _(" Press macro hotkey: "),
		       1));
	if (command == CK_Macro (0))
	    command = -1;
	goto fin;
    }
    /* edit is a pointer to the widget */
    if (edit)
	if (x_key == XCTRL ('r')) {
	    command =
		edit->macro_i <
		0 ? CK_Begin_Record_Macro : CK_End_Record_Macro;
	    goto fin;
	}


    /* an ordinary insertable character */
    if (x_key < 256) {
	int c = convert_from_input_c (x_key);

	if (is_printable (c)) {
	    char_for_insertion = c;
	    goto fin;
	}
    }

    /* Commands specific to the key emulation */
    i = 0;
    while (key_map[i] != x_key && (key_map[i] || key_map[i + 1]))
	i += 2;
    command = key_map[i + 1];
    if (command)
	goto fin;

    /* Commands common for the key emulations */
    key_map = common_key_map;
    i = 0;
    while (key_map[i] != x_key && (key_map[i] || key_map[i + 1]))
	i += 2;
    command = key_map[i + 1];
    if (command)
	goto fin;

    /* Function still not found for this key, so try macros */
    /* This allows the same macro to be 
       enabled by either eg "ALT('f')" or "XCTRL('f')" or "XCTRL('a'), 'f'" */

    if (x_key & ALT (0)) {	/* is an alt key ? */
	command = CK_Macro (x_key - ALT (0));
	goto fin;
    }
    if (x_key < ' ') {		/* is a ctrl key ? */
	command = CK_Macro (x_key);
	goto fin;
    }
  fin:

    *cmd = command;
    *ch = char_for_insertion;

    if ((command == -1 || command == 0) && char_for_insertion == -1) {
	/* unchanged, key has no function here */
	return 0;
    }

    return 1;
}
