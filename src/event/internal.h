#ifndef MC_EVENT_INTERNAL_H
#define MC_EVENT_INTERNAL_H

#include "event.h"

/*** typedefs(not structures) and defined constants ********************/

/*** enums *************************************************************/

/*** structures declarations (and typedefs of structures)***************/

typedef struct mc_event_struct {
    gchar *name;
    GSList *callbacks;
} mc_event_t;


typedef struct mc_event_callback_struct {
    gpointer init_data;
    mc_event_callback_func_t callback;
} mc_event_callback_t;


/*** global variables defined in .c file *******************************/

extern GHashTable *mc_event_hashlist;

/*** declarations of public functions **********************************/

#endif
