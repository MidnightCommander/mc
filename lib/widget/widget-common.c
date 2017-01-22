/*
   Widgets for the Midnight Commander

   Copyright (C) 1994-2017
   Free Software Foundation, Inc.

   Authors:
   Radek Doulik, 1994, 1995
   Miguel de Icaza, 1994, 1995
   Jakub Jelinek, 1995
   Andrej Borsenkow, 1996
   Norbert Warmuth, 1997
   Andrew Borodin <aborodin@vmail.ru>, 2009, 2010, 2011, 2012, 2013

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

/** \file widget-common.c
 *  \brief Source: shared stuff of widgets
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/color.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/widget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
widget_do_focus (Widget * w, gboolean enable)
{
    if (w != NULL && widget_get_state (WIDGET (w->owner), WST_FOCUSED))
        widget_set_state (w, WST_FOCUSED, enable);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Focus specified widget in it's owner.
 *
 * @param w widget to be focused.
 */

static void
widget_focus (Widget * w)
{
    WDialog *h = DIALOG (w->owner);

    if (h == NULL)
        return;

    if (WIDGET (h->current->data) != w)
    {
        widget_do_focus (WIDGET (h->current->data), FALSE);
        /* Test if focus lost was allowed and focus has really been loose */
        if (h->current == NULL || !widget_get_state (WIDGET (h->current->data), WST_FOCUSED))
        {
            widget_do_focus (w, TRUE);
            h->current = dlg_find (h, w);
        }
    }
    else if (!widget_get_state (w, WST_FOCUSED))
        widget_do_focus (w, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Put widget on top or bottom of Z-order.
 */
static void
widget_reorder (GList * l, gboolean set_top)
{
    WDialog *h = WIDGET (l->data)->owner;

    h->widgets = g_list_remove_link (h->widgets, l);
    if (set_top)
        h->widgets = g_list_concat (h->widgets, l);
    else
        h->widgets = g_list_concat (l, h->widgets);
}


/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

struct hotkey_t
parse_hotkey (const char *text)
{
    hotkey_t result;
    const char *cp, *p;

    if (text == NULL)
        text = "";

    /* search for '&', that is not on the of text */
    cp = strchr (text, '&');
    if (cp != NULL && cp[1] != '\0')
    {
        result.start = g_strndup (text, cp - text);

        /* skip '&' */
        cp++;
        p = str_cget_next_char (cp);
        result.hotkey = g_strndup (cp, p - cp);

        cp = p;
        result.end = g_strdup (cp);
    }
    else
    {
        result.start = g_strdup (text);
        result.hotkey = NULL;
        result.end = NULL;
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */

void
release_hotkey (const hotkey_t hotkey)
{
    g_free (hotkey.start);
    g_free (hotkey.hotkey);
    g_free (hotkey.end);
}

/* --------------------------------------------------------------------------------------------- */

int
hotkey_width (const hotkey_t hotkey)
{
    int result;

    result = str_term_width1 (hotkey.start);
    result += (hotkey.hotkey != NULL) ? str_term_width1 (hotkey.hotkey) : 0;
    result += (hotkey.end != NULL) ? str_term_width1 (hotkey.end) : 0;
    return result;
}

/* --------------------------------------------------------------------------------------------- */

void
hotkey_draw (Widget * w, const hotkey_t hotkey, gboolean focused)
{
    widget_selectcolor (w, focused, FALSE);
    tty_print_string (hotkey.start);

    if (hotkey.hotkey != NULL)
    {
        widget_selectcolor (w, focused, TRUE);
        tty_print_string (hotkey.hotkey);
        widget_selectcolor (w, focused, FALSE);
    }

    if (hotkey.end != NULL)
        tty_print_string (hotkey.end);
}

/* --------------------------------------------------------------------------------------------- */

void
widget_init (Widget * w, int y, int x, int lines, int cols,
             widget_cb_fn callback, widget_mouse_cb_fn mouse_callback)
{
    w->x = x;
    w->y = y;
    w->cols = cols;
    w->lines = lines;
    w->pos_flags = WPOS_KEEP_DEFAULT;
    w->callback = callback;
    w->mouse_callback = mouse_callback;
    w->owner = NULL;
    w->mouse.forced_capture = FALSE;
    w->mouse.capture = FALSE;
    w->mouse.last_msg = MSG_MOUSE_NONE;
    w->mouse.last_buttons_down = 0;

    w->options = WOP_DEFAULT;
    w->state = WST_DEFAULT;
}

/* --------------------------------------------------------------------------------------------- */

/* Default callback for widgets */
cb_ret_t
widget_default_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    (void) w;
    (void) sender;
    (void) parm;
    (void) data;

    switch (msg)
    {
    case MSG_INIT:
    case MSG_FOCUS:
    case MSG_UNFOCUS:
    case MSG_ENABLE:
    case MSG_DISABLE:
    case MSG_DRAW:
    case MSG_DESTROY:
    case MSG_CURSOR:
    case MSG_IDLE:
        return MSG_HANDLED;

    default:
        return MSG_NOT_HANDLED;
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Apply new options to widget.
 *
 * @param w       widget
 * @param options widget option flags to modify. Several flags per call can be modified.
 * @param enable  TRUE if specified options should be added, FALSE if options should be removed
 */
void
widget_set_options (Widget * w, widget_options_t options, gboolean enable)
{
    if (enable)
        w->options |= options;
    else
        w->options &= ~options;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Modify state of widget.
 *
 * @param w      widget
 * @param state  widget state flag to modify
 * @param enable specifies whether to turn the flag on (TRUE) or off (FALSE).
 *               Only one flag per call can be modified.
 * @return       MSG_HANDLED if set was handled successfully, MSG_NOT_HANDLED otherwise.
 */
cb_ret_t
widget_set_state (Widget * w, widget_state_t state, gboolean enable)
{
    gboolean ret = MSG_HANDLED;

    if (enable)
        w->state |= state;
    else
        w->state &= ~state;

    if (enable)
    {
        /* exclusive bits */
        if ((state & WST_CONSTRUCT) != 0)
            w->state &= ~(WST_ACTIVE | WST_SUSPENDED | WST_CLOSED);
        else if ((state & WST_ACTIVE) != 0)
            w->state &= ~(WST_CONSTRUCT | WST_SUSPENDED | WST_CLOSED);
        else if ((state & WST_SUSPENDED) != 0)
            w->state &= ~(WST_CONSTRUCT | WST_ACTIVE | WST_CLOSED);
        else if ((state & WST_CLOSED) != 0)
            w->state &= ~(WST_CONSTRUCT | WST_ACTIVE | WST_SUSPENDED);
    }

    if (w->owner == NULL)
        return MSG_NOT_HANDLED;

    switch (state)
    {
    case WST_DISABLED:
        ret = send_message (w, NULL, enable ? MSG_DISABLE : MSG_ENABLE, 0, NULL);
        if (ret == MSG_HANDLED && widget_get_state (WIDGET (w->owner), WST_ACTIVE))
            ret = send_message (w, NULL, MSG_DRAW, 0, NULL);
        break;

    case WST_FOCUSED:
        {
            widget_msg_t msg;

            msg = enable ? MSG_FOCUS : MSG_UNFOCUS;
            ret = send_message (w, NULL, msg, 0, NULL);
            if (ret == MSG_HANDLED && widget_get_state (WIDGET (w->owner), WST_ACTIVE))
            {
                send_message (w, NULL, MSG_DRAW, 0, NULL);
                /* Notify owner that focus was moved from one widget to another */
                send_message (w->owner, w, MSG_CHANGED_FOCUS, 0, NULL);
            }
        }
        break;

    default:
        break;
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

void
widget_set_size (Widget * widget, int y, int x, int lines, int cols)
{
    widget->x = x;
    widget->y = y;
    widget->cols = cols;
    widget->lines = lines;
    send_message (widget, NULL, MSG_RESIZE, 0, NULL);
    if (widget->owner != NULL && widget_get_state (WIDGET (widget->owner), WST_ACTIVE))
        send_message (widget, NULL, MSG_DRAW, 0, NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
widget_selectcolor (Widget * w, gboolean focused, gboolean hotkey)
{
    WDialog *h = w->owner;
    int color;

    if (widget_get_state (w, WST_DISABLED))
        color = DISABLED_COLOR;
    else if (hotkey)
    {
        if (focused)
            color = h->color[DLG_COLOR_HOT_FOCUS];
        else
            color = h->color[DLG_COLOR_HOT_NORMAL];
    }
    else
    {
        if (focused)
            color = h->color[DLG_COLOR_FOCUS];
        else
            color = h->color[DLG_COLOR_NORMAL];
    }

    tty_setcolor (color);
}

/* --------------------------------------------------------------------------------------------- */

void
widget_erase (Widget * w)
{
    if (w != NULL)
        tty_fill_region (w->y, w->x, w->lines, w->cols, ' ');
}

/* --------------------------------------------------------------------------------------------- */
/**
  * Check whether widget is active or not.
  * @param w the widget
  *
  * @return TRUE if the widget is active, FALSE otherwise
  */

gboolean
widget_is_active (const void *w)
{
    return (w == CONST_WIDGET (w)->owner->current->data);
}

/* --------------------------------------------------------------------------------------------- */

void
widget_redraw (Widget * w)
{
    if (w != NULL)
    {
        WDialog *h = w->owner;

        if (h != NULL && widget_get_state (WIDGET (h), WST_ACTIVE))
            w->callback (w, NULL, MSG_DRAW, 0, NULL);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
  * Replace widget in the dialog.
  *
  * @param old_w old widget that need to be replaced
  * @param new_w new widget that will replace @old_w
  */

void
widget_replace (Widget * old_w, Widget * new_w)
{
    WDialog *h = old_w->owner;
    gboolean should_focus = FALSE;
    GList *holder;

    if (h->widgets == NULL)
        return;

    if (h->current == NULL)
        h->current = h->widgets;

    /* locate widget position in the list */
    if (old_w == h->current->data)
        holder = h->current;
    else
        holder = g_list_find (h->widgets, old_w);

    /* if old widget is focused, we should focus the new one... */
    if (widget_get_state (old_w, WST_FOCUSED))
        should_focus = TRUE;
    /* ...but if new widget isn't selectable, we cannot focus it */
    if (!widget_get_options (new_w, WOP_SELECTABLE))
        should_focus = FALSE;

    /* if new widget isn't selectable, select other widget before replace */
    if (!should_focus)
    {
        GList *l;

        for (l = dlg_get_widget_next_of (holder);
             !widget_get_options (WIDGET (l->data), WOP_SELECTABLE)
             && !widget_get_state (WIDGET (l->data), WST_DISABLED); l = dlg_get_widget_next_of (l))
            ;

        widget_select (WIDGET (l->data));
    }

    /* replace widget */
    new_w->owner = h;
    new_w->id = old_w->id;
    holder->data = new_w;

    send_message (old_w, NULL, MSG_DESTROY, 0, NULL);
    send_message (new_w, NULL, MSG_INIT, 0, NULL);

    if (should_focus)
        widget_select (new_w);
    else
        widget_redraw (new_w);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Select specified widget in it's owner.
 *
 * @param w widget to be selected
 */

void
widget_select (Widget * w)
{
    WDialog *h;

    if (!widget_get_options (w, WOP_SELECTABLE))
        return;

    h = w->owner;
    if (h != NULL)
    {
        if (widget_get_options (w, WOP_TOP_SELECT))
        {
            GList *l;

            l = dlg_find (h, w);
            widget_reorder (l, TRUE);
        }

        widget_focus (w);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Set widget at bottom of widget list.
 */

void
widget_set_bottom (Widget * w)
{
    widget_reorder (dlg_find (w->owner, w), FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/**
  * Check whether two widgets are overlapped or not.
  * @param a 1st widget
  * @param b 2nd widget
  *
  * @return TRUE if widgets are overlapped, FALSE otherwise.
  */

gboolean
widget_overlapped (const Widget * a, const Widget * b)
{
    return !((b->x >= a->x + a->cols)
             || (a->x >= b->x + b->cols) || (b->y >= a->y + a->lines) || (a->y >= b->y + b->lines));
}

/* --------------------------------------------------------------------------------------------- */
/* get mouse pointer location within widget */

Gpm_Event
mouse_get_local (const Gpm_Event * global, const Widget * w)
{
    Gpm_Event local;

    local.buttons = global->buttons;
#ifdef HAVE_LIBGPM
    local.clicks = 0;
    local.margin = 0;
    local.modifiers = 0;
    local.vc = 0;
#endif
    local.x = global->x - w->x;
    local.y = global->y - w->y;
    local.type = global->type;

    return local;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mouse_global_in_widget (const Gpm_Event * event, const Widget * w)
{
    return (event->x > w->x) && (event->y > w->y) && (event->x <= w->x + w->cols)
        && (event->y <= w->y + w->lines);
}

/* --------------------------------------------------------------------------------------------- */
