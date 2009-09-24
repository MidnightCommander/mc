#ifndef W_WIDGET_DESKTOP_H
#define W_WIDGET_DESKTOP_H

#include "widget.h"

/*** typedefs(not structures) and defined constants ********************/

/*** enums *************************************************************/

/*** structures declarations (and typedefs of structures)***************/

typedef struct w_desktop_struct {
    w_widget_t widget;
} w_desktop_t;


/*** global variables defined in .c file *******************************/

/*** declarations of public functions **********************************/

w_desktop_t *w_desktop_new (const char *);
void w_desktop_free (w_desktop_t *);

#endif
