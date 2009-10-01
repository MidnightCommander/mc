/* editor menu definitions and initialisation

   Copyright (C) 1996, 1998, 2001, 2002, 2003, 2005, 2007
   Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

/** \file
 *  \brief Source: editor menu definitions and initialisation
 *  \author Paul Sheer
 *  \date 1996, 1997
 */

#include <config.h>

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "../src/global.h"

#include "../src/tty/tty.h"	/* KEY_F */
#include "../src/tty/key.h"	/* XCTRL */

#include "../src/menu.h"	/* menu_entry */
#include "../src/main.h"	/* drop_menus */
#include "../src/dialog.h"	/* cb_ret_t */
#include "../src/cmddef.h"

#include "edit-impl.h"
#include "edit-widget.h"

cb_ret_t
edit_menu_execute (int command)
{
    edit_execute_key_command (wedit, command, -1);
    edit_update_screen (wedit);
    return MSG_HANDLED;
}

static GList *
create_file_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Open file..."),         CK_Load));
    entries = g_list_append (entries, menu_entry_create (_("&New              C-n"), CK_New));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Save              F2"), CK_Save));
    entries = g_list_append (entries, menu_entry_create (_("Save &as...       F12"), CK_Save_As));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Insert file...   F15"), CK_Insert_File));
    entries = g_list_append (entries, menu_entry_create (_("Copy to &file...  C-f"), CK_Save_Block));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&User menu...     F11"), CK_User_Menu));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("A&bout...            "), CK_About));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Quit             F10"), CK_Exit));

    return entries;
}

static GList *
create_file_menu_emacs (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Open file..."),         CK_Load));
    entries = g_list_append (entries, menu_entry_create (_("&New            C-x k"), CK_New));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Save              F2"), CK_Save));
    entries = g_list_append (entries, menu_entry_create (_("Save &as...       F12"), CK_Save_As));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Insert file...   F15"), CK_Insert_File));
    entries = g_list_append (entries, menu_entry_create (_("Copy to &file...     "), CK_Save_Block));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&User menu...     F11"), CK_User_Menu));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("A&bout...            "), CK_About));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Quit             F10"), CK_Exit));

    return entries;
}

static GList *
create_edit_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Toggle Mark       F3"), CK_Mark));
    entries = g_list_append (entries, menu_entry_create (_("&Mark Columns    S-F3"), CK_Column_Mark));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Toggle &ins/overw Ins"), CK_Toggle_Insert));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Copy              F5"), CK_Copy));
    entries = g_list_append (entries, menu_entry_create (_("&Move              F6"), CK_Move));
    entries = g_list_append (entries, menu_entry_create (_("&Delete            F8"), CK_Remove));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("C&opy to clipfile         C-Ins"), CK_XStore));
    entries = g_list_append (entries, menu_entry_create (_("C&ut to clipfile          S-Del"), CK_XCut));
    entries = g_list_append (entries, menu_entry_create (_("&Paste from clipfile      S-Ins"), CK_XPaste));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Toggle bookmar&k            M-k"), CK_Toggle_Bookmark));
    entries = g_list_append (entries, menu_entry_create (_("&Next bookmark              M-j"), CK_Next_Bookmark));
    entries = g_list_append (entries, menu_entry_create (_("Pre&v bookmark              M-i"), CK_Prev_Bookmark));
    entries = g_list_append (entries, menu_entry_create (_("&Flush bookmark             M-o"), CK_Flush_Bookmarks));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Undo                       C-u"), CK_Undo));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Beginning     C-PgUp"), CK_Beginning_Of_Text));
    entries = g_list_append (entries, menu_entry_create (_("&End           C-PgDn"), CK_End_Of_Text));

    return entries;
}

static GList *
create_edit_menu_emacs (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Toggle mark                 F3"), CK_Mark));
    entries = g_list_append (entries, menu_entry_create (_("Mar&k columns              S-F3"), CK_Column_Mark));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Toggle &ins/overw           Ins"), CK_Toggle_Insert));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Copy                        F5"), CK_Copy));
    entries = g_list_append (entries, menu_entry_create (_("&Move                        F6"), CK_Move));
    entries = g_list_append (entries, menu_entry_create (_("&Delete                      F8"), CK_Remove));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("C&opy to clipfile           M-w"), CK_XStore));
    entries = g_list_append (entries, menu_entry_create (_("C&ut to clipfile            C-w"), CK_XCut));
    entries = g_list_append (entries, menu_entry_create (_("&Paste from clipfile        C-y"), CK_XPaste));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Toggle bookmar&k               "), CK_Toggle_Bookmark));
    entries = g_list_append (entries, menu_entry_create (_("&Next bookmark                 "), CK_Next_Bookmark));
    entries = g_list_append (entries, menu_entry_create (_("Pre&v bookmark                 "), CK_Prev_Bookmark));
    entries = g_list_append (entries, menu_entry_create (_("&Flush bookmark                "), CK_Flush_Bookmarks));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Undo                       C-u"), CK_Undo));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Beginning               C-PgUp"), CK_Beginning_Of_Text));
    entries = g_list_append (entries, menu_entry_create (_("&End                     C-PgDn"), CK_End_Of_Text));

    return entries;
}

