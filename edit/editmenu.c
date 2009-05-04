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

#include "edit-impl.h"
#include "edit-widget.h"

#include "../src/tty/tty.h"	/* KEY_F */
#include "../src/tty/key.h"	/* XCTRL */

#include "../src/cmd.h"		/* save_setup_cmd() */
#include "../src/wtools.h"	/* query_dialog() */
#include "../src/menu.h"	/* menu_entry */
#include "../src/main.h"	/* drop_menus */
#include "../src/learn.h"	/* learn_keys */

#include "edit-widget.h"
#include "../src/cmddef.h"

static void
menu_cmd (int command)
{
    edit_execute_key_command (wedit, command, -1);
    edit_update_screen (wedit);
}

static void menu_key (int i)
{
    send_message ((Widget *) wedit, WIDGET_KEY, i);
}

static void
edit_about_cmd (void)
{
    query_dialog (_(" About "),
		  _("\n                Cooledit  v3.11.5\n\n"
		    " Copyright (C) 1996 the Free Software Foundation\n\n"
		    "       A user friendly text editor written\n"
		    "           for the Midnight Commander.\n"), D_NORMAL,
		  1, _("&OK"));
}

static void
menu_mail_cmd (void)
{
    menu_cmd (CK_Mail);
}

static void
menu_load_cmd (void)
{
    menu_cmd (CK_Load);
}

static void
menu_new_cmd (void)
{
    menu_cmd (CK_New);
}

static void
menu_save_cmd (void)
{
    menu_cmd (CK_Save);
}

static void
menu_save_as_cmd (void)
{
    menu_cmd (CK_Save_As);
}

static void
menu_insert_file_cmd (void)
{
    menu_cmd (CK_Insert_File);
}

static void
menu_quit_cmd (void)
{
    menu_cmd (CK_Exit);
}

static void
menu_mark_cmd (void)
{
    menu_cmd (CK_Mark);
}

static void
menu_markcol_cmd (void)
{
    menu_cmd (CK_Column_Mark);
}

static void
menu_ins_cmd (void)
{
    menu_cmd (CK_Toggle_Insert);
}

static void
menu_copy_cmd (void)
{
    menu_cmd (CK_Copy);
}

static void
menu_move_cmd (void)
{
    menu_cmd (CK_Move);
}

static void
menu_delete_cmd (void)
{
    menu_cmd (CK_Remove);
}

static void
menu_xstore_cmd (void)
{
    menu_cmd (CK_XStore);
}

static void
menu_xcut_cmd (void)
{
    menu_cmd (CK_XCut);
}

static void
menu_xpaste_cmd (void)
{
    menu_cmd (CK_XPaste);
}

static void
menu_toggle_bookmark_cmd (void)
{
    menu_cmd (CK_Toggle_Bookmark);
}

static void
menu_flush_bookmark_cmd (void)
{
    menu_cmd (CK_Flush_Bookmarks);
}

static void
menu_next_bookmark_cmd (void)
{
    menu_cmd (CK_Next_Bookmark);
}

static void
menu_prev_bookmark_cmd (void)
{
    menu_cmd (CK_Prev_Bookmark);
}

static void
menu_cut_cmd (void)
{
    menu_cmd (CK_Save_Block);
}

static void
menu_search_cmd (void)
{
    menu_cmd (CK_Find);
}

static void
menu_search_again_cmd (void)
{
    menu_cmd (CK_Find_Again);
}

static void
menu_replace_cmd (void)
{
    menu_cmd (CK_Replace);
}

static void
menu_select_codepage_cmd (void)
{
    menu_cmd (CK_SelectCodepage);
}

static void
menu_begin_record_cmd (void)
{
    menu_cmd (CK_Begin_Record_Macro);
}

static void
menu_end_record_cmd (void)
{
    menu_cmd (CK_End_Record_Macro);
}

static void
menu_exec_macro_cmd (void)
{
    menu_key (XCTRL ('a'));
}

static void
menu_exec_macro_delete_cmd (void)
{
    menu_cmd (CK_Delete_Macro);
}

static void
menu_c_form_cmd (void)
{
    menu_key (KEY_F (19));
}

static void
menu_ispell_cmd (void)
{
    menu_cmd (CK_Pipe_Block (1));
}

static void
menu_sort_cmd (void)
{
    menu_cmd (CK_Sort);
}

static void
menu_ext_cmd (void)
{
    menu_cmd (CK_ExtCmd);
}

static void
menu_date_cmd (void)
{
    menu_cmd (CK_Date);
}

static void
menu_undo_cmd (void)
{
    menu_cmd (CK_Undo);
}

