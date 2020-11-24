#ifndef MC__EVENT_H
#define MC__EVENT_H

#include "event-types.h"

/*** typedefs(not structures) and defined constants **********************************************/

typedef gboolean (*mc_event_callback_func_t) (const gchar *, const gchar *, gpointer, gpointer);

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    const char *event_group_name;
    const char *event_name;
    mc_event_callback_func_t cb;
    gpointer init_data;
} event_init_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* event.c: */
gboolean mc_event_init (GError **);
gboolean mc_event_deinit (GError **);


/* manage.c: */
gboolean mc_event_add (const gchar *, const gchar *, mc_event_callback_func_t, gpointer, GError **);
void mc_event_del (const gchar *, const gchar *, mc_event_callback_func_t, gpointer);
void mc_event_destroy (const gchar *, const gchar *);
void mc_event_group_del (const gchar *);
gboolean mc_event_present (const gchar *, const gchar *);
gboolean mc_event_mass_add (const event_init_t *, GError **);

/* raise.c: */
gboolean mc_event_raise (const gchar *, const gchar *, gpointer);


/*** inline functions ****************************************************************************/

#endif /* MC__EVENT_H */
