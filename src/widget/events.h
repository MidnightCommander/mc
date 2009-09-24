#ifndef W_WIDGET_EVENTS_H
#define W_WIDGET_EVENTS_H

#include "object.h"
#include "container.h"

/*** typedefs(not structures) and defined constants ********************/

/*** enums *************************************************************/

/*** structures declarations (and typedefs of structures)***************/

/*** global variables defined in .c file *******************************/

/*** declarations of public functions **********************************/


void w_event_set (w_object_t *, const char *, w_event_cb_t);
void w_event_remove (w_object_t *, const char *);
gboolean w_event_isset (w_object_t *, const char *);

gboolean w_event_raise (w_object_t *, const char *, gpointer);

void w_event_raise_to_parents (w_container_t *, const char *, gpointer);
gboolean w_event_raise_to_children (w_container_t *, const char *, gpointer);


#endif