static void
menu_beginning_cmd (void)
{
    menu_cmd (CK_Beginning_Of_Text);
}

static void
menu_end_cmd (void)
{
    menu_cmd (CK_End_Of_Text);
}

static void
menu_refresh_cmd (void)
{
    menu_cmd (CK_Refresh);
}

static void
menu_goto_line (void)
{
    menu_cmd (CK_Goto);
}

static void
menu_toggle_line_state (void)
{
    menu_cmd (CK_Toggle_Line_State);
}

static void
menu_goto_bracket (void)
{
    menu_cmd (CK_Match_Bracket);
}

static void
menu_lit_cmd (void)
{
    menu_key (XCTRL ('q'));
}

static void
menu_format_paragraph (void)
{
    menu_cmd (CK_Paragraph_Format);
}

static void
menu_edit_syntax_file_cmd (void)
{
    menu_cmd (CK_Load_Syntax_File);
}

static void
menu_edit_menu_file_cmd (void)
{
    menu_cmd (CK_Load_Menu_File);
}

static void
menu_user_menu_cmd (void)
{
    menu_key (KEY_F (11));
}

static void
menu_find_declare (void)
{
    menu_cmd (CK_Find_Definition);
}

static void
menu_declare_back (void)
{
    menu_cmd (CK_Load_Prev_File);
}

static void
menu_declare_forward (void)
{
    menu_cmd (CK_Load_Next_File);
}

static GPtrArray *
create_file_menu (void)
{
    GPtrArray *entries = g_ptr_array_new ();

    g_ptr_array_add (entries, menu_entry_create (_("&Open file..."),         menu_load_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&New              C-n"), menu_new_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Save              F2"), menu_save_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Save &as...       F12"), menu_save_as_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Insert file...   F15"), menu_insert_file_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Copy to &file...  C-f"), menu_cut_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&User menu...     F11"), menu_user_menu_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("A&bout...            "), edit_about_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Quit             F10"), menu_quit_cmd));

    return entries;
}

static GPtrArray *
create_file_menu_emacs (void)
{
    GPtrArray *entries = g_ptr_array_new ();

    g_ptr_array_add (entries, menu_entry_create (_("&Open file..."),         menu_load_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&New            C-x k"), menu_new_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Save              F2"), menu_save_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Save &as...       F12"), menu_save_as_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Insert file...   F15"), menu_insert_file_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Copy to &file...     "), menu_cut_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&User menu...     F11"), menu_user_menu_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("A&bout...            "), edit_about_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Quit             F10"), menu_quit_cmd));

    return entries;
}

static GPtrArray *
create_edit_menu (void)
{
    GPtrArray *entries = g_ptr_array_new ();

    g_ptr_array_add (entries, menu_entry_create (_("&Toggle Mark       F3"), menu_mark_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Mark Columns    S-F3"), menu_markcol_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Toggle &ins/overw Ins"), menu_ins_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Copy              F5"), menu_copy_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Move              F6"), menu_move_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Delete            F8"), menu_delete_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("C&opy to clipfile         C-Ins"), menu_xstore_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("C&ut to clipfile          S-Del"), menu_xcut_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Paste from clipfile      S-Ins"), menu_xpaste_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Toggle bookmar&k            M-k"), menu_toggle_bookmark_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Next bookmark              M-j"), menu_next_bookmark_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Pre&v bookmark              M-i"), menu_prev_bookmark_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Flush bookmark             M-o"), menu_flush_bookmark_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Undo                       C-u"), menu_undo_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Beginning     C-PgUp"), menu_beginning_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&End           C-PgDn"), menu_end_cmd));

    return entries;
}

static GPtrArray *
create_edit_menu_emacs (void)
{
    GPtrArray *entries = g_ptr_array_new ();

    g_ptr_array_add (entries, menu_entry_create (_("&Toggle mark                 F3"), menu_mark_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Mar&k columns              S-F3"), menu_markcol_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Toggle &ins/overw           Ins"), menu_ins_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Copy                        F5"), menu_copy_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Move                        F6"), menu_move_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Delete                      F8"), menu_delete_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("C&opy to clipfile           M-w"), menu_xstore_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("C&ut to clipfile            C-w"), menu_xcut_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Paste from clipfile        C-y"), menu_xpaste_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Toggle bookmar&k               "), menu_toggle_bookmark_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Next bookmark                 "), menu_next_bookmark_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Pre&v bookmark                 "), menu_prev_bookmark_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Flush bookmark                "), menu_flush_bookmark_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Undo                       C-u"), menu_undo_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Beginning               C-PgUp"), menu_beginning_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&End                     C-PgDn"), menu_end_cmd));

    return entries;
}

