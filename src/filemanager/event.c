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
    event_init_group_t filemanager_panel_group_events[] =
    {
        {"update_panels", mc_panel_cmd_update_panels, NULL},
        {"save_current_file_to_clip_file", mc_panel_cmd_save_current_file_to_clip_file, NULL},
        {"chdir_other", mc_panel_cmd_chdir_other, NULL},
        {"chdir_other_if_link", mc_panel_cmd_chdir_other_if_link, NULL},
        {"rename", mc_panel_cmd_rename, NULL},
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
    event_init_group_t filemanager_group_events[] =
    {
        {"help", mc_filemanager_cmd_help, NULL},
        {"hotlist_add", mc_filemanager_cmd_hotlist_add, NULL},
        {"chmod", mc_filemanager_cmd_chmod, NULL},
        {"chown", mc_filemanager_cmd_chown, NULL},
        {"chown_advanced", mc_filemanager_cmd_chown, NULL},
        {"compare_dirs", mc_filemanager_cmd_compare_dirs, NULL},
        {"configuration_show_dialog", mc_filemanager_cmd_configuration_show_dialog, NULL},
        {"configuration_confirmations_show_dialog", mc_filemanager_cmd_configuration_confirmations_show_dialog, NULL},
        {"configuration_display_bits_show_dialog", mc_filemanager_cmd_configuration_display_bits_show_dialog, NULL},
        {"configuration_layout_show_dialog", mc_filemanager_cmd_configuration_layout_show_dialog, NULL},
        {"configuration_appearance_show_dialog", mc_filemanager_cmd_configuration_appearance_show_dialog, NULL},
        {"configuration_panel_show_dialog", mc_filemanager_cmd_configuration_panel_show_dialog, NULL},
        {"change_listing_mode", mc_filemanager_cmd_change_listing_mode, NULL},
        {"copy", mc_filemanager_cmd_copy, NULL},
        {"put_path_to_cmdline", mc_filemanager_cmd_put_path_to_cmdline, NULL},
        {"put_link_to_cmdline", mc_filemanager_cmd_put_link_to_cmdline, NULL},
        {"put_tagged_to_cmdline", mc_filemanager_cmd_put_tagged_to_cmdline, NULL},
        {"delete", mc_filemanager_cmd_delete, NULL},
        {"run_diffviewer", mc_filemanager_cmd_run_diffviewer, NULL},
        {"run_editor", mc_filemanager_cmd_run_editor, NULL},
        {"run_editor_internal", mc_filemanager_cmd_run_editor_internal, NULL},
        {"extention_rules_file_edit", mc_filemanager_cmd_extention_rules_file_edit, NULL},
        {"file_highlight_rules_edit", mc_filemanager_cmd_file_highlight_rules_edit, NULL},
        {"user_menu_edit", mc_filemanager_cmd_user_menu_edit, NULL},
        {"symlink_edit", mc_filemanager_cmd_symlink_edit, NULL},
        {"external_panelize", mc_filemanager_cmd_external_panelize, NULL},
        {"filter", mc_filemanager_cmd_filter, NULL},
        {"view_filtered", mc_filemanager_cmd_view_filtered, NULL},
        {"find_file", mc_filemanager_cmd_find_file, NULL},
        {"panelize", mc_filemanager_cmd_panelize, NULL},
        {"panel_info", mc_filemanager_cmd_panel_info, NULL},
        {"hard_link", mc_filemanager_cmd_hard_link, NULL},
        {"panel_listing", mc_filemanager_cmd_panel_listing, NULL},
        {"menu", mc_filemanager_cmd_menu, NULL},
        {"menu_last_selected", mc_filemanager_cmd_menu_last_selected, NULL},
        {"mkdir", mc_filemanager_cmd_mkdir, NULL},
        {"quick_cd", mc_filemanager_cmd_quick_cd, NULL},
        {"hotlist", mc_filemanager_cmd_hotlist, NULL},
        {"panel_quick_view", mc_filemanager_cmd_panel_quick_view, NULL},
        {"quiet_quit", mc_filemanager_cmd_quiet_quit, NULL},
        {"quit", mc_filemanager_cmd_quit, NULL},
        {"sym_link_relative", mc_filemanager_cmd_sym_link_relative, NULL},
        {"reread", mc_filemanager_cmd_reread, NULL},
        {"view_other", mc_filemanager_cmd_view_other, NULL},
        {"smart_dirsize", mc_filemanager_cmd_smart_dirsize, NULL},
        {"sort", mc_filemanager_cmd_sort, NULL},
        {"ctl_x", mc_filemanager_cmd_ctl_x, NULL},
        {"swap", mc_filemanager_cmd_swap, NULL},
        {"sym_link_absolute", mc_filemanager_cmd_sym_link_absolute, NULL},

        {"panel_listing_switch", mc_filemanager_cmd_panel_listing_switch, NULL},
        {"toggle_hidden", mc_filemanager_cmd_toggle_hidden, NULL},
        {"toggle_panels_split", mc_filemanager_cmd_toggle_panels_split, NULL},
        {"panels_split_equal", mc_filemanager_cmd_panels_split_equal, NULL},
        {"panels_split_less", mc_filemanager_cmd_panels_split_less, NULL},
        {"panels_split_more", mc_filemanager_cmd_panels_split_more, NULL},
        {"panel_tree", mc_filemanager_cmd_panel_tree, NULL},
        {"treebox", mc_filemanager_cmd_treebox, NULL},
        {"user_file_menu", mc_filemanager_cmd_user_file_menu, NULL},
        {"view_file", mc_filemanager_cmd_view_file, NULL},

#ifdef ENABLE_VFS
        {"configuration_vfs_show_dialog", mc_filemanager_cmd_configuration_vfs_show_dialog, NULL},
#endif /* ENABLE_VFS */
#ifdef ENABLE_VFS_FISH
        {"fish_connect_show_dialog", mc_filemanager_cmd_fish_connect_show_dialog, NULL},
#endif
#ifdef ENABLE_VFS_FTP
        {"ftp_connect_show_dialog", mc_filemanager_cmd_ftp_connect_show_dialog, NULL},
#endif
#ifdef ENABLE_VFS_SFTP
        {"sftp_connect_show_dialog", mc_filemanager_cmd_sftp_connect_show_dialog, NULL},
#endif
#ifdef ENABLE_VFS_SMB
        {"smb_connect_show_dialog", mc_filemanager_cmd_smb_connect_show_dialog, NULL},
#endif
#ifdef ENABLE_BACKGROUND
        {"show_background_jobs", mc_filemanager_cmd_show_background_jobs, NULL},
#endif
#ifdef LISTMODE_EDITOR
        {"listmode", mc_filemanager_cmd_listmode, NULL},
#endif
#ifdef HAVE_CHARSET
        {"select_encoding", mc_filemanager_cmd_select_encoding, NULL},
#endif
#ifdef ENABLE_VFS
        {"show_vfs_list", mc_filemanager_cmd_show_vfs_list, NULL},
#endif
#ifdef ENABLE_VFS_UNDELFS
        {"undelete", mc_filemanager_cmd_undelete, NULL},
#endif

        {NULL, NULL, NULL}
    };
    /* *INDENT-ON* */

    /* *INDENT-OFF* */
    event_init_t standard_events[] =
    {
        {MCEVENT_GROUP_TREEVIEW, treeview_group_events},
        {MCEVENT_GROUP_FILEMANAGER, filemanager_group_events},
        {MCEVENT_GROUP_FILEMANAGER_PANEL, filemanager_panel_group_events},
        {NULL, NULL}
    };
    /* *INDENT-ON* */

    mc_event_mass_add (standard_events, error);

}

/* --------------------------------------------------------------------------------------------- */
