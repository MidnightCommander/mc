/*
   Widgets for the Midnight Commander

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009, 2010, 2011
   The Free Software Foundation, Inc.

   Authors:
   Radek Doulik, 1994, 1995
   Miguel de Icaza, 1994, 1995
   Jakub Jelinek, 1995
   Andrej Borsenkow, 1996
   Norbert Warmuth, 1997
   Andrew Borodin <aborodin@vmail.ru>, 2009, 2010

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

/** \file listbox.c
 *  \brief Source: WListbox widget
 */

#include <config.h>

#include <stdlib.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/mouse.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/util.h"           /* Q_() */
#include "lib/keybind.h"        /* global_keymap_t */
#include "lib/widget.h"

/*** global variables ****************************************************************************/

const global_keymap_t *listbox_map = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static int
listbox_entry_cmp (const void *a, const void *b)
{
    const WLEntry *ea = (const WLEntry *) a;
    const WLEntry *eb = (const WLEntry *) b;

    return strcmp (ea->text, eb->text);
}

/* --------------------------------------------------------------------------------------------- */

static void
listbox_entry_free (void *data)
{
    WLEntry *e = data;
    g_free (e->text);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

static void
listbox_drawscroll (WListbox * l)
{
    const int max_line = l->widget.lines - 1;
    int line = 0;
    int i;

    /* Are we at the top? */
    widget_move (&l->widget, 0, l->widget.cols);
    if (l->top == 0)
        tty_print_one_vline (TRUE);
    else
        tty_print_char ('^');

    /* Are we at the bottom? */
    widget_move (&l->widget, max_line, l->widget.cols);
    if ((l->top + l->widget.lines == l->count) || (l->widget.lines >= l->count))
        tty_print_one_vline (TRUE);
    else
        tty_print_char ('v');

    /* Now draw the nice relative pointer */
    if (l->count != 0)
        line = 1 + ((l->pos * (l->widget.lines - 2)) / l->count);

    for (i = 1; i < max_line; i++)
    {
        widget_move (&l->widget, i, l->widget.cols);
        if (i != line)
            tty_print_one_vline (TRUE);
        else
            tty_print_char ('*');
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
listbox_draw (WListbox * l, gboolean focused)
{
    const Dlg_head *h = l->widget.owner;
    const gboolean disabled = (((Widget *) l)->options & W_DISABLED) != 0;
    const int normalc = disabled ? DISABLED_COLOR : h->color[DLG_COLOR_NORMAL];
    int selc =
        disabled ? DISABLED_COLOR : focused ? h->
        color[DLG_COLOR_HOT_FOCUS] : h->color[DLG_COLOR_FOCUS];

    GList *le;
    int pos;
    int i;
    int sel_line = -1;

    le = g_list_nth (l->list, l->top);
    /*    pos = (le == NULL) ? 0 : g_list_position (l->list, le); */
    pos = (le == NULL) ? 0 : l->top;

    for (i = 0; i < l->widget.lines; i++)
    {
        const char *text;

        /* Display the entry */
        if (pos == l->pos && sel_line == -1)
        {
            sel_line = i;
            tty_setcolor (selc);
        }
        else
            tty_setcolor (normalc);

        widget_move (&l->widget, i, 1);

        if ((i > 0 && pos >= l->count) || (l->list == NULL) || (le == NULL))
            text = "";
        else
        {
            WLEntry *e = (WLEntry *) le->data;
            text = e->text;
            le = g_list_next (le);
            pos++;
        }

        tty_print_string (str_fit_to_term (text, l->widget.cols - 2, J_LEFT_FIT));
    }

    l->cursor_y = sel_line;

    if (l->scrollbar && (l->count > l->widget.lines))
    {
        tty_setcolor (normalc);
        listbox_drawscroll (l);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
listbox_check_hotkey (WListbox * l, int key)
{
    int i;
    GList *le;

    for (i = 0, le = l->list; le != NULL; i++, le = g_list_next (le))
    {
        WLEntry *e = (WLEntry *) le->data;

        if (e->hotkey == key)
            return i;
    }

    return (-1);
}

/* --------------------------------------------------------------------------------------------- */

/* Selects from base the pos element */
static int
listbox_select_pos (WListbox * l, int base, int pos)
{
    int last = l->count - 1;

    base += pos;
    base = min (base, last);

    return base;
}

/* --------------------------------------------------------------------------------------------- */

static void
listbox_fwd (WListbox * l)
{
    if (l->pos + 1 >= l->count)
        listbox_select_first (l);
    else
        listbox_select_entry (l, l->pos + 1);
}

/* --------------------------------------------------------------------------------------------- */

static void
listbox_back (WListbox * l)
{
    if (l->pos <= 0)
        listbox_select_last (l);
    else
        listbox_select_entry (l, l->pos - 1);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
listbox_execute_cmd (WListbox * l, unsigned long command)
{
    cb_ret_t ret = MSG_HANDLED;
    int i;

    switch (command)
    {
    case CK_Up:
        listbox_back (l);
        break;
    case CK_Down:
        listbox_fwd (l);
        break;
    case CK_Top:
        listbox_select_first (l);
        break;
    case CK_Bottom:
        listbox_select_last (l);
        break;
    case CK_PageUp:
        for (i = 0; (i < l->widget.lines - 1) && (l->pos > 0); i++)
            listbox_back (l);
        break;
    case CK_PageDown:
        for (i = 0; (i < l->widget.lines - 1) && (l->pos < l->count - 1); i++)
            listbox_fwd (l);
        break;
    case CK_Delete:
        if (l->deletable)
        {
            gboolean is_last = (l->pos + 1 >= l->count);
            gboolean is_more = (l->top + l->widget.lines >= l->count);

            listbox_remove_current (l);
            if ((l->top > 0) && (is_last || is_more))
                l->top--;
        }
        break;
    case CK_Clear:
        if (l->deletable && mc_global.widget.confirm_history_cleanup
            /* TRANSLATORS: no need to translate 'DialogTitle', it's just a context prefix */
            && (query_dialog (Q_ ("DialogTitle|History cleanup"),
                              _("Do you want clean this history?"),
                              D_ERROR, 2, _("&Yes"), _("&No")) == 0))
            listbox_remove_list (l);
        break;
    default:
        ret = MSG_NOT_HANDLED;
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

/* Return MSG_HANDLED if we want a redraw */
static cb_ret_t
listbox_key (WListbox * l, int key)
{
    unsigned long command;

    if (l->list == NULL)
        return MSG_NOT_HANDLED;

    /* focus on listbox item N by '0'..'9' keys */
    if (key >= '0' && key <= '9')
    {
        int oldpos = l->pos;
        listbox_select_entry (l, key - '0');

        /* need scroll to item? */
        if (abs (oldpos - l->pos) > l->widget.lines)
            l->top = l->pos;

        return MSG_HANDLED;
    }

    command = keybind_lookup_keymap_command (listbox_map, key);
    if (command == CK_IgnoreKey)
        return MSG_NOT_HANDLED;
    return listbox_execute_cmd (l, command);
}

/* --------------------------------------------------------------------------------------------- */

/* Listbox item adding function */
static inline void
listbox_append_item (WListbox * l, WLEntry * e, listbox_append_t pos)
{
    switch (pos)
    {
    case LISTBOX_APPEND_AT_END:
        l->list = g_list_append (l->list, e);
        break;

    case LISTBOX_APPEND_BEFORE:
        l->list = g_list_insert_before (l->list, g_list_nth (l->list, l->pos), e);
        if (l->pos > 0)
            l->pos--;
        break;

    case LISTBOX_APPEND_AFTER:
        l->list = g_list_insert (l->list, e, l->pos + 1);
        break;

    case LISTBOX_APPEND_SORTED:
        l->list = g_list_insert_sorted (l->list, e, (GCompareFunc) listbox_entry_cmp);
        break;

    default:
        return;
    }

    l->count++;
}

/* --------------------------------------------------------------------------------------------- */

static inline void
listbox_destroy (WListbox * l)
{
    listbox_remove_list (l);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
listbox_callback (Widget * w, widget_msg_t msg, int parm)
{
    WListbox *l = (WListbox *) w;
    Dlg_head *h = l->widget.owner;
    cb_ret_t ret_code;

    switch (msg)
    {
    case WIDGET_INIT:
        return MSG_HANDLED;

    case WIDGET_HOTKEY:
        {
            int pos, action;

            pos = listbox_check_hotkey (l, parm);
            if (pos < 0)
                return MSG_NOT_HANDLED;

            listbox_select_entry (l, pos);
            h->callback (h, w, DLG_ACTION, l->pos, NULL);

            if (l->callback != NULL)
                action = l->callback (l);
            else
                action = LISTBOX_DONE;

            if (action == LISTBOX_DONE)
            {
                h->ret_value = B_ENTER;
                dlg_stop (h);
            }

            return MSG_HANDLED;
        }

    case WIDGET_KEY:
        ret_code = listbox_key (l, parm);
        if (ret_code != MSG_NOT_HANDLED)
        {
            listbox_draw (l, TRUE);
            h->callback (h, w, DLG_ACTION, l->pos, NULL);
        }
        return ret_code;

    case WIDGET_COMMAND:
        return listbox_execute_cmd (l, parm);

    case WIDGET_CURSOR:
        widget_move (&l->widget, l->cursor_y, 0);
        h->callback (h, w, DLG_ACTION, l->pos, NULL);
        return MSG_HANDLED;

    case WIDGET_FOCUS:
    case WIDGET_UNFOCUS:
    case WIDGET_DRAW:
        listbox_draw (l, msg != WIDGET_UNFOCUS);
        return MSG_HANDLED;

    case WIDGET_DESTROY:
        listbox_destroy (l);
        return MSG_HANDLED;

    case WIDGET_RESIZED:
        return MSG_HANDLED;

    default:
        return default_proc (msg, parm);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
listbox_event (Gpm_Event * event, void *data)
{
    WListbox *l = data;
    int i;

    Dlg_head *h = l->widget.owner;

    /* Single click */
    if (event->type & GPM_DOWN)
        dlg_select_widget (l);

    if (l->list == NULL)
        return MOU_NORMAL;

    if (event->type & (GPM_DOWN | GPM_DRAG))
    {
        int ret = MOU_REPEAT;

        if (event->x < 0 || event->x > l->widget.cols)
            return ret;

        if (event->y < 1)
            for (i = -event->y; i >= 0; i--)
                listbox_back (l);
        else if (event->y > l->widget.lines)
            for (i = event->y - l->widget.lines; i > 0; i--)
                listbox_fwd (l);
        else if (event->buttons & GPM_B_UP)
        {
            listbox_back (l);
            ret = MOU_NORMAL;
        }
        else if (event->buttons & GPM_B_DOWN)
        {
            listbox_fwd (l);
            ret = MOU_NORMAL;
        }
        else
            listbox_select_entry (l, listbox_select_pos (l, l->top, event->y - 1));

        /* We need to refresh ourselves since the dialog manager doesn't */
        /* know about this event */
        listbox_draw (l, TRUE);
        return ret;
    }

    /* Double click */
    if ((event->type & (GPM_DOUBLE | GPM_UP)) == (GPM_UP | GPM_DOUBLE))
    {
        int action;

        if (event->x < 0 || event->x >= l->widget.cols
            || event->y < 1 || event->y > l->widget.lines)
            return MOU_NORMAL;

        dlg_select_widget (l);
        listbox_select_entry (l, listbox_select_pos (l, l->top, event->y - 1));

        if (l->callback != NULL)
            action = l->callback (l);
        else
            action = LISTBOX_DONE;

        if (action == LISTBOX_DONE)
        {
            h->ret_value = B_ENTER;
            dlg_stop (h);
            return MOU_NORMAL;
        }
    }
    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WListbox *
listbox_new (int y, int x, int height, int width, gboolean deletable, lcback_fn callback)
{
    WListbox *l;

    if (height <= 0)
        height = 1;

    l = g_new (WListbox, 1);
    init_widget (&l->widget, y, x, height, width, listbox_callback, listbox_event);

    l->list = NULL;
    l->top = l->pos = 0;
    l->count = 0;
    l->deletable = deletable;
    l->callback = callback;
    l->allow_duplicates = TRUE;
    l->scrollbar = !mc_global.tty.slow_terminal;
    widget_want_hotkey (l->widget, TRUE);
    widget_want_cursor (l->widget, FALSE);

    return l;
}

/* --------------------------------------------------------------------------------------------- */

int
listbox_search_text (WListbox * l, const char *text)
{
    if (l != NULL)
    {
        int i;
        GList *le;

        for (i = 0, le = l->list; le != NULL; i++, le = g_list_next (le))
        {
            WLEntry *e = (WLEntry *) le->data;

            if (strcmp (e->text, text) == 0)
                return i;
        }
    }

    return (-1);
}

/* --------------------------------------------------------------------------------------------- */

/* Selects the first entry and scrolls the list to the top */
void
listbox_select_first (WListbox * l)
{
    l->pos = l->top = 0;
}

/* --------------------------------------------------------------------------------------------- */

/* Selects the last entry and scrolls the list to the bottom */
void
listbox_select_last (WListbox * l)
{
    l->pos = l->count - 1;
    l->top = l->count > l->widget.lines ? l->count - l->widget.lines : 0;
}

/* --------------------------------------------------------------------------------------------- */

void
listbox_select_entry (WListbox * l, int dest)
{
    GList *le;
    int pos;
    gboolean top_seen = FALSE;

    if (dest < 0)
        return;

    /* Special case */
    for (pos = 0, le = l->list; le != NULL; pos++, le = g_list_next (le))
    {
        if (pos == l->top)
            top_seen = TRUE;

        if (pos == dest)
        {
            l->pos = dest;
            if (!top_seen)
                l->top = l->pos;
            else if (l->pos - l->top >= l->widget.lines)
                l->top = l->pos - l->widget.lines + 1;
            return;
        }
    }

    /* If we are unable to find it, set decent values */
    l->pos = l->top = 0;
}

/* --------------------------------------------------------------------------------------------- */

/* Returns the current string text as well as the associated extra data */
void
listbox_get_current (WListbox * l, char **string, void **extra)
{
    WLEntry *e = NULL;
    gboolean ok;

    if (l != NULL)
        e = (WLEntry *) g_list_nth_data (l->list, l->pos);

    ok = (e != NULL);

    if (string != NULL)
        *string = ok ? e->text : NULL;

    if (extra != NULL)
        *extra = ok ? e->data : NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
listbox_remove_current (WListbox * l)
{
    if ((l != NULL) && (l->count != 0))
    {
        GList *current;

        current = g_list_nth (l->list, l->pos);
        l->list = g_list_remove_link (l->list, current);
        listbox_entry_free ((WLEntry *) current->data);
        g_list_free_1 (current);
        l->count--;

        if (l->count == 0)
            l->top = l->pos = 0;
        else if (l->pos >= l->count)
            l->pos = l->count - 1;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
listbox_set_list (WListbox * l, GList * list)
{
    listbox_remove_list (l);

    if (l != NULL)
    {
        l->list = list;
        l->top = l->pos = 0;
        l->count = g_list_length (list);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
listbox_remove_list (WListbox * l)
{
    if ((l != NULL) && (l->count != 0))
    {
        g_list_foreach (l->list, (GFunc) listbox_entry_free, NULL);
        g_list_free (l->list);
        l->list = NULL;
        l->count = l->pos = l->top = 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

char *
listbox_add_item (WListbox * l, listbox_append_t pos, int hotkey, const char *text, void *data)
{
    WLEntry *entry;

    if (l == NULL)
        return NULL;

    if (!l->allow_duplicates && (listbox_search_text (l, text) >= 0))
        return NULL;

    entry = g_new (WLEntry, 1);
    entry->text = g_strdup (text);
    entry->data = data;
    entry->hotkey = hotkey;

    listbox_append_item (l, entry, pos);

    return entry->text;
}

/* --------------------------------------------------------------------------------------------- */
