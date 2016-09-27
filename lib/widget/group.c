/*
   Widget group features module for the Midnight Commander

   Copyright (C) 2020
   The Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2020

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file group.c
 *  \brief Source: widget group features module
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"

#include "lib/widget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static GList *
group_get_next_or_prev_of (GList * list, gboolean next)
{
    GList *l = NULL;

    if (list != NULL)
    {
        WGroup *owner = WIDGET (list->data)->owner;

        if (owner != NULL)
        {
            if (next)
            {
                l = g_list_next (list);
                if (l == NULL)
                    l = owner->widgets;
            }
            else
            {
                l = g_list_previous (list);
                if (l == NULL)
                    l = g_list_last (owner->widgets);
            }
        }
    }

    return l;
}

/* --------------------------------------------------------------------------------------------- */

static void
group_select_next_or_prev (WGroup * g, gboolean next)
{
    if (g->widgets != NULL && g->current != NULL)
    {
        GList *l = g->current;
        Widget *w;

        do
        {
            l = group_get_next_or_prev_of (l, next);
            w = WIDGET (l->data);
        }
        while ((widget_get_state (w, WST_DISABLED) || !widget_get_options (w, WOP_SELECTABLE))
               && l != g->current);

        widget_select (l->data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Switch current widget to widget after current in group.
 *
 * @param g WGroup object
 */

void
group_set_current_widget_next (WGroup * g)
{
    g->current = group_get_next_or_prev_of (g->current, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Switch current widget to widget before current in group.
 *
 * @param g WGroup object
 */

void
group_set_current_widget_prev (WGroup * g)
{
    g->current = group_get_next_or_prev_of (g->current, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get widget that is after specified widget in group.
 *
 * @param w widget holder
 *
 * @return widget that is after "w" or NULL if "w" is NULL or widget doesn't have owner
 */

GList *
group_get_widget_next_of (GList * w)
{
    return group_get_next_or_prev_of (w, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get widget that is before specified widget in group.
 *
 * @param w widget holder
 *
 * @return widget that is before "w" or NULL if "w" is NULL or widget doesn't have owner
 */

GList *
group_get_widget_prev_of (GList * w)
{
    return group_get_next_or_prev_of (w, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Try to select next widget in the Z order.
 *
 * @param g WGroup object
 */

void
group_select_next_widget (WGroup * g)
{
    group_select_next_or_prev (g, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Try to select previous widget in the Z order.
 *
 * @param g WGroup object
 */

void
group_select_prev_widget (WGroup * g)
{
    group_select_next_or_prev (g, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Find the widget with the specified ID in the group and select it
 *
 * @param g WGroup object
 * @param id widget ID
 */

void
group_select_widget_by_id (const WGroup * g, unsigned long id)
{
    Widget *w;

    w = dlg_find_by_id (DIALOG (g), id);
    if (w != NULL)
        widget_select (w);
}

/* --------------------------------------------------------------------------------------------- */
