#ifndef W_WIDGET_CONTAINER_H
#define W_WIDGET_CONTAINER_H

#include "object.h"

/*** typedefs(not structures) and defined constants ********************/

/*** enums *************************************************************/

/*** structures declarations (and typedefs of structures)***************/

typedef struct w_container_struct {
    w_object_t object;

    struct w_container_struct *parent;
    struct w_container_struct *children;        /* pointer to first child */
    struct w_container_struct *prev;
    struct w_container_struct *next;
} w_container_t;

/*** global variables defined in .c file *******************************/

/*** declarations of public functions **********************************/

void w_container_init (w_container_t *);
void w_container_deinit (w_container_t *);

GPtrArray *w_container_get_path (w_container_t *);

void w_container_append_child (w_container_t *, w_container_t *);
void w_container_remove_from_siblings (w_container_t *);
w_container_t *w_container_get_last_child (w_container_t *);


#endif
