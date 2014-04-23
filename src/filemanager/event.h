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

/*** inline functions ****************************************************************************/

#endif /* MC__FILEMANAGER_EVENT_H */
