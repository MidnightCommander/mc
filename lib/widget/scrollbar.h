
/** \file scrollbar.h
 *  \brief Header: WScrollBar widget
 */

#ifndef MC__WIDGET_SCROLLBAR_H
#define MC__WIDGET_SCROLLBAR_H

/*** typedefs(not structures) and defined constants **********************************************/

#define SCROLLBAR(x) ((WScrollBar *)(x))

/*** enums ***************************************************************************************/

typedef enum
{
    SCROLLBAR_VERTICAL,
    SCROLLBAR_HORISONTAL
} scrollbar_type_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    Widget widget;
    scrollbar_type_t type;
    int *total;
    int *current;
    int *first_displayed;
    Widget *parent;
} WScrollBar;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WScrollBar *scrollbar_new (Widget * parent, scrollbar_type_t type);
void scrollbar_set_total (WScrollBar * scrollbar, int *total);
void scrollbar_set_current (WScrollBar * scrollbar, int *current);
void scrollbar_set_first_displayed (WScrollBar * scrollbar, int *first_displayed);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_SCROLLBAR_H */
