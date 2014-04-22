#ifndef MC__EVENT_H
#define MC__EVENT_H

#include "event-types.h"

/*** typedefs(not structures) and defined constants **********************************************/

struct event_info_t;
typedef gboolean (*mc_event_callback_func_t) (struct event_info_t *, gpointer, GError **);

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef union
{
    gboolean b;
    int i;
    char *s;
    void *p;
} event_return_t;

typedef struct event_info_t
{
    const char *group_name;
    const char *name;
    gpointer init_data;
    event_return_t *ret;
} event_info_t;

typedef struct
{
    const char *name;
    mc_event_callback_func_t cb;
    gpointer init_data;
} event_init_group_t;

typedef struct
{
    const char *group_name;
    event_init_group_t *events;
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
gboolean mc_event_mass_add (event_init_t *, GError **);

/* raise.c: */
gboolean
mc_event_raise (const gchar * event_group_name, const gchar * event_name, gpointer event_data,
                event_return_t * ret, GError ** error);
/*** inline functions ****************************************************************************/
#endif /* MC__EVENT_H */
