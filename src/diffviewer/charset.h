#ifndef MC__DIFFVIEW_CHARSET_H
#define MC__DIFFVIEW_CHARSET_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean mc_diffviewer_cmd_select_encoding_show_dialog (event_info_t * event_info, gpointer data,
                                                        GError ** error);

void mc_diffviewer_set_codeset (WDiff * dview);

/*** inline functions ****************************************************************************/

#endif /* MC__DIFFVIEW_CHARSET_H */
