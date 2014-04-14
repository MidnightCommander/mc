#ifndef MC__DIFFVIEW_OPTIONS_H
#define MC__DIFFVIEW_OPTIONS_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean mc_diffviewer_cmd_options_save (event_info_t * event_info, gpointer data, GError ** error);

gboolean mc_diffviewer_cmd_options_load (event_info_t * event_info, gpointer data, GError ** error);

gboolean mc_diffviewer_cmd_options_show_dialog (event_info_t * event_info, gpointer data,
                                                GError ** error);

/*** inline functions ****************************************************************************/

#endif /* MC__DIFFVIEW_OPTIONS_H */
