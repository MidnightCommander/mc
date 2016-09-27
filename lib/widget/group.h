/*
 *  Widget group features module for Midnight Commander
 */

/** \file group.h
 *  \brief Header: widget group features module
 */

#ifndef MC__GROUP_H
#define MC__GROUP_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define GROUP(x) ((WGroup *)(x))
#define CONST_GROUP(x) ((const WGroup *)(x))

/*** enums ***************************************************************************************/

/*** typedefs(not structures) ********************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct WGroup
{
    Widget widget;

    /* Group members */
    GList *widgets;             /* widgets list */
    GList *current;             /* Currently active widget */

    gboolean winch_pending;     /* SIGWINCH signal has been got. Resize group after rise */
};

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void group_set_current_widget_next (WGroup * g);
void group_set_current_widget_prev (WGroup * g);

GList *group_get_widget_next_of (GList * w);
GList *group_get_widget_prev_of (GList * w);

void group_select_next_widget (WGroup * g);
void group_select_prev_widget (WGroup * g);

void group_select_widget_by_id (const WGroup * g, unsigned long id);

/* --------------------------------------------------------------------------------------------- */
/*** inline functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Select current widget in the group.
 *
 * @param g WGroup object
 */

static inline void
group_select_current_widget (WGroup * g)
{
    if (g->current != NULL)
        widget_select (WIDGET (g->current->data));
}

/* --------------------------------------------------------------------------------------------- */

#endif /* MC__GROUP_H */
