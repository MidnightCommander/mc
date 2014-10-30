#ifndef MC__EVENTS_INIT_H
#define MC__EVENTS_INIT_H

#include "lib/event.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/


/*** declarations of public functions ************************************************************/

gboolean events_init (GError **);

gboolean mc_core_cmd_configuration_learn_keys_show_dialog (event_info_t * event_info, gpointer data,
                                                           GError ** error);
gboolean mc_core_cmd_save_setup (event_info_t * event_info, gpointer data, GError ** error);

gboolean mc_help_cmd_interactive_display (event_info_t * event_info, gpointer data,
                                          GError ** error);

gboolean mc_help_cmd_show_dialog (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_index (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_back (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_down (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_next_link (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_up (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_prev_link (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_page_down (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_page_up (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_half_page_down (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_half_page_up (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_top (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_bottom (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_select_link (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_next_node (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_go_prev_node (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_help_cmd_quit (event_info_t * event_info, gpointer data, GError ** error);

/*** inline functions ****************************************************************************/

#endif /* MC__EVENTS_INIT_H */
