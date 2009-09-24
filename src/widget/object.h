#ifndef W_WIDGET_OBJECT_H
#define W_WIDGET_OBJECT_H

#include <config.h>
#include "../src/global.h"

/*** typedefs(not structures) and defined constants ********************/

/*** enums *************************************************************/

/*** structures declarations (and typedefs of structures)***************/

struct w_object_struct;

typedef gboolean (*w_event_cb_t) (struct w_object_struct *, const char *, gpointer);

typedef struct w_object_struct {
    GHashTable *events;

} w_object_t;

/*** global variables defined in .c file *******************************/

/*** declarations of public functions **********************************/

void w_object_init (w_object_t *);
void w_object_deinit (w_object_t *);

#endif
