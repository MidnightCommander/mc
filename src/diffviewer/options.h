#ifndef MC__DIFFVIEW_OPTIONS_H
#define MC__DIFFVIEW_OPTIONS_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean mc_diffviewer_cmd_options_save (const gchar * event_group_name, const gchar * event_name,
                                         gpointer init_data, gpointer data);

gboolean mc_diffviewer_cmd_options_load (const gchar * event_group_name, const gchar * event_name,
                                         gpointer init_data, gpointer data);

gboolean mc_diffviewer_cmd_options_show_dialog (const gchar * event_group_name,
                                                const gchar * event_name, gpointer init_data,
                                                gpointer data);

/*** inline functions ****************************************************************************/

#endif /* MC__DIFFVIEW_OPTIONS_H */
