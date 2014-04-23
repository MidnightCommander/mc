/*
   Editor's events definitions.

   Copyright (C) 2012-2014
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2014

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
#include "lib/event.h"

#include "event.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_filemanager_init_events (GError ** error)
{
    /* *INDENT-OFF* */
    event_init_group_t treeview_group_events[] =
    {
        {"help", mc_tree_cmd_help, NULL},
        {"forget", mc_tree_cmd_forget, NULL},
        {"navigation_mode_toggle", mc_tree_cmd_navigation_mode_toggle, NULL},
        {"copy", mc_tree_cmd_copy, NULL},
        {"move", mc_tree_cmd_move, NULL},
        {"goto_up", mc_tree_cmd_goto_up, NULL},
        {"goto_down", mc_tree_cmd_goto_down, NULL},
        {"goto_home", mc_tree_cmd_goto_home, NULL},
        {"goto_end", mc_tree_cmd_goto_end, NULL},
        {"goto_page_up", mc_tree_cmd_goto_page_up, NULL},
        {"goto_page_down", mc_tree_cmd_goto_page_down, NULL},
        {"goto_left", mc_tree_cmd_goto_left, NULL},
        {"goto_right", mc_tree_cmd_goto_right, NULL},
        {"enter", mc_tree_cmd_enter, NULL},
        {"rescan", mc_tree_cmd_rescan, NULL},
        {"search_begin", mc_tree_cmd_search_begin, NULL},
        {"rmdir", mc_tree_cmd_rmdir, NULL},
        {"chdir", mc_tree_cmd_chdir, NULL},
        {"show_box", mc_tree_cmd_show_box, NULL},

        {NULL, NULL, NULL}
    };
    /* *INDENT-ON* */

    /* *INDENT-OFF* */
    event_init_group_t filemanager_group_events[] =
    {
        {"update_panels", mc_panel_cmd_update_panels, NULL},
        {"save_current_file_to_clip_file", mc_panel_cmd_save_current_file_to_clip_file, NULL},
        {"chdir_other", mc_panel_cmd_chdir_other, NULL},
        {"chdir_other_if_link", mc_panel_cmd_chdir_other_if_link, NULL},
        {"copy_single", mc_panel_cmd_copy_single, NULL},
        {"delete_single", mc_panel_cmd_delete_single, NULL},
        {"enter", mc_panel_cmd_enter, NULL},
        {"view_raw", mc_panel_cmd_file_view_raw, NULL},
        {"view", mc_panel_cmd_file_view, NULL},
        {"edit_new", mc_panel_cmd_edit_new, NULL},
        {"rename_single", mc_panel_cmd_rename_single, NULL},
        {"goto_page_down", mc_panel_cmd_goto_page_down, NULL},
        {"goto_page_up", mc_panel_cmd_goto_page_up, NULL},
        {"ch_sub_dir", mc_panel_cmd_ch_sub_dir, NULL},
        {"ch_parent_dir", mc_panel_cmd_ch_parent_dir, NULL},
        {"directory_history_list", mc_panel_cmd_directory_history_list, NULL},
        {"directory_history_next", mc_panel_cmd_directory_history_next, NULL},
        {"directory_history_prev", mc_panel_cmd_directory_history_prev, NULL},
        {"goto_bottom_screen", mc_panel_cmd_goto_bottom_screen, NULL},
        {"goto_middle_screen", mc_panel_cmd_goto_middle_screen, NULL},
        {"goto_top_screen", mc_panel_cmd_goto_top_screen, NULL},
        {"mark", mc_panel_cmd_mark, NULL},
        {"mark_up", mc_panel_cmd_mark_up, NULL},
        {"mark_down", mc_panel_cmd_mark_down, NULL},
        {"mark_left", mc_panel_cmd_mark_left, NULL},
        {"mark_right", mc_panel_cmd_mark_right, NULL},
        {"cd_parent_smart", mc_panel_cmd_cd_parent_smart, NULL},

        {"goto_up", mc_panel_cmd_goto_up, NULL},
        {"goto_down", mc_panel_cmd_goto_down, NULL},
        {"goto_left", mc_panel_cmd_goto_left, NULL},
        {"goto_right", mc_panel_cmd_goto_right, NULL},
        {"goto_home", mc_panel_cmd_goto_home, NULL},
        {"goto_end", mc_panel_cmd_goto_end, NULL},

        {"content_scroll_left", mc_panel_cmd_content_scroll_left, NULL},
        {"content_scroll_right", mc_panel_cmd_content_scroll_right, NULL},

        {"search", mc_panel_cmd_search, NULL},
        {"search_stop", mc_panel_cmd_search_stop, NULL},
        {"sync_other", mc_panel_cmd_sync_other, NULL},

        {"sort_order_select", mc_panel_cmd_sort_order_select, NULL},
        {"sort_order_prev", mc_panel_cmd_sort_order_prev, NULL},
        {"sort_order_next", mc_panel_cmd_sort_order_next, NULL},
        {"sort_order_reverse", mc_panel_cmd_sort_order_next, NULL},
        {"sort_by_name", mc_panel_cmd_sort_order_next, NULL},
        {"sort_by_extension", mc_panel_cmd_sort_order_next, NULL},
        {"sort_by_size", mc_panel_cmd_sort_order_next, NULL},
        {"sort_by_mtime", mc_panel_cmd_sort_order_next, NULL},

        {"select_files", mc_panel_cmd_select_files, NULL},
        {"unselect_files", mc_panel_cmd_unselect_files, NULL},
        {"select_invert_files", mc_panel_cmd_select_invert_files, NULL},
        {"select_files_by_extension", mc_panel_cmd_select_files_by_extension, NULL},

#ifdef HAVE_CHARSET
        {"select_codepage", mc_panel_cmd_select_codepage, NULL},
#endif /* HAVE_CHARSET */

        {NULL, NULL, NULL}
    };
    /* *INDENT-ON* */

    /* *INDENT-OFF* */
    event_init_t standard_events[] =
    {
        {MCEVENT_GROUP_TREEVIEW, treeview_group_events},
        {MCEVENT_GROUP_FILEMANAGER, filemanager_group_events},
        {NULL, NULL}
    };
    /* *INDENT-ON* */

    mc_event_mass_add (standard_events, error);

}

/* --------------------------------------------------------------------------------------------- */