static GPtrArray *
create_search_replace_menu (void)
{
    GPtrArray *entries = g_ptr_array_new ();

    g_ptr_array_add (entries, menu_entry_create (_("&Search...         F7"), menu_search_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Search &again     F17"), menu_search_again_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Replace...        F4"), menu_replace_cmd));

    return entries;
}

static GPtrArray *
create_command_menu (void)
{
    GPtrArray *entries = g_ptr_array_new ();

    g_ptr_array_add (entries, menu_entry_create (_("&Go to line...            M-l"), menu_goto_line));
    g_ptr_array_add (entries, menu_entry_create (_("Toggle li&ne state        M-n"), menu_toggle_line_state));
    g_ptr_array_add (entries, menu_entry_create (_("Go to matching &bracket   M-b"), menu_goto_bracket));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Find declaration      A-Enter"), menu_find_declare));
    g_ptr_array_add (entries, menu_entry_create (_("Back from declaration     M--"), menu_declare_back));
    g_ptr_array_add (entries, menu_entry_create (_("Forward to declaration    M-+"), menu_declare_forward));
#ifdef HAVE_CHARSET
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Encod&ing...             M-e"), menu_select_codepage_cmd));
#endif
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Insert &literal...       C-q"),  menu_lit_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Refresh screen          C-l"),  menu_refresh_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Start record macro      C-r"),  menu_begin_record_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Finish record macro...  C-r"),  menu_end_record_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Execute macro...   C-a, KEY"),  menu_exec_macro_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Delete macr&o...            "),  menu_exec_macro_delete_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Insert &date/time           "),  menu_date_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Format p&aragraph        M-p"),  menu_format_paragraph));
    g_ptr_array_add (entries, menu_entry_create (_("'ispell' s&pell check    C-p"),  menu_ispell_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Sor&t...                 M-t"),  menu_sort_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Paste o&utput of...      M-u"),  menu_ext_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("E&xternal Formatter      F19"),  menu_c_form_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Mail...                    "),  menu_mail_cmd));

    return entries;
}

static GPtrArray *
create_command_menu_emacs (void)
{
    GPtrArray *entries = g_ptr_array_new ();

    g_ptr_array_add (entries, menu_entry_create (_("&Go to line...            M-l"), menu_goto_line));
    g_ptr_array_add (entries, menu_entry_create (_("Toggle li&ne state        M-n"), menu_toggle_line_state));
    g_ptr_array_add (entries, menu_entry_create (_("Go to matching &bracket   M-b"), menu_goto_bracket));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Find declaration      A-Enter"), menu_find_declare));
    g_ptr_array_add (entries, menu_entry_create (_("Back from declaration     M--"), menu_declare_back));
    g_ptr_array_add (entries, menu_entry_create (_("Forward to declaration    M-+"), menu_declare_forward));
#ifdef HAVE_CHARSET
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Encod&ing...             C-t"), menu_select_codepage_cmd));
#endif
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Insert &literal...       C-q"), menu_lit_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Refresh screen          C-l"), menu_refresh_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("&Start record macro      C-r"), menu_begin_record_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Finish record macro...  C-r"), menu_end_record_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Execute macro... C-x e, KEY"), menu_exec_macro_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Delete macr&o...            "), menu_exec_macro_delete_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Insert &date/time           "), menu_date_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Format p&aragraph        M-p"), menu_format_paragraph));
    g_ptr_array_add (entries, menu_entry_create (_("'ispell' s&pell check    M-$"), menu_ispell_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Sor&t...                 M-t"), menu_sort_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Paste o&utput of...      M-u"), menu_ext_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("E&xternal Formatter      F19"), menu_c_form_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Mail...                    "), menu_mail_cmd));

    return entries;
}

static GPtrArray *
create_options_menu (void)
{
    GPtrArray *entries = g_ptr_array_new ();

    g_ptr_array_add (entries, menu_entry_create (_("&General...  "),           edit_options_dialog));
    g_ptr_array_add (entries, menu_entry_create (_("&Save mode..."),           menu_save_mode_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("Learn &Keys..."),          learn_keys));
    g_ptr_array_add (entries, menu_entry_create (_("Syntax &Highlighting..."), edit_syntax_dialog));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("S&yntax file"),            menu_edit_syntax_file_cmd));
    g_ptr_array_add (entries, menu_entry_create (_("&Menu file"),              menu_edit_menu_file_cmd));
    g_ptr_array_add (entries, menu_separator_create ());
    g_ptr_array_add (entries, menu_entry_create (_("Save setu&p..."),          save_setup_cmd));

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
