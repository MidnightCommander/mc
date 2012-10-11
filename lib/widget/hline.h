
/** \file hline.h
 *  \brief Header: WHLine widget
 */

#ifndef MC__WIDGET_HLINE_H
#define MC__WIDGET_HLINE_H

/*** typedefs(not structures) and defined constants **********************************************/

#define HLINE(x) ((WHLine *)(x))

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    Widget widget;
    gboolean auto_adjust_cols;  /* Compute widget.cols from parent width? */
    gboolean transparent;       /* Paint in the default color fg/bg */
} WHLine;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WHLine *hline_new (int y, int x, int width);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_HLINE_H */
