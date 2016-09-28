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

    unsigned long widget_id;    /* maximum id of all widgets */
    gboolean winch_pending;     /* SIGWINCH signal has been got. Resize group after rise */
};

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

unsigned long group_add_widget_autopos (WGroup * g, void *w, widget_pos_flags_t pos_flags,
                                        const void *before);
void group_remove_widget (void *w);

void group_set_current_widget_next (WGroup * g);
void group_set_current_widget_prev (WGroup * g);

GList *group_get_widget_next_of (GList * w);
GList *group_get_widget_prev_of (GList * w);

void group_select_next_widget (WGroup * g);
void group_select_prev_widget (WGroup * g);

void group_select_widget_by_id (const WGroup * g, unsigned long id);

void group_send_broadcast_msg (WGroup * g, widget_msg_t message);

/* --------------------------------------------------------------------------------------------- */
/*** inline functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Add widget to group before current widget.
 *
 * @param g WGroup object
 * @param w widget to be added
 *
 * @return widget ID
 */

static inline unsigned long
group_add_widget (WGroup * g, void *w)
{
    return group_add_widget_autopos (g, w, WPOS_KEEP_DEFAULT,
                                     g->current != NULL ? g->current->data : NULL);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Add widget to group before specified widget.
 *
 * @param g WGroup object
 * @param w widget to be added
 * @param before add @w before this widget
 *
 * @return widget ID
 */

static inline unsigned long
group_add_widget_before (WGroup * g, void *w, void *before)
{
    return group_add_widget_autopos (g, w, WPOS_KEEP_DEFAULT, before);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Select current widget in the Dialog.
 *
 * @param h WDialog object
 */

static inline void
group_select_current_widget (WGroup * g)
{
    if (g->current != NULL)
        widget_select (WIDGET (g->current->data));
}

/* --------------------------------------------------------------------------------------------- */

#endif /* MC__GROUP_H */
