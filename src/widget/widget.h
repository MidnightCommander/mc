#ifndef W_WIDGET_H
#define W_WIDGET_H

#include "container.h"

/*** typedefs(not structures) and defined constants ********************/

/*** enums *************************************************************/

/*** structures declarations (and typedefs of structures)***************/

typedef struct w_widget_struct {
    w_container_t tree;
    char *name;

    int left;
    int top;
    int width;
    int height;

    /* Todo: is this good place for these variables? */
    int color_pair;
    GHashTable *keybindings;

} w_widget_t;


/*** global variables defined in .c file *******************************/

/*** declarations of public functions **********************************/

void w_widget_init (w_widget_t *, const char *);
void w_widget_deinit (w_widget_t *);

w_widget_t *w_widget_find_by_name (w_widget_t *, const char *);

#endif
