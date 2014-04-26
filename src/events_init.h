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

/*** inline functions ****************************************************************************/

#endif /* MC__EVENTS_INIT_H */
