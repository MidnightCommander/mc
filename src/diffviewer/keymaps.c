/*
   Default values for keymapping engine

   Copyright (C) 2009-2014
   Free Software Foundation, Inc.

   Written by:
   Vitja Makarov, 2005
   Ilia Maslakov <il.smind@gmail.com>, 2009, 2010
   Andrew Borodin <aborodin@vmail.ru>, 2010, 2011

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

#include <config.h>

#include "lib/global.h"
#include "lib/widget.h"         /* dialog_map, input_map, listbox_map */
#include "lib/keymap.h"

#include "src/keybind-defaults.h"
#include "internal.h"

/*** global variables ****************************************************************************/

GArray *diff_keymap = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/* default keymaps in ini (key=value) format */

/*** file scope variables ************************************************************************/

/* diff viewer */
static const default_keymap_ini_t default_diff_keymap[] = {
    {"ShowSymbols", "alt-s; s"},
    {"ShowNumbers", "alt-n; l"},
    {"SplitFull", "f"},
    {"SplitEqual", "equal"},
    {"SplitMore", "gt"},
    {"SplitLess", "lt"},
    {"Tab2", "2"},
    {"Tab3", "3"},
    {"Tab4", "4"},
    {"Tab8", "8"},
    {"Swap", "ctrl-u"},
    {"Redo", "ctrl-r"},
    {"HunkNext", "n; enter; space"},
    {"HunkPrev", "p; backspace"},
    {"Goto", "g; shift-g"},
    {"Save", "f2"},
    {"Edit", "f4"},
    {"EditOther", "f14"},
    {"Merge", "f5"},
    {"MergeOther", "f15"},
    {"Search", "f7"},
    {"SearchContinue", "f17"},
    {"Options", "f9"},
    {"Top", "ctrl-home"},
    {"Bottom", "ctrl-end"},
    {"Down", "down"},
    {"Up", "up"},
    {"LeftQuick", "ctrl-left"},
    {"RightQuick", "ctrl-right"},
    {"Left", "left"},
    {"Right", "right"},
    {"PageDown", "pgdn"},
    {"PageUp", "pgup"},
    {"Home", "home"},
    {"End", "end"},
    {"Quit", "f10; q; shift-q; esc"},
#ifdef HAVE_CHARSET
    {"SelectCodepage", "alt-e"},
#endif
    {"Shell", "ctrl-o"},
    {NULL, NULL}
};


static const mc_keymap_event_init_group_t mc_keymap_event_diffview[] = {
    {"ShowSymbols", MCEVENT_GROUP_DIFFVIEWER, "show_symbols"},
    {"ShowNumbers", MCEVENT_GROUP_DIFFVIEWER, "show_numbers"},
    {"SplitFull", MCEVENT_GROUP_DIFFVIEWER, "split_full"},
    {"SplitEqual", MCEVENT_GROUP_DIFFVIEWER, "split_equal"},
    {"SplitMore", MCEVENT_GROUP_DIFFVIEWER, "split_more"},
    {"SplitLess", MCEVENT_GROUP_DIFFVIEWER, "split_less"},
    {"Tab2", MCEVENT_GROUP_DIFFVIEWER, "tab_size_2"},
    {"Tab3", MCEVENT_GROUP_DIFFVIEWER, "tab_size_3"},
    {"Tab4", MCEVENT_GROUP_DIFFVIEWER, "tab_size_4"},
    {"Tab8", MCEVENT_GROUP_DIFFVIEWER, "tab_size_8"},
    {"Swap", MCEVENT_GROUP_DIFFVIEWER, "swap"},
    {"Redo", MCEVENT_GROUP_DIFFVIEWER, "redo"},
    {"HunkNext", MCEVENT_GROUP_DIFFVIEWER, "hunk_next"},
    {"HunkPrev", MCEVENT_GROUP_DIFFVIEWER, "hunk_prev"},
    {"Goto", MCEVENT_GROUP_DIFFVIEWER, "goto_line"},
    {"Save", MCEVENT_GROUP_DIFFVIEWER, "save_changes"},
    {"Edit", MCEVENT_GROUP_DIFFVIEWER, "edit_current"},
    {"EditOther", MCEVENT_GROUP_DIFFVIEWER, "edit_other"},
    {"Merge", MCEVENT_GROUP_DIFFVIEWER, "merge_from_left_to_right"},
    {"MergeOther", MCEVENT_GROUP_DIFFVIEWER, "merge_from_right_to_left"},
    {"Search", MCEVENT_GROUP_DIFFVIEWER, "search"},
    {"SearchContinue", MCEVENT_GROUP_DIFFVIEWER, "continue_search"},
    {"Options", MCEVENT_GROUP_DIFFVIEWER, "options_show_dialog"},
    {"Top", MCEVENT_GROUP_DIFFVIEWER, "goto_top"},
    {"Bottom", MCEVENT_GROUP_DIFFVIEWER, "goto_bottom"},
    {"Down", MCEVENT_GROUP_DIFFVIEWER, "goto_down"},
    {"Up", MCEVENT_GROUP_DIFFVIEWER, "goto_up"},
    {"LeftQuick", MCEVENT_GROUP_DIFFVIEWER, "goto_left_quick"},
    {"RightQuick", MCEVENT_GROUP_DIFFVIEWER, "goto_right_quick"},
    {"Left", MCEVENT_GROUP_DIFFVIEWER, "goto_left"},
    {"Right", MCEVENT_GROUP_DIFFVIEWER, "goto_right"},
    {"PageDown", MCEVENT_GROUP_DIFFVIEWER, "goto_page_down"},
    {"PageUp", MCEVENT_GROUP_DIFFVIEWER, "goto_page_up"},
    {"Home", MCEVENT_GROUP_DIFFVIEWER, "goto_start_of_line"},
    {"Quit", MCEVENT_GROUP_DIFFVIEWER, "quit"},
#ifdef HAVE_CHARSET
    {"SelectCodepage", MCEVENT_GROUP_DIFFVIEWER, "select_encoding_show_dialog"},
#endif
    {"Shell", MCEVENT_GROUP_FILEMANAGER, "view_other"},
    {NULL, NULL, NULL}
};


static const mc_keymap_event_init_t mc_keymap_event_defaults[] = {
    {KEYMAP_SECTION_DIFFVIEWER, mc_keymap_event_diffview},
    {NULL, NULL}
};


/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_diffviewer_init_keymaps (GError ** error)
{
    mc_core_keybind_mass_init (KEYMAP_SECTION_DIFFVIEWER, default_diff_keymap, FALSE, error);
    mc_keymap_mass_bind_event (mc_keymap_event_defaults, error);
}