static GList *
create_search_replace_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Search...         F7"), CK_Find));
    entries = g_list_append (entries, menu_entry_create (_("Search &again     F17"), CK_Find_Again));
    entries = g_list_append (entries, menu_entry_create (_("&Replace...        F4"), CK_Replace));

    return entries;
}

static GList *
create_command_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Go to line...            M-l"), CK_Goto));
    entries = g_list_append (entries, menu_entry_create (_("Toggle li&ne state        M-n"), CK_Toggle_Line_State));
    entries = g_list_append (entries, menu_entry_create (_("Go to matching &bracket   M-b"), CK_Match_Bracket));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Find declaration      A-Enter"), CK_Find_Definition));
    entries = g_list_append (entries, menu_entry_create (_("Back from declaration     M--"), CK_Load_Prev_File));
    entries = g_list_append (entries, menu_entry_create (_("Forward to declaration    M-+"), CK_Load_Next_File));
#ifdef HAVE_CHARSET
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Encod&ing...             M-e"), CK_SelectCodepage));
#endif
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Insert &literal...       C-q"),  CK_Insert_Literal));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Refresh screen          C-l"),  CK_Refresh));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Start record macro      C-r"),  CK_Begin_Record_Macro));
    entries = g_list_append (entries, menu_entry_create (_("&Finish record macro...  C-r"),  CK_End_Record_Macro));
    entries = g_list_append (entries, menu_entry_create (_("&Execute macro...   C-a, KEY"),  CK_Execute_Macro));
    entries = g_list_append (entries, menu_entry_create (_("Delete macr&o...            "),  CK_Delete_Macro));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Insert &date/time           "),  CK_Date));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Format p&aragraph        M-p"),  CK_Paragraph_Format));
    entries = g_list_append (entries, menu_entry_create (_("'ispell' s&pell check    C-p"),  CK_Pipe_Block (1)));
    entries = g_list_append (entries, menu_entry_create (_("Sor&t...                 M-t"),  CK_Sort));
    entries = g_list_append (entries, menu_entry_create (_("Paste o&utput of...      M-u"),  CK_ExtCmd));
    entries = g_list_append (entries, menu_entry_create (_("E&xternal Formatter      F19"),  CK_Pipe_Block (0)));
    entries = g_list_append (entries, menu_entry_create (_("&Mail...                    "),  CK_Mail));

    return entries;
}

static GList *
create_command_menu_emacs (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Go to line...            M-l"), CK_Goto));
    entries = g_list_append (entries, menu_entry_create (_("Toggle li&ne state        M-n"), CK_Toggle_Line_State));
    entries = g_list_append (entries, menu_entry_create (_("Go to matching &bracket   M-b"), CK_Match_Bracket));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Find declaration      A-Enter"), CK_Find_Definition));
    entries = g_list_append (entries, menu_entry_create (_("Back from declaration     M--"), CK_Load_Prev_File));
    entries = g_list_append (entries, menu_entry_create (_("Forward to declaration    M-+"), CK_Load_Next_File));
#ifdef HAVE_CHARSET
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Encod&ing...             C-t"), CK_SelectCodepage));
#endif
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Insert &literal...       C-q"), CK_Insert_Literal));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Refresh screen          C-l"), CK_Refresh));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Start record macro      C-r"), CK_Begin_Record_Macro));
    entries = g_list_append (entries, menu_entry_create (_("&Finish record macro...  C-r"), CK_End_Record_Macro));
    entries = g_list_append (entries, menu_entry_create (_("&Execute macro... C-x e, KEY"), CK_Execute_Macro));
    entries = g_list_append (entries, menu_entry_create (_("Delete macr&o...            "), CK_Delete_Macro));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Insert &date/time           "), CK_Date));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Format p&aragraph        M-p"), CK_Paragraph_Format));
    entries = g_list_append (entries, menu_entry_create (_("'ispell' s&pell check    M-$"), CK_Pipe_Block (1)));
    entries = g_list_append (entries, menu_entry_create (_("Sor&t...                 M-t"), CK_Sort));
    entries = g_list_append (entries, menu_entry_create (_("Paste o&utput of...      M-u"), CK_ExtCmd));
    entries = g_list_append (entries, menu_entry_create (_("E&xternal Formatter      F19"), CK_Pipe_Block (0)));
    entries = g_list_append (entries, menu_entry_create (_("&Mail...                    "), CK_Mail));

    return entries;
}

