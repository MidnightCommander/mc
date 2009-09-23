#ifndef MC__EVENT_H
#define MC__EVENT_H

#include <config.h>

#include "../src/global.h"      /* <glib.h> */


/*** typedefs(not structures) and defined constants **********************************************/

typedef gboolean (*mc_event_callback_func_t) (const gchar *, gpointer, gpointer);

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* event.c: */
gboolean mc_event_init (void);
void mc_event_deinit (void);


/* manage.c: */
gboolean mc_event_add (const gchar *, mc_event_callback_func_t, gpointer);
gboolean mc_event_del (const gchar *, mc_event_callback_func_t);
gboolean mc_event_destroy (const gchar *);

/* raise.c: */
gboolean mc_event_raise (const gchar *, gpointer);

#endif
