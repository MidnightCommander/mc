
/** \file history.h
 *  \brief Header: save, load and show history
 */

#ifndef MC__WIDGET_HISTORY_H
#define MC__WIDGET_HISTORY_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

GList *history_get (const char *input_name);
void history_put (const char *input_name, GList * h);
/* for repositioning of history dialog we should pass widget to this
 * function, as position of history dialog depends on widget's position */
char *history_show (GList ** history, Widget * widget);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_HISTORY_H */
