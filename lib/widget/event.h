#ifndef MC__WIDGET_EVENT_H
#define MC__WIDGET_EVENT_H

#include "lib/event.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void mc_widget_init_events (GError ** error);

gboolean mc_widget_dialog_show_dialog_list (event_info_t * event_info, gpointer data,
                                            GError ** error);


gboolean mc_widget_input_show_history (event_info_t * event_info, gpointer data, GError ** error);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_EVENT_H */
