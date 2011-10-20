/*
   src/editor - common include files for testing static functions

   Copyright (C) 2011
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "edit-widget.h"
#include "lib/global.h"
#include "lib/keybind.h"

/* ********************************************************************************************* */
/* mock variables and functions */

int drop_menus = 0;
const global_keymap_t *editor_map = NULL;
const global_keymap_t *editor_x_map = NULL;
GArray *macros_list = NULL;
int option_tab_spacing = 8;
int macro_index = -1;

static gboolean
do_select_codepage (void)
{
    return TRUE;
}
static gboolean
user_menu_cmd (struct WEdit *edit_widget, const char *menu_file, int selected_entry)
{
    (void) edit_widget;
    (void) menu_file;
    (void) selected_entry;
    return TRUE;
}
static int
check_for_default (const char *default_file, const char *file)
{
    (void) default_file;
    (void) file;
    return 0;
}
static void
save_setup_cmd (void)
{
}
static void
learn_keys (void)
{
}
static void
view_other_cmd (void)
{
}

/* ********************************************************************************************* */

#include "bookmark.c"
#include "edit.c"
#include "editcmd.c"
#include "editwidget.c"
#include "editdraw.c"
#include "editkeys.c"
#include "editmenu.c"
#include "editoptions.c"
#include "syntax.c"
#include "wordproc.c"
#include "choosesyntax.c"
#include "etags.c"
#include "editcmd_dialogs.c"
