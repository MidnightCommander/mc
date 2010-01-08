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

#include "src/global.h"

#include "lib/tty/tty.h"	/* KEY_F */
#include "lib/tty/key.h"	/* XCTRL */

#include "src/menu.h"	/* menu_entry */
#include "src/main.h"	/* drop_menus */
#include "src/dialog.h"	/* cb_ret_t */
#include "src/cmddef.h"

#include "edit-impl.h"
#include "edit-widget.h"

static GList *
create_file_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Open file..."),    CK_Load));
    entries = g_list_append (entries, menu_entry_create (_("&New"),             CK_New));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Save"),            CK_Save));
    entries = g_list_append (entries, menu_entry_create (_("Save &as..."),      CK_Save_As));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Insert file..."),  CK_Insert_File));
    entries = g_list_append (entries, menu_entry_create (_("Cop&y to file..."), CK_Save_Block));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&User menu..."),    CK_User_Menu));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("A&bout..."),        CK_About));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Quit"),            CK_Quit));

    return entries;
}

static GList *
create_edit_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Undo"),                CK_Undo));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Toggle ins/overw"),    CK_Toggle_Insert));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("To&ggle mark"),         CK_Mark));
    entries = g_list_append (entries, menu_entry_create (_("&Mark columns"),        CK_Column_Mark));
    entries = g_list_append (entries, menu_entry_create (_("Mark &all"),            CK_Mark_All));
    entries = g_list_append (entries, menu_entry_create (_("Unmar&k"),              CK_Unmark));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Cop&y"),                CK_Copy));
    entries = g_list_append (entries, menu_entry_create (_("Mo&ve"),                CK_Move));
    entries = g_list_append (entries, menu_entry_create (_("&Delete"),              CK_Remove));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Co&py to clipfile"),    CK_XStore));
    entries = g_list_append (entries, menu_entry_create (_("&Cut to clipfile"),     CK_XCut));
    entries = g_list_append (entries, menu_entry_create (_("Pa&ste from clipfile"), CK_XPaste));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Beginning"),           CK_Beginning_Of_Text));
    entries = g_list_append (entries, menu_entry_create (_("&End"),                 CK_End_Of_Text));

    return entries;
}

static GList *
create_search_replace_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Search..."),           CK_Find));
    entries = g_list_append (entries, menu_entry_create (_("Search &again"),        CK_Find_Again));
    entries = g_list_append (entries, menu_entry_create (_("&Replace..."),          CK_Replace));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Toggle bookmark"),     CK_Toggle_Bookmark));
    entries = g_list_append (entries, menu_entry_create (_("&Next bookmark"),       CK_Next_Bookmark));
    entries = g_list_append (entries, menu_entry_create (_("&Prev bookmark"),       CK_Prev_Bookmark));
    entries = g_list_append (entries, menu_entry_create (_("&Flush bookmark"),      CK_Flush_Bookmarks));

    return entries;
}

static GList *
create_command_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Go to line..."),          CK_Goto));
    entries = g_list_append (entries, menu_entry_create (_("&Toggle line state"),      CK_Toggle_Line_State));
    entries = g_list_append (entries, menu_entry_create (_("Go to matching &bracket"), CK_Match_Bracket));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Find declaration"),       CK_Find_Definition));
    entries = g_list_append (entries, menu_entry_create (_("Back from &declaration"),  CK_Load_Prev_File));
    entries = g_list_append (entries, menu_entry_create (_("For&ward to declaration"), CK_Load_Next_File));
#ifdef HAVE_CHARSET
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Encod&ing..."),            CK_SelectCodepage));
#endif
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Refresh screen"),         CK_Refresh));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Start record macro"),     CK_Begin_Record_Macro));
    entries = g_list_append (entries, menu_entry_create (_("Finis&h record macro..."), CK_End_Record_Macro));
    entries = g_list_append (entries, menu_entry_create (_("&Execute macro..."),       CK_Execute_Macro));
    entries = g_list_append (entries, menu_entry_create (_("Delete macr&o..."),        CK_Delete_Macro));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("'ispell' s&pell check"),   CK_Pipe_Block (1)));
    entries = g_list_append (entries, menu_entry_create (_("&Mail..."),                CK_Mail));

    return entries;
}

static GList *
create_format_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("Insert &literal..."),      CK_Insert_Literal));
    entries = g_list_append (entries, menu_entry_create (_("Insert &date/time"),       CK_Date));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Format paragraph"),       CK_Paragraph_Format));
    entries = g_list_append (entries, menu_entry_create (_("&Sort..."),                CK_Sort));
    entries = g_list_append (entries, menu_entry_create (_("&Paste output of..."),     CK_ExtCmd));
    entries = g_list_append (entries, menu_entry_create (_("&External formatter"),     CK_Pipe_Block (0)));

    return entries;
}

static GList *
create_options_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&General...  "),           CK_Edit_Options));
    entries = g_list_append (entries, menu_entry_create (_("Save &mode..."),           CK_Edit_Save_Mode));
    entries = g_list_append (entries, menu_entry_create (_("Learn &keys..."),          CK_LearnKeys));
    entries = g_list_append (entries, menu_entry_create (_("Syntax &highlighting..."), CK_Choose_Syntax));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("S&yntax file"),            CK_Load_Syntax_File));
    entries = g_list_append (entries, menu_entry_create (_("&Menu file"),              CK_Load_Menu_File));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Save setup"),             CK_SaveSetupCmd));

    return entries;
}

void
edit_init_menu (struct WMenuBar *menubar)
{
    menubar_add_menu (menubar, create_menu (_("&File"),    create_file_menu (),           "[Internal File Editor]"));
    menubar_add_menu (menubar, create_menu (_("&Edit"),    create_edit_menu (),           "[Internal File Editor]"));
    menubar_add_menu (menubar, create_menu (_("&Search"),  create_search_replace_menu (), "[Internal File Editor]"));
    menubar_add_menu (menubar, create_menu (_("&Command"), create_command_menu (),        "[Internal File Editor]"));
    menubar_add_menu (menubar, create_menu (_("For&mat"),  create_format_menu (),         "[Internal File Editor]"));
    menubar_add_menu (menubar, create_menu (_("&Options"), create_options_menu (),        "[Internal File Editor]"));
}

static void
edit_drop_menu_cmd (WEdit *e, int which)
{
    if (!edit_menubar->is_active) {
	edit_menubar->is_active = TRUE;
	edit_menubar->is_dropped = (drop_menus != 0);
	if (which >= 0)
	    edit_menubar->selected = which;

	edit_menubar->previous_widget = e->widget.parent->current->dlg_id;
	dlg_select_widget (edit_menubar);
    }
}

void
edit_menu_cmd (WEdit *e)
{
    edit_drop_menu_cmd (e, -1);
}

int
edit_drop_hotkey_menu (WEdit *e, int key)
{
    int m = 0;
    switch (key) {
    case ALT ('f'):
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
    case ALT ('m'):
	m = 4;
	break;
    case ALT ('o'):
	m = 5;
	break;
    default:
	return 0;
    }

    edit_drop_menu_cmd (e, m);
    return 1;
}