static GList *
create_options_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&General...  "),           CK_Edit_Options));
    entries = g_list_append (entries, menu_entry_create (_("&Save mode..."),           CK_Edit_Save_Mode));
    entries = g_list_append (entries, menu_entry_create (_("Learn &Keys..."),          CK_LearnKeys));
    entries = g_list_append (entries, menu_entry_create (_("Syntax &Highlighting..."), CK_Choose_Syntax));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("S&yntax file"),            CK_Load_Syntax_File));
    entries = g_list_append (entries, menu_entry_create (_("&Menu file"),              CK_Load_Menu_File));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Save setu&p..."),          CK_SaveSetupCmd));

    return entries;
}

static void
edit_init_menu_normal (struct WMenuBar *menubar)
{
    menubar_add_menu (menubar, create_menu (_(" &File "),      create_file_menu (),           "[Internal File Editor]"));
    menubar_add_menu (menubar, create_menu (_(" &Edit "),      create_edit_menu (),           "[Internal File Editor]"));
    menubar_add_menu (menubar, create_menu (_(" &Sear/Repl "), create_search_replace_menu (), "[Internal File Editor]"));
    menubar_add_menu (menubar, create_menu (_(" &Command "),   create_command_menu (),        "[Internal File Editor]"));
    menubar_add_menu (menubar, create_menu (_(" &Options "),   create_options_menu (),        "[Internal File Editor]"));
}

static void
edit_init_menu_emacs (struct WMenuBar *menubar)
{
    menubar_add_menu (menubar, create_menu (_(" &File "),      create_file_menu_emacs (),     "[Internal File Editor]"));
    menubar_add_menu (menubar, create_menu (_(" &Edit "),      create_edit_menu_emacs (),     "[Internal File Editor]"));
    menubar_add_menu (menubar, create_menu (_(" &Sear/Repl "), create_search_replace_menu (), "[Internal File Editor]"));
    menubar_add_menu (menubar, create_menu (_(" &Command "),   create_command_menu_emacs (),  "[Internal File Editor]"));
    menubar_add_menu (menubar, create_menu (_(" &Options "),   create_options_menu (),        "[Internal File Editor]"));
}

static void
edit_init_menu (struct WMenuBar *menubar)
{
    switch (edit_key_emulation) {
    default:
    case EDIT_KEY_EMULATION_NORMAL:
	edit_init_menu_normal (menubar);
	break;
    case EDIT_KEY_EMULATION_EMACS:
	edit_init_menu_emacs (menubar);
	break;
    }
}

struct WMenuBar *
edit_create_menu (void)
{
    struct WMenuBar *menubar;

    menubar = menubar_new (0, 0, COLS, NULL);
    edit_init_menu (menubar);
    return menubar;
}

void
edit_reload_menu (void)
{
    menubar_set_menu (edit_menubar, NULL);
    edit_init_menu (edit_menubar);
}


static void
edit_drop_menu_cmd (WEdit *e, int which)
{
    if (edit_menubar->is_active)
	return;
    edit_menubar->is_active = TRUE;
    edit_menubar->is_dropped = (drop_menus != 0);
    if (which >= 0)
	edit_menubar->selected = which;

    edit_menubar->previous_widget = e->widget.parent->current->dlg_id;
    dlg_select_widget (edit_menubar);
}

void edit_menu_cmd (WEdit * e)
{
    edit_drop_menu_cmd (e, -1);
}

int edit_drop_hotkey_menu (WEdit * e, int key)
{
    int m = 0;
    switch (key) {
    case ALT ('f'):
	if (edit_key_emulation == EDIT_KEY_EMULATION_EMACS)
	    return 0;
	m = 0;
	break;
    case ALT ('e'):
	m = 1;
	break;
    case ALT ('s'):
	m = 2;
	break;
    case ALT ('c'):
	m = 3;
	break;
    case ALT ('o'):
	m = 4;
	break;
    default:
	return 0;
    }

    edit_drop_menu_cmd (e, m);
    return 1;
}
