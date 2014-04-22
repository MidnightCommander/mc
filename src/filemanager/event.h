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


/*** inline functions ****************************************************************************/

#endif /* MC__FILEMANAGER_EVENT_H */
