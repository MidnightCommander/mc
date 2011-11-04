/*
   Dialog box features module for the Midnight Commander

   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2007, 2009, 2010, 2011
   The Free Software Foundation, Inc.

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

/** \file dialog.c
 *  \brief Source: dialog box features module
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>              /* open() */

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/skin.h"
#include "lib/tty/mouse.h"
#include "lib/tty/key.h"
#include "lib/strutil.h"
#include "lib/widget.h"
#include "lib/fileloc.h"        /* MC_HISTORY_FILE */
#include "lib/event.h"          /* mc_event_raise() */

/*** global variables ****************************************************************************/

/* Color styles for normal and error dialogs */
dlg_colors_t dialog_colors;
dlg_colors_t alarm_colors;

/* Primitive way to check if the the current dialog is our dialog */
/* This is needed by async routines like load_prompt */
GList *top_dlg = NULL;

/* A hook list for idle events */
hook_t *idle_hook = NULL;

/* If set then dialogs just clean the screen when refreshing, else */
/* they do a complete refresh, refreshing all the parts of the program */
int fast_refresh = 0;

/* left click outside of dialog closes it */
int mouse_close_dialog = 0;

const global_keymap_t *dialog_map = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/** What to do if the requested widget doesn't take focus */
typedef enum
{
    SELECT_NEXT,                /* go the the next widget */
    SELECT_PREV,                /* go the the previous widget */
    SELECT_EXACT                /* use current widget */
} select_dir_t;

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static GList *
dlg_widget_next (Dlg_head * h, GList * l)
{
    GList *next;

    next = g_list_next (l);
    if (next == NULL)
        next = h->widgets;

    return next;
}

/* --------------------------------------------------------------------------------------------- */

