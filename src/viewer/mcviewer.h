/** \file view.h
 *  \brief Header: internal file viewer
 */

#ifndef MC_VIEWER_H
#define MC_VIEWER_H

/*** typedefs(not structures) and defined constants ********************/

typedef struct WView WView;     /* Can be cast to Widget */

/*** enums *************************************************************/

/*** structures declarations (and typedefs of structures)***************/

/*** global variables defined in .c file *******************************/

/*** declarations of public functions **********************************/

/* Creates a new WView object with the given properties. Caveat: the
 * origin is in y-x order, while the extent is in x-y order. */
extern WView *view_new (int y, int x, int cols, int lines, int is_panel);


#endif
