#ifndef MC__FILEMANAGER_EVENT_H
#define MC__FILEMANAGER_EVENT_H

#include "lib/event.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void mc_filemanager_init_events (GError ** error);

gboolean mc_tree_cmd_help (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_forget (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_navigation_mode_toggle (event_info_t * event_info, gpointer data,
                                             GError ** error);
gboolean mc_tree_cmd_copy (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_move (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_goto_up (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_goto_down (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_goto_home (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_goto_end (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_goto_page_up (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_goto_page_down (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_goto_left (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_goto_right (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_enter (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_rescan (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_search_begin (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_rmdir (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_chdir (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_tree_cmd_show_box (event_info_t * event_info, gpointer data, GError ** error);

gboolean mc_panel_cmd_update_panels (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_save_current_file_to_clip_file (event_info_t * event_info, gpointer data,
                                                      GError ** error);
gboolean mc_panel_cmd_chdir_other (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_chdir_other_if_link (event_info_t * event_info, gpointer data,
                                           GError ** error);
gboolean mc_panel_cmd_copy_single (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_delete_single (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_enter (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_file_view_raw (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_file_view (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_edit_new (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_rename_single (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_goto_page_down (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_goto_page_up (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_ch_sub_dir (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_ch_parent_dir (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_directory_history_list (event_info_t * event_info, gpointer data,
                                              GError ** error);
gboolean mc_panel_cmd_directory_history_next (event_info_t * event_info, gpointer data,
                                              GError ** error);
gboolean mc_panel_cmd_directory_history_prev (event_info_t * event_info, gpointer data,
                                              GError ** error);
gboolean mc_panel_cmd_goto_bottom_screen (event_info_t * event_info, gpointer data,
                                          GError ** error);
gboolean mc_panel_cmd_goto_middle_screen (event_info_t * event_info, gpointer data,
                                          GError ** error);
gboolean mc_panel_cmd_goto_top_screen (event_info_t * event_info, gpointer data, GError ** error);

gboolean mc_panel_cmd_mark (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_mark_up (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_mark_down (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_mark_left (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_mark_right (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_cd_parent_smart (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_goto_up (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_goto_down (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_goto_left (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_goto_right (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_goto_home (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_goto_end (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_content_scroll_left (event_info_t * event_info, gpointer data,
                                           GError ** error);
gboolean mc_panel_cmd_content_scroll_right (event_info_t * event_info, gpointer data,
                                            GError ** error);
gboolean mc_panel_cmd_search (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_search_stop (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_sync_other (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_sort_order_select (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_sort_order_prev (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_sort_order_next (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_sort_order_reverse (event_info_t * event_info, gpointer data,
                                          GError ** error);
gboolean mc_panel_cmd_sort_by_name (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_sort_by_extension (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_sort_by_size (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_sort_by_mtime (event_info_t * event_info, gpointer data, GError ** error);

gboolean mc_panel_cmd_select_files (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_unselect_files (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_select_invert_files (event_info_t * event_info, gpointer data,
                                           GError ** error);
gboolean mc_panel_cmd_select_files_by_extension (event_info_t * event_info, gpointer data,
                                                 GError ** error);

#ifdef HAVE_CHARSET
gboolean mc_panel_cmd_select_codepage (event_info_t * event_info, gpointer data, GError ** error);
#endif /* HAVE_CHARSET */

gboolean mc_filemanager_cmd_hotlist_add (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_change_listing_mode (event_info_t * event_info, gpointer data,
                                                 GError ** error);
gboolean mc_filemanager_cmd_chmod (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_chown (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_chown_advanced (event_info_t * event_info, gpointer data,
                                            GError ** error);
gboolean mc_filemanager_cmd_compare_dirs (event_info_t * event_info, gpointer data,
                                          GError ** error);
gboolean mc_filemanager_cmd_configuration_show_dialog (event_info_t * event_info, gpointer data,
                                                       GError ** error);
#ifdef ENABLE_VFS
gboolean mc_filemanager_cmd_configuration_vfs_show_dialog (event_info_t * event_info, gpointer data,
                                                           GError ** error);
#endif /* ENABLE_VFS */
gboolean mc_filemanager_cmd_configuration_confirmations_show_dialog (event_info_t * event_info,
                                                                     gpointer data,
                                                                     GError ** error);
gboolean mc_filemanager_cmd_copy (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_put_path_to_cmdline (event_info_t * event_info, gpointer data,
                                                 GError ** error);
gboolean mc_filemanager_cmd_put_link_to_cmdline (event_info_t * event_info, gpointer data,
                                                 GError ** error);
gboolean mc_filemanager_cmd_put_tagged_to_cmdline (event_info_t * event_info, gpointer data,
                                                   GError ** error);
gboolean mc_filemanager_cmd_delete (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_run_diffviewer (event_info_t * event_info, gpointer data,
                                            GError ** error);
gboolean mc_filemanager_cmd_configuration_display_bits_show_dialog (event_info_t * event_info,
                                                                    gpointer data, GError ** error);
gboolean mc_filemanager_cmd_run_editor (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_run_editor_internal (event_info_t * event_info, gpointer data,
                                                 GError ** error);
gboolean mc_filemanager_cmd_extention_rules_file_edit (event_info_t * event_info, gpointer data,
                                                       GError ** error);
gboolean mc_filemanager_cmd_user_menu_edit (event_info_t * event_info, gpointer data,
                                            GError ** error);
gboolean mc_filemanager_cmd_file_highlight_rules_edit (event_info_t * event_info, gpointer data,
                                                       GError ** error);
gboolean mc_filemanager_cmd_symlink_edit (event_info_t * event_info, gpointer data,
                                          GError ** error);
gboolean mc_filemanager_cmd_external_panelize (event_info_t * event_info, gpointer data,
                                               GError ** error);
gboolean mc_filemanager_cmd_panelize (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_filter (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_view_filtered (event_info_t * event_info, gpointer data,
                                           GError ** error);
gboolean mc_filemanager_cmd_find_file (event_info_t * event_info, gpointer data, GError ** error);
#ifdef ENABLE_VFS_FISH
gboolean mc_filemanager_cmd_fish_connect_show_dialog (event_info_t * event_info, gpointer data,
                                                      GError ** error);
#endif
#ifdef ENABLE_VFS_FTP
gboolean mc_filemanager_cmd_ftp_connect_show_dialog (event_info_t * event_info, gpointer data,
                                                     GError ** error);
#endif
#ifdef ENABLE_VFS_SFTP
gboolean mc_filemanager_cmd_sftp_connect_show_dialog (event_info_t * event_info, gpointer data,
                                                      GError ** error);
#endif
#ifdef ENABLE_VFS_SMB
gboolean mc_filemanager_cmd_smb_connect_show_dialog (event_info_t * event_info, gpointer data,
                                                     GError ** error);
#endif
gboolean mc_filemanager_cmd_help (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_panel_info (event_info_t * event_info, gpointer data, GError ** error);

#ifdef ENABLE_BACKGROUND
gboolean mc_filemanager_cmd_show_background_jobs (event_info_t * event_info, gpointer data,
                                                  GError ** error);
#endif
gboolean mc_filemanager_cmd_configuration_layout_show_dialog (event_info_t * event_info,
                                                              gpointer data, GError ** error);
gboolean mc_filemanager_cmd_configuration_appearance_show_dialog (event_info_t * event_info,
                                                                  gpointer data, GError ** error);
gboolean mc_filemanager_cmd_hard_link (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_sym_link_relative (event_info_t * event_info, gpointer data,
                                               GError ** error);
gboolean mc_filemanager_cmd_sym_link_absolute (event_info_t * event_info, gpointer data,
                                               GError ** error);
gboolean mc_filemanager_cmd_panel_listing (event_info_t * event_info, gpointer data,
                                           GError ** error);
gboolean mc_filemanager_cmd_panel_listing_switch (event_info_t * event_info, gpointer data,
                                                  GError ** error);
#ifdef LISTMODE_EDITOR
gboolean mc_filemanager_cmd_listmode (event_info_t * event_info, gpointer data, GError ** error);
#endif
gboolean mc_filemanager_cmd_menu (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_menu_last_selected (event_info_t * event_info, gpointer data,
                                                GError ** error);
gboolean mc_filemanager_cmd_mkdir (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_configuration_panel_show_dialog (event_info_t * event_info,
                                                             gpointer data, GError ** error);
#ifdef HAVE_CHARSET
gboolean mc_filemanager_cmd_select_encoding (event_info_t * event_info, gpointer data,
                                             GError ** error);
#endif
gboolean mc_filemanager_cmd_quick_cd (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_hotlist (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_panel_quick_view (event_info_t * event_info, gpointer data,
                                              GError ** error);
gboolean mc_filemanager_cmd_quiet_quit (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_quit (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_panel_cmd_rename (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_reread (event_info_t * event_info, gpointer data, GError ** error);

#ifdef ENABLE_VFS
gboolean mc_filemanager_cmd_show_vfs_list (event_info_t * event_info, gpointer data,
                                           GError ** error);
#endif

gboolean mc_filemanager_cmd_toggle_hidden (event_info_t * event_info, gpointer data,
                                           GError ** error);
gboolean mc_filemanager_cmd_view_other (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_smart_dirsize (event_info_t * event_info, gpointer data,
                                           GError ** error);
gboolean mc_filemanager_cmd_sort (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_ctl_x (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_swap (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_toggle_panels_split (event_info_t * event_info, gpointer data,
                                                 GError ** error);
gboolean mc_filemanager_cmd_panels_split_equal (event_info_t * event_info, gpointer data,
                                                GError ** error);
gboolean mc_filemanager_cmd_panels_split_less (event_info_t * event_info, gpointer data,
                                               GError ** error);
gboolean mc_filemanager_cmd_panels_split_more (event_info_t * event_info, gpointer data,
                                               GError ** error);
gboolean mc_filemanager_cmd_panel_tree (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_filemanager_cmd_treebox (event_info_t * event_info, gpointer data, GError ** error);

#ifdef ENABLE_VFS_UNDELFS
gboolean mc_filemanager_cmd_undelete (event_info_t * event_info, gpointer data, GError ** error);
#endif

gboolean mc_filemanager_cmd_user_file_menu (event_info_t * event_info, gpointer data,
                                            GError ** error);
gboolean mc_filemanager_cmd_view_file (event_info_t * event_info, gpointer data, GError ** error);

/*** inline functions ****************************************************************************/

#endif /* MC__FILEMANAGER_EVENT_H */