static GList *
dlg_widget_prev (Dlg_head * h, GList * l)
{
    GList *prev;

    prev = g_list_previous (l);
    if (prev == NULL)
        prev = g_list_last (h->widgets);

    return prev;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * broadcast a message to all the widgets in a dialog that have
 * the options set to flags. If flags is zero, the message is sent
 * to all widgets.
 */

static void
dlg_broadcast_msg_to (Dlg_head * h, widget_msg_t msg, gboolean reverse, int flags)
{
    GList *p, *first;

    if (h->widgets == NULL)
        return;

    if (h->current == NULL)
        h->current = h->widgets;

    if (reverse)
        p = dlg_widget_prev (h, h->current);
    else
        p = dlg_widget_next (h, h->current);

    first = p;

    do
    {
        Widget *w = (Widget *) p->data;

        if (reverse)
            p = dlg_widget_prev (h, p);
        else
            p = dlg_widget_next (h, p);

        if ((flags == 0) || ((flags & w->options) != 0))
            send_message (w, msg, 0);
    }
    while (first != p);
}

/* --------------------------------------------------------------------------------------------- */

/**
  * Read histories from the ${XDG_CACHE_HOME}/mc/history file
  */
static void
dlg_read_history (Dlg_head * h)
{
    char *profile;
    ev_history_load_save_t event_data;

    if (num_history_items_recorded == 0)        /* this is how to disable */
        return;

    profile = g_build_filename (mc_config_get_cache_path (), MC_HISTORY_FILE, NULL);
    event_data.cfg = mc_config_init (profile);
    event_data.receiver = NULL;

    /* create all histories in dialog */
    mc_event_raise (h->event_group, MCEVENT_HISTORY_LOAD, &event_data);

    mc_config_deinit (event_data.cfg);
    g_free (profile);
}

/* --------------------------------------------------------------------------------------------- */

static int
dlg_unfocus (Dlg_head * h)
{
    /* ... but can unfocus disabled widget */

    if ((h->current != NULL) && (h->state == DLG_ACTIVE))
    {
        Widget *current = (Widget *) h->current->data;

        if (send_message (current, WIDGET_UNFOCUS, 0) == MSG_HANDLED)
        {
            h->callback (h, current, DLG_UNFOCUS, 0, NULL);
            return 1;
        }
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
dlg_find_widget_callback (const void *a, const void *b)
{
    const Widget *w = (const Widget *) a;
    callback_fn f = (callback_fn) b;

    return (w->callback == f) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Try to select another widget.  If forward is set, follow tab order.
 * Otherwise go to the previous widget.
 */

static void
do_select_widget (Dlg_head * h, GList * w, select_dir_t dir)
{
    Widget *w0 = (Widget *) h->current->data;

    if (!dlg_unfocus (h))
        return;

    h->current = w;

    do
    {
        if (dlg_focus (h))
            break;

        switch (dir)
        {
        case SELECT_EXACT:
            h->current = g_list_find (h->widgets, w0);
            if (dlg_focus (h))
                return;
            /* try find another widget that can take focus */
            dir = SELECT_NEXT;
            /* fallthrough */
        case SELECT_NEXT:
            h->current = dlg_widget_next (h, h->current);
            break;
        case SELECT_PREV:
            h->current = dlg_widget_prev (h, h->current);
            break;
        }
    }
    while (h->current != w /* && (((Widget *) h->current->data)->options & W_DISABLED) == 0 */ );

    if (dlg_overlap (w0, (Widget *) h->current->data))
    {
        send_message ((Widget *) h->current->data, WIDGET_DRAW, 0);
        send_message ((Widget *) h->current->data, WIDGET_FOCUS, 0);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
refresh_cmd (void)
{
#ifdef HAVE_SLANG
    tty_touch_screen ();
    mc_refresh ();
#else
    /* Use this if the refreshes fail */
    clr_scr ();
    repaint_screen ();
#endif /* HAVE_SLANG */
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dlg_execute_cmd (Dlg_head * h, unsigned long command)
{
    cb_ret_t ret = MSG_HANDLED;
    switch (command)
    {
    case CK_Ok:
        h->ret_value = B_ENTER;
        dlg_stop (h);
        break;
    case CK_Cancel:
        h->ret_value = B_CANCEL;
        dlg_stop (h);
        break;

    case CK_Up:
    case CK_Left:
        dlg_one_up (h);
        break;
    case CK_Down:
    case CK_Right:
        dlg_one_down (h);
        break;

    case CK_Help:
        {
            ev_help_t event_data = { NULL, h->help_ctx };
            mc_event_raise (MCEVENT_GROUP_CORE, "help", &event_data);
        }
        break;

    case CK_Suspend:
        mc_event_raise (MCEVENT_GROUP_CORE, "suspend", NULL);
        refresh_cmd ();
        break;
    case CK_Refresh:
        refresh_cmd ();
        break;

    case CK_ScreenList:
        if (!h->modal)
            dialog_switch_list ();
        else
            ret = MSG_NOT_HANDLED;
        break;
    case CK_ScreenNext:
        if (!h->modal)
            dialog_switch_next ();
        else
            ret = MSG_NOT_HANDLED;
        break;
    case CK_ScreenPrev:
        if (!h->modal)
            dialog_switch_prev ();
        else
            ret = MSG_NOT_HANDLED;
        break;

    default:
        ret = MSG_NOT_HANDLED;
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dlg_handle_key (Dlg_head * h, int d_key)
{
    unsigned long command;
    command = keybind_lookup_keymap_command (dialog_map, d_key);
    if ((command == CK_IgnoreKey) || (dlg_execute_cmd (h, command) == MSG_NOT_HANDLED))
        return MSG_NOT_HANDLED;
    else
        return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static int
dlg_mouse_event (Dlg_head * h, Gpm_Event * event)
{
    GList *item;
    GList *starting_widget = h->current;
    Gpm_Event new_event;
    int x = event->x;
    int y = event->y;

    /* close the dialog by mouse click out of dialog area */
    if (mouse_close_dialog && !h->fullscreen && ((event->buttons & GPM_B_LEFT) != 0) && ((event->type & GPM_DOWN) != 0) /* left click */
        && !((x > h->x) && (x <= h->x + h->cols) && (y > h->y) && (y <= h->y + h->lines)))
    {
        h->ret_value = B_CANCEL;
        dlg_stop (h);
        return MOU_NORMAL;
    }

    item = starting_widget;
    do
    {
        Widget *widget = (Widget *) item->data;

        if ((h->flags & DLG_REVERSE) == 0)
            item = dlg_widget_prev (h, item);
        else
            item = dlg_widget_next (h, item);

        if (((widget->options & W_DISABLED) == 0)
            && (x > widget->x) && (x <= widget->x + widget->cols)
            && (y > widget->y) && (y <= widget->y + widget->lines))
        {
            new_event = *event;
            new_event.x -= widget->x;
            new_event.y -= widget->y;

            if (widget->mouse != NULL)
                return widget->mouse (&new_event, widget);
        }
    }
    while (item != starting_widget);

    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dlg_try_hotkey (Dlg_head * h, int d_key)
{
    GList *hot_cur;
    Widget *current;
    cb_ret_t handled;
    int c;

    if (h->widgets == NULL)
        return MSG_NOT_HANDLED;

    if (h->current == NULL)
        h->current = h->widgets;

    /*
     * Explanation: we don't send letter hotkeys to other widgets if
     * the currently selected widget is an input line
     */

    current = (Widget *) h->current->data;

    if ((current->options & W_DISABLED) != 0)
        return MSG_NOT_HANDLED;

    if (current->options & W_IS_INPUT)
    {
        /* skip ascii control characters, anything else can valid character in
         * some encoding */
        if (d_key >= 32 && d_key < 256)
            return MSG_NOT_HANDLED;
    }

    /* If it's an alt key, send the message */
    c = d_key & ~ALT (0);
    if (d_key & ALT (0) && g_ascii_isalpha (c))
        d_key = g_ascii_tolower (c);

    handled = MSG_NOT_HANDLED;
    if ((current->options & W_WANT_HOTKEY) != 0)
        handled = send_message (current, WIDGET_HOTKEY, d_key);

    /* If not used, send hotkey to other widgets */
    if (handled == MSG_HANDLED)
        return MSG_HANDLED;

    hot_cur = dlg_widget_next (h, h->current);

    /* send it to all widgets */
    while (h->current != hot_cur && handled == MSG_NOT_HANDLED)
    {
        current = (Widget *) hot_cur->data;

        if ((current->options & W_WANT_HOTKEY) != 0)
            handled = send_message (current, WIDGET_HOTKEY, d_key);

        if (handled == MSG_NOT_HANDLED)
            hot_cur = dlg_widget_next (h, hot_cur);
    }

    if (handled == MSG_HANDLED)
        do_select_widget (h, hot_cur, SELECT_EXACT);

    return handled;
}

/* --------------------------------------------------------------------------------------------- */

static void
dlg_key_event (Dlg_head * h, int d_key)
{
    cb_ret_t handled;

    if (h->widgets == NULL)
        return;

    if (h->current == NULL)
        h->current = h->widgets;

    /* TAB used to cycle */
    if ((h->flags & DLG_WANT_TAB) == 0)
    {
        if (d_key == '\t')
        {
            dlg_one_down (h);
            return;
        }
        else if (d_key == KEY_BTAB)
        {
            dlg_one_up (h);
            return;
        }
    }

    /* first can dlg_callback handle the key */
    handled = h->callback (h, NULL, DLG_KEY, d_key, NULL);

    /* next try the hotkey */
    if (handled == MSG_NOT_HANDLED)
        handled = dlg_try_hotkey (h, d_key);

    if (handled == MSG_HANDLED)
        h->callback (h, NULL, DLG_HOTKEY_HANDLED, 0, NULL);
    else
        /* not used - then try widget_callback */
        handled = send_message ((Widget *) h->current->data, WIDGET_KEY, d_key);

    /* not used- try to use the unhandled case */
    if (handled == MSG_NOT_HANDLED)
        handled = h->callback (h, NULL, DLG_UNHANDLED_KEY, d_key, NULL);

    if (handled == MSG_NOT_HANDLED)
        handled = dlg_handle_key (h, d_key);

    h->callback (h, NULL, DLG_POST_KEY, d_key, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
frontend_run_dlg (Dlg_head * h)
{
    int d_key;
    Gpm_Event event;

    event.x = -1;

    /* close opened editors, viewers, etc */
    if (!h->modal && mc_global.widget.midnight_shutdown)
    {
        h->callback (h, NULL, DLG_VALIDATE, 0, NULL);
        return;
    }

    while (h->state == DLG_ACTIVE)
    {
        if (mc_global.tty.winch_flag)
            dialog_change_screen_size ();

        if (is_idle ())
        {
            if (idle_hook)
                execute_hooks (idle_hook);

            while ((h->flags & DLG_WANT_IDLE) && is_idle ())
                h->callback (h, NULL, DLG_IDLE, 0, NULL);

            /* Allow terminating the dialog from the idle handler */
            if (h->state != DLG_ACTIVE)
                break;
        }

        update_cursor (h);

        /* Clear interrupt flag */
        tty_got_interrupt ();
        d_key = tty_get_event (&event, h->mouse_status == MOU_REPEAT, TRUE);

        dlg_process_event (h, d_key, &event);

        if (h->state == DLG_CLOSED)
            h->callback (h, NULL, DLG_VALIDATE, 0, NULL);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
dlg_find_widget_by_id (gconstpointer a, gconstpointer b)
{
    Widget *w = (Widget *) a;
    unsigned long id = GPOINTER_TO_UINT (b);

    return w->id == id ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** draw box in window */
void
draw_box (Dlg_head * h, int y, int x, int ys, int xs, gboolean single)
{
    tty_draw_box (h->y + y, h->x + x, ys, xs, single);
}

/* --------------------------------------------------------------------------------------------- */

/** Clean the dialog area, draw the frame and the title */
void
common_dialog_repaint (Dlg_head * h)
{
    int space;

    if (h->state != DLG_ACTIVE)
        return;

    space = (h->flags & DLG_COMPACT) ? 0 : 1;

    tty_setcolor (h->color[DLG_COLOR_NORMAL]);
    dlg_erase (h);
    draw_box (h, space, space, h->lines - 2 * space, h->cols - 2 * space, FALSE);

    if (h->title != NULL)
    {
        tty_setcolor (h->color[DLG_COLOR_TITLE]);
        dlg_move (h, space, (h->cols - str_term_width1 (h->title)) / 2);
        tty_print_string (h->title);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** this function allows to set dialog position */

void
dlg_set_position (Dlg_head * h, int y1, int x1, int y2, int x2)
{
    /* save old positions, will be used to reposition childs */
    int ox, oy, oc, ol;
    int shift_x, shift_y, scale_x, scale_y;

    /* save old positions, will be used to reposition childs */
    ox = h->x;
    oy = h->y;
    oc = h->cols;
    ol = h->lines;

    h->x = x1;
    h->y = y1;
    h->lines = y2 - y1;
    h->cols = x2 - x1;

    /* dialog is empty */
    if (h->widgets == NULL)
        return;

    if (h->current == NULL)
        h->current = h->widgets;

    /* values by which controls should be moved */
    shift_x = h->x - ox;
    shift_y = h->y - oy;
    scale_x = h->cols - oc;
    scale_y = h->lines - ol;

    if ((shift_x != 0) || (shift_y != 0) || (scale_x != 0) || (scale_y != 0))
    {
        GList *w;

        for (w = h->widgets; w != NULL; w = g_list_next (w))
        {
            /* there are, mainly, 2 generally possible
               situations:

               1. control sticks to one side - it
               should be moved

               2. control sticks to two sides of
               one direction - it should be sized */

            Widget *c = (Widget *) w->data;
            int x = c->x;
            int y = c->y;
            int cols = c->cols;
            int lines = c->lines;

            if ((c->pos_flags & WPOS_KEEP_LEFT) && (c->pos_flags & WPOS_KEEP_RIGHT))
            {
                x += shift_x;
                cols += scale_x;
            }
            else if (c->pos_flags & WPOS_KEEP_LEFT)
                x += shift_x;
            else if (c->pos_flags & WPOS_KEEP_RIGHT)
                x += shift_x + scale_x;

            if ((c->pos_flags & WPOS_KEEP_TOP) && (c->pos_flags & WPOS_KEEP_BOTTOM))
            {
                y += shift_y;
                lines += scale_y;
            }
            else if (c->pos_flags & WPOS_KEEP_TOP)
                y += shift_y;
            else if (c->pos_flags & WPOS_KEEP_BOTTOM)
                y += shift_y + scale_y;

            widget_set_size (c, y, x, lines, cols);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */
/** this function sets only size, leaving positioning to automatic methods */

void
dlg_set_size (Dlg_head * h, int lines, int cols)
{
    int x = h->x;
    int y = h->y;

    if (h->flags & DLG_CENTER)
    {
        y = (LINES - lines) / 2;
        x = (COLS - cols) / 2;
    }

    if ((h->flags & DLG_TRYUP) && (y > 3))
        y -= 2;

    dlg_set_position (h, y, x, y + lines, x + cols);
}

/* --------------------------------------------------------------------------------------------- */
/** Default dialog callback */

cb_ret_t
default_dlg_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    (void) sender;
    (void) parm;
    (void) data;

    switch (msg)
    {
    case DLG_DRAW:
        if (h->color != NULL)
        {
            common_dialog_repaint (h);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case DLG_IDLE:
        dlg_broadcast_msg_to (h, WIDGET_IDLE, FALSE, W_WANT_IDLE);
        return MSG_HANDLED;

    case DLG_RESIZE:
        /* this is default resizing mechanism */
        /* the main idea of this code is to resize dialog
           according to flags (if any of flags require automatic
           resizing, like DLG_CENTER, end after that reposition
           controls in dialog according to flags of widget) */
        dlg_set_size (h, h->lines, h->cols);
        return MSG_HANDLED;

    default:
        break;
    }

    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

Dlg_head *
create_dlg (gboolean modal, int y1, int x1, int lines, int cols,
            const int *colors, dlg_cb_fn callback, const char *help_ctx,
            const char *title, dlg_flags_t flags)
{
    Dlg_head *new_d;

    new_d = g_new0 (Dlg_head, 1);
    new_d->modal = modal;
    if (colors != NULL)
        memmove (new_d->color, colors, sizeof (dlg_colors_t));
    new_d->help_ctx = help_ctx;
    new_d->callback = (callback != NULL) ? callback : default_dlg_callback;
    new_d->x = x1;
    new_d->y = y1;
    new_d->flags = flags;
    new_d->data = NULL;

    dlg_set_size (new_d, lines, cols);
    new_d->fullscreen = (new_d->x == 0 && new_d->y == 0
                         && new_d->cols == COLS && new_d->lines == LINES);

    new_d->mouse_status = MOU_NORMAL;

    /* Strip existing spaces, add one space before and after the title */
    if (title != NULL)
    {
        char *t;

        t = g_strstrip (g_strdup (title));
        if (*t != '\0')
            new_d->title = g_strdup_printf (" %s ", t);
        g_free (t);
    }

    /* unique name got event group for this dialog */
    new_d->event_group = g_strdup_printf ("%s_%p", MCEVENT_GROUP_DIALOG, (void *) new_d);

    return new_d;
}

/* --------------------------------------------------------------------------------------------- */

void
dlg_set_default_colors (void)
{
    dialog_colors[DLG_COLOR_NORMAL] = COLOR_NORMAL;
    dialog_colors[DLG_COLOR_FOCUS] = COLOR_FOCUS;
    dialog_colors[DLG_COLOR_HOT_NORMAL] = COLOR_HOT_NORMAL;
    dialog_colors[DLG_COLOR_HOT_FOCUS] = COLOR_HOT_FOCUS;
    dialog_colors[DLG_COLOR_TITLE] = COLOR_TITLE;

    alarm_colors[DLG_COLOR_NORMAL] = ERROR_COLOR;
    alarm_colors[DLG_COLOR_FOCUS] = ERROR_FOCUS;
    alarm_colors[DLG_COLOR_HOT_NORMAL] = ERROR_HOT_NORMAL;
    alarm_colors[DLG_COLOR_HOT_FOCUS] = ERROR_HOT_FOCUS;
    alarm_colors[DLG_COLOR_TITLE] = ERROR_TITLE;
}

/* --------------------------------------------------------------------------------------------- */

void
dlg_erase (Dlg_head * h)
{
    if ((h != NULL) && (h->state == DLG_ACTIVE))
        tty_fill_region (h->y, h->x, h->lines, h->cols, ' ');
}

/* --------------------------------------------------------------------------------------------- */

void
set_idle_proc (Dlg_head * d, int enable)
{
    if (enable)
        d->flags |= DLG_WANT_IDLE;
    else
        d->flags &= ~DLG_WANT_IDLE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Insert widget to dialog before current widget.  For dialogs populated
 * from the bottom, make the widget current.  Return widget number.
 */

int
add_widget_autopos (Dlg_head * h, void *w, widget_pos_flags_t pos_flags)
{
    Widget *widget = (Widget *) w;

    /* Don't accept 0 widgets */
    if (w == NULL)
        abort ();

    widget->x += h->x;
    widget->y += h->y;
    widget->owner = h;
    widget->pos_flags = pos_flags;
    widget->id = h->widget_id++;

    if ((h->flags & DLG_REVERSE) != 0)
        h->widgets = g_list_prepend (h->widgets, widget);
    else
        h->widgets = g_list_append (h->widgets, widget);

    h->current = h->widgets;

    return widget->id;
}

/* --------------------------------------------------------------------------------------------- */
/** wrapper to simply add lefttop positioned controls */

int
add_widget (Dlg_head * h, void *w)
{
    return add_widget_autopos (h, w, WPOS_KEEP_LEFT | WPOS_KEEP_TOP);
}

/* --------------------------------------------------------------------------------------------- */

void
do_refresh (void)
{
    GList *d = top_dlg;

    if (fast_refresh)
    {
        if ((d != NULL) && (d->data != NULL))
            dlg_redraw ((Dlg_head *) d->data);
    }
    else
    {
        /* Search first fullscreen dialog */
        for (; d != NULL; d = g_list_next (d))
            if ((d->data != NULL) && ((Dlg_head *) d->data)->fullscreen)
                break;
        /* back to top dialog */
        for (; d != NULL; d = g_list_previous (d))
            if (d->data != NULL)
                dlg_redraw ((Dlg_head *) d->data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** broadcast a message to all the widgets in a dialog */

void
dlg_broadcast_msg (Dlg_head * h, widget_msg_t msg, gboolean reverse)
{
    dlg_broadcast_msg_to (h, msg, reverse, 0);
}

/* --------------------------------------------------------------------------------------------- */

int
dlg_focus (Dlg_head * h)
{
    /* cannot focus disabled widget ... */

    if ((h->current != NULL) && (h->state == DLG_ACTIVE))
    {
        Widget *current = (Widget *) h->current->data;

        if (((current->options & W_DISABLED) == 0)
            && (send_message (current, WIDGET_FOCUS, 0) == MSG_HANDLED))
        {
            h->callback (h, current, DLG_FOCUS, 0, NULL);
            return 1;
        }
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/** Return true if the windows overlap */

int
dlg_overlap (Widget * a, Widget * b)
{
    return !((b->x >= a->x + a->cols)
             || (a->x >= b->x + b->cols) || (b->y >= a->y + a->lines) || (a->y >= b->y + b->lines));
}


/* --------------------------------------------------------------------------------------------- */
/** Find the widget with the given callback in the dialog h */

Widget *
find_widget_type (const Dlg_head * h, callback_fn callback)
{
    GList *w;

    w = g_list_find_custom (h->widgets, callback, dlg_find_widget_callback);

    return (w == NULL) ? NULL : (Widget *) w->data;
}

/* --------------------------------------------------------------------------------------------- */
/** Find the widget with the given id */

Widget *
dlg_find_by_id (const Dlg_head * h, unsigned long id)
{
    GList *w;

    w = g_list_find_custom (h->widgets, GUINT_TO_POINTER (id), dlg_find_widget_by_id);
    return w != NULL ? (Widget *) w->data : NULL;
}

/* --------------------------------------------------------------------------------------------- */
/** Find the widget with the given id in the dialog h and select it */

void
dlg_select_by_id (const Dlg_head * h, unsigned long id)
{
    Widget *w;

    w = dlg_find_by_id (h, id);
    if (w != NULL)
        dlg_select_widget (w);
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Try to select widget in the dialog.
 */

void
dlg_select_widget (void *w)
{
    const Widget *widget = (Widget *) w;
    Dlg_head *h = widget->owner;

    do_select_widget (h, g_list_find (h->widgets, widget), SELECT_EXACT);
}

/* --------------------------------------------------------------------------------------------- */
/** Try to select previous widget in the tab order */

void
dlg_one_up (Dlg_head * h)
{
    if (h->widgets != NULL)
        do_select_widget (h, dlg_widget_prev (h, h->current), SELECT_PREV);
}

/* --------------------------------------------------------------------------------------------- */
/** Try to select next widget in the tab order */

void
dlg_one_down (Dlg_head * h)
{
    if (h->widgets != NULL)
        do_select_widget (h, dlg_widget_next (h, h->current), SELECT_NEXT);
}

/* --------------------------------------------------------------------------------------------- */

void
update_cursor (Dlg_head * h)
{
    GList *p = h->current;

    if ((p != NULL) && (h->state == DLG_ACTIVE))
    {
        Widget *w;

        w = (Widget *) p->data;

        if (((w->options & W_DISABLED) == 0) && ((w->options & W_WANT_CURSOR) != 0))
            send_message (w, WIDGET_CURSOR, 0);
        else
            do
            {
                p = dlg_widget_next (h, p);
                if (p == h->current)
                    break;

                w = (Widget *) p->data;

                if (((w->options & W_DISABLED) == 0) && ((w->options & W_WANT_CURSOR) != 0))
                    if (send_message (w, WIDGET_CURSOR, 0) == MSG_HANDLED)
                        break;
            }
            while (TRUE);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Redraw the widgets in reverse order, leaving the current widget
 * as the last one
 */

void
dlg_redraw (Dlg_head * h)
{
    if (h->state != DLG_ACTIVE)
        return;

    if (h->winch_pending)
    {
        h->winch_pending = FALSE;
        h->callback (h, NULL, DLG_RESIZE, 0, NULL);
    }

    h->callback (h, NULL, DLG_DRAW, 0, NULL);
    dlg_broadcast_msg (h, WIDGET_DRAW, TRUE);
    update_cursor (h);
}

/* --------------------------------------------------------------------------------------------- */

void
dlg_stop (Dlg_head * h)
{
    h->state = DLG_CLOSED;
}

/* --------------------------------------------------------------------------------------------- */
/** Init the process */

void
init_dlg (Dlg_head * h)
{
    if ((top_dlg != NULL) && ((Dlg_head *) top_dlg->data)->modal)
        h->modal = TRUE;

    /* add dialog to the stack */
    top_dlg = g_list_prepend (top_dlg, h);

    /* Initialize dialog manager and widgets */
    if (h->state == DLG_ACTIVE)
    {
        if (!h->modal)
            dialog_switch_add (h);

        h->callback (h, NULL, DLG_INIT, 0, NULL);
        dlg_broadcast_msg (h, WIDGET_INIT, FALSE);
        dlg_read_history (h);
    }

    h->state = DLG_ACTIVE;

    dlg_redraw (h);

    /* Select the first widget that takes focus */
    while (h->current != NULL && !dlg_focus (h))
        h->current = dlg_widget_next (h, h->current);

    h->ret_value = 0;
}

/* --------------------------------------------------------------------------------------------- */

void
dlg_process_event (Dlg_head * h, int key, Gpm_Event * event)
{
    if (key == EV_NONE)
    {
        if (tty_got_interrupt ())
            if (h->callback (h, NULL, DLG_ACTION, CK_Cancel, NULL) != MSG_HANDLED)
                dlg_execute_cmd (h, CK_Cancel);

        return;
    }

    if (key == EV_MOUSE)
        h->mouse_status = dlg_mouse_event (h, event);
    else
        dlg_key_event (h, key);
}

/* --------------------------------------------------------------------------------------------- */
/** Shutdown the run_dlg */

void
dlg_run_done (Dlg_head * h)
{
    top_dlg = g_list_remove (top_dlg, h);

    if (h->state == DLG_CLOSED)
    {
        h->callback (h, (Widget *) h->current->data, DLG_END, 0, NULL);
        if (!h->modal)
            dialog_switch_remove (h);
    }

}

/* --------------------------------------------------------------------------------------------- */
/**
 * Standard run dialog routine
 * We have to keep this routine small so that we can duplicate it's
 * behavior on complex routines like the file routines, this way,
 * they can call the dlg_process_event without rewriting all the code
 */

int
run_dlg (Dlg_head * h)
{
    init_dlg (h);
    frontend_run_dlg (h);
    dlg_run_done (h);
    return h->ret_value;
}

/* --------------------------------------------------------------------------------------------- */

void
destroy_dlg (Dlg_head * h)
{
    /* if some widgets have history, save all history at one moment here */
    dlg_save_history (h);
    dlg_broadcast_msg (h, WIDGET_DESTROY, FALSE);
    g_list_foreach (h->widgets, (GFunc) g_free, NULL);
    g_list_free (h->widgets);
    mc_event_group_del (h->event_group);
    g_free (h->event_group);
    g_free (h->title);
    g_free (h);

    do_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

/**
  * Write history to the ${XDG_CACHE_HOME}/mc/history file
  */
void
dlg_save_history (Dlg_head * h)
{
    char *profile;
    int i;

    if (num_history_items_recorded == 0)        /* this is how to disable */
        return;

    profile = g_build_filename (mc_config_get_cache_path (), MC_HISTORY_FILE, (char *) NULL);
    i = open (profile, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (i != -1)
        close (i);

    /* Make sure the history is only readable by the user */
    if (chmod (profile, S_IRUSR | S_IWUSR) != -1 || errno == ENOENT)
    {
        ev_history_load_save_t event_data;

        event_data.cfg = mc_config_init (profile);
        event_data.receiver = NULL;

        /* get all histories in dialog */
        mc_event_raise (h->event_group, MCEVENT_HISTORY_SAVE, &event_data);

        mc_config_save_file (event_data.cfg, NULL);
        mc_config_deinit (event_data.cfg);
    }

    g_free (profile);
}

/* --------------------------------------------------------------------------------------------- */

char *
dlg_get_title (const Dlg_head * h, size_t len)
{
    char *t;

    if (h == NULL)
        abort ();

    if (h->get_title != NULL)
        t = h->get_title (h, len);
    else
        t = g_strdup ("");

    return t;
}

/* --------------------------------------------------------------------------------------------- */
/** Replace widget old_w for widget new_w in the dialog */

void
dlg_replace_widget (Widget * old_w, Widget * new_w)
{
    Dlg_head *h = old_w->owner;
    gboolean should_focus = FALSE;

    if (h->widgets == NULL)
        return;

    if (h->current == NULL)
        h->current = h->widgets;

    if (old_w == h->current->data)
        should_focus = TRUE;

    new_w->owner = h;
    new_w->id = old_w->id;

    if (should_focus)
        h->current->data = new_w;
    else
        g_list_find (h->widgets, old_w)->data = new_w;

    send_message (old_w, WIDGET_DESTROY, 0);
    send_message (new_w, WIDGET_INIT, 0);

    if (should_focus)
        dlg_select_widget (new_w);

    send_message (new_w, WIDGET_DRAW, 0);
}

/* --------------------------------------------------------------------------------------------- */
