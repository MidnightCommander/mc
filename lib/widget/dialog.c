/*
   Dialog box features module for the Midnight Commander

   Copyright (C) 1994-2014
   Free Software Foundation, Inc.

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

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/skin.h"
#include "lib/tty/key.h"
#include "lib/strutil.h"
#include "lib/fileloc.h"        /* MC_HISTORY_FILE */
#include "lib/event.h"          /* mc_event_raise() */

#include "lib/widget.h"
#include "lib/widget/mouse.h"

/*** global variables ****************************************************************************/

/* Color styles for normal and error dialogs */
dlg_colors_t dialog_colors;
dlg_colors_t alarm_colors;
dlg_colors_t listbox_colors;

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

/* Control widget positions in dialog */
typedef struct
{
    int shift_x;
    int scale_x;
    int shift_y;
    int scale_y;
} widget_shift_scale_t;

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static GList *
dlg_widget_next (WDialog * h, GList * l)
{
    GList *next;

    next = g_list_next (l);
    if (next == NULL)
        next = h->widgets;

    return next;
}

/* --------------------------------------------------------------------------------------------- */

static GList *
dlg_widget_prev (WDialog * h, GList * l)
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
dlg_broadcast_msg_to (WDialog * h, widget_msg_t msg, gboolean reverse, widget_options_t flags)
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
        Widget *w = WIDGET (p->data);

        if (reverse)
            p = dlg_widget_prev (h, p);
        else
            p = dlg_widget_next (h, p);

        if ((flags == 0) || ((flags & w->options) != 0))
            send_message (w, NULL, msg, 0, NULL);
    }
    while (first != p);
}

/* --------------------------------------------------------------------------------------------- */

/**
  * Read histories from the ${XDG_CACHE_HOME}/mc/history file
  */
static void
dlg_read_history (WDialog * h)
{
    char *profile;
    ev_history_load_save_t event_data;

    if (num_history_items_recorded == 0)        /* this is how to disable */
        return;

    profile = mc_config_get_full_path (MC_HISTORY_FILE);
    event_data.cfg = mc_config_init (profile, TRUE);
    event_data.receiver = NULL;

    /* create all histories in dialog */
    mc_event_raise (h->event_group, MCEVENT_HISTORY_LOAD, &event_data);

    mc_config_deinit (event_data.cfg);
    g_free (profile);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
dlg_unfocus (WDialog * h)
{
    /* we can unfocus disabled widget */
    if (h->current != NULL)
    {
        Widget *wh = WIDGET (h);

        if (widget_get_state (wh, WST_CONSTRUCT) || widget_get_state (wh, WST_ACTIVE))
        {
            Widget *current = WIDGET (h->current->data);

            if (send_message (current, NULL, MSG_UNFOCUS, 0, NULL) == MSG_HANDLED)
            {
                send_message (h, current, MSG_UNFOCUS, 0, NULL);
                return TRUE;
            }

        }
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static int
dlg_find_widget_callback (const void *a, const void *b)
{
    const Widget *w = CONST_WIDGET (a);
    const widget_cb_fn f = b;

    return (w->callback == f) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Put widget on top or bottom of Z-order.
 */
static void
dlg_reorder_widgets (GList * l, gboolean set_top)
{
    WDialog *h = WIDGET (l->data)->owner;

    h->widgets = g_list_remove_link (h->widgets, l);
    if (set_top)
        h->widgets = g_list_concat (h->widgets, l);
    else
        h->widgets = g_list_concat (l, h->widgets);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Try to select another widget.  If forward is set, follow tab order.
 * Otherwise go to the previous widget.
 */

static void
do_select_widget (WDialog * h, GList * w, select_dir_t dir)
{
    Widget *w0 = WIDGET (h->current->data);

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
        default:
            break;
        }
    }
    while (h->current != w);

    if (widget_get_options (WIDGET (h->current->data), WOP_TOP_SELECT))
        dlg_reorder_widgets (h->current, TRUE);

    if (widget_overlapped (w0, WIDGET (h->current->data)))
    {
        send_message (h->current->data, NULL, MSG_DRAW, 0, NULL);
        send_message (h->current->data, NULL, MSG_FOCUS, 0, NULL);
    }

    if (widget_get_options (WIDGET (h->current->data), WOP_TOP_SELECT))
    {
        h->widgets = g_list_remove_link (h->widgets, h->current);
        h->widgets = g_list_concat (h->widgets, h->current);
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
dlg_execute_cmd (WDialog * h, long command)
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
        if (!widget_get_state (WIDGET (h), WST_MODAL))
            dialog_switch_list ();
        else
            ret = MSG_NOT_HANDLED;
        break;
    case CK_ScreenNext:
        if (!widget_get_state (WIDGET (h), WST_MODAL))
            dialog_switch_next ();
        else
            ret = MSG_NOT_HANDLED;
        break;
    case CK_ScreenPrev:
        if (!widget_get_state (WIDGET (h), WST_MODAL))
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
dlg_handle_key (WDialog * h, int d_key)
{
    long command;

    command = keybind_lookup_keymap_command (dialog_map, d_key);

    if (command == CK_IgnoreKey)
        return MSG_NOT_HANDLED;

    if (send_message (h, NULL, MSG_ACTION, command, NULL) == MSG_HANDLED
        || dlg_execute_cmd (h, command) == MSG_HANDLED)
        return MSG_HANDLED;

    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * This is the low-level mouse handler.
 * It receives a Gpm_Event event and translates it into a higher level protocol.
 */
static int
dlg_mouse_translator (Gpm_Event * event, Widget * w)
{
    mouse_event_t me;

    me = mouse_translate_event (w, event);

    return mouse_process_event (w, &me);
}

/* --------------------------------------------------------------------------------------------- */

static int
dlg_mouse_event (WDialog * h, Gpm_Event * event)
{
    Widget *wh = WIDGET (h);

    GList *p;

    /* close the dialog by mouse left click out of dialog area */
    if (mouse_close_dialog && (wh->pos_flags & WPOS_FULLSCREEN) == 0
        && ((event->buttons & GPM_B_LEFT) != 0) && ((event->type & GPM_DOWN) != 0)
        && !mouse_global_in_widget (event, wh))
    {
        h->ret_value = B_CANCEL;
        dlg_stop (h);
        return MOU_NORMAL;
    }

    if (wh->mouse_callback != NULL)
    {
        int mou;

        mou = dlg_mouse_translator (event, wh);
        if (mou != MOU_UNHANDLED)
            return mou;
    }

    /* send the event to widgets in reverse Z-order */
    p = g_list_last (h->widgets);
    do
    {
        Widget *w = WIDGET (p->data);

        if (!widget_get_state (w, WST_DISABLED) && w->mouse_callback != NULL)
        {
            /* put global cursor position to the widget */
            int ret;

            ret = dlg_mouse_translator (event, w);
            if (ret != MOU_UNHANDLED)
                return ret;
        }

        p = g_list_previous (p);
    }
    while (p != NULL);

    return MOU_UNHANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dlg_try_hotkey (WDialog * h, int d_key)
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

    current = WIDGET (h->current->data);

    if (widget_get_state (current, WST_DISABLED))
        return MSG_NOT_HANDLED;

    if (widget_get_options (current, WOP_IS_INPUT))
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
    if (widget_get_options (current, WOP_WANT_HOTKEY))
        handled = send_message (current, NULL, MSG_HOTKEY, d_key, NULL);

    /* If not used, send hotkey to other widgets */
    if (handled == MSG_HANDLED)
        return MSG_HANDLED;

    hot_cur = dlg_widget_next (h, h->current);

    /* send it to all widgets */
    while (h->current != hot_cur && handled == MSG_NOT_HANDLED)
    {
        current = WIDGET (hot_cur->data);

        if (widget_get_options (current, WOP_WANT_HOTKEY)
            && !widget_get_state (current, WST_DISABLED))
            handled = send_message (current, NULL, MSG_HOTKEY, d_key, NULL);

        if (handled == MSG_NOT_HANDLED)
            hot_cur = dlg_widget_next (h, hot_cur);
    }

    if (handled == MSG_HANDLED)
        do_select_widget (h, hot_cur, SELECT_EXACT);

    return handled;
}

/* --------------------------------------------------------------------------------------------- */

static void
dlg_key_event (WDialog * h, int d_key)
{
    cb_ret_t handled;

    if (h->widgets == NULL)
        return;

    if (h->current == NULL)
        h->current = h->widgets;

    /* TAB used to cycle */
    if (!widget_get_options (WIDGET (h), WOP_WANT_TAB))
    {
        if (d_key == '\t')
        {
            dlg_one_down (h);
            return;
        }
        else if ((d_key & ~(KEY_M_SHIFT | KEY_M_CTRL)) == '\t')
        {
            dlg_one_up (h);
            return;
        }
    }

    /* first can dlg_callback handle the key */
    handled = send_message (h, NULL, MSG_KEY, d_key, NULL);

    /* next try the hotkey */
    if (handled == MSG_NOT_HANDLED)
        handled = dlg_try_hotkey (h, d_key);

    if (handled == MSG_HANDLED)
        send_message (h, NULL, MSG_HOTKEY_HANDLED, 0, NULL);
    else
        /* not used - then try widget_callback */
        handled = send_message (h->current->data, NULL, MSG_KEY, d_key, NULL);

    /* not used- try to use the unhandled case */
    if (handled == MSG_NOT_HANDLED)
        handled = send_message (h, NULL, MSG_UNHANDLED_KEY, d_key, NULL);

    if (handled == MSG_NOT_HANDLED)
        handled = dlg_handle_key (h, d_key);

    (void) handled;
    send_message (h, NULL, MSG_POST_KEY, d_key, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
frontend_dlg_run (WDialog * h)
{
    Widget *wh = WIDGET (h);
    Gpm_Event event;

    event.x = -1;

    /* close opened editors, viewers, etc */
    if (!widget_get_state (wh, WST_MODAL) && mc_global.midnight_shutdown)
    {
        send_message (h, NULL, MSG_VALIDATE, 0, NULL);
        return;
    }

    while (widget_get_state (wh, WST_ACTIVE))
    {
        int d_key;

        if (mc_global.tty.winch_flag != 0)
            dialog_change_screen_size ();

        if (is_idle ())
        {
            if (idle_hook)
                execute_hooks (idle_hook);

            while (widget_get_state (wh, WST_IDLE) && is_idle ())
                send_message (wh, NULL, MSG_IDLE, 0, NULL);

            /* Allow terminating the dialog from the idle handler */
            if (!widget_get_state (wh, WST_ACTIVE))
                break;
        }

        update_cursor (h);

        /* Clear interrupt flag */
        tty_got_interrupt ();
        d_key = tty_get_event (&event, h->mouse_status == MOU_REPEAT, TRUE);

        dlg_process_event (h, d_key, &event);

        if (widget_get_state (wh, WST_CLOSED))
            send_message (h, NULL, MSG_VALIDATE, 0, NULL);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
dlg_find_widget_by_id (gconstpointer a, gconstpointer b)
{
    const Widget *w = CONST_WIDGET (a);
    unsigned long id = GPOINTER_TO_UINT (b);

    return w->id == id ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
static void
dlg_widget_set_position (gpointer data, gpointer user_data)
{
    /* there are, mainly, 2 generally possible situations:
     * 1. control sticks to one side - it should be moved
     * 2. control sticks to two sides of one direction - it should be sized
     */

    Widget *c = WIDGET (data);
    Widget *wh = WIDGET (c->owner);
    const widget_shift_scale_t *wss = (const widget_shift_scale_t *) user_data;
    int x = c->x;
    int y = c->y;
    int cols = c->cols;
    int lines = c->lines;

    if ((c->pos_flags & WPOS_CENTER_HORZ) != 0)
        x = wh->x + (wh->cols - c->cols) / 2;
    else if ((c->pos_flags & WPOS_KEEP_LEFT) != 0 && (c->pos_flags & WPOS_KEEP_RIGHT) != 0)
    {
        x += wss->shift_x;
        cols += wss->scale_x;
    }
    else if ((c->pos_flags & WPOS_KEEP_LEFT) != 0)
        x += wss->shift_x;
    else if ((c->pos_flags & WPOS_KEEP_RIGHT) != 0)
        x += wss->shift_x + wss->scale_x;

    if ((c->pos_flags & WPOS_CENTER_VERT) != 0)
        y = wh->y + (wh->lines - c->lines) / 2;
    else if ((c->pos_flags & WPOS_KEEP_TOP) != 0 && (c->pos_flags & WPOS_KEEP_BOTTOM) != 0)
    {
        y += wss->shift_y;
        lines += wss->scale_y;
    }
    else if ((c->pos_flags & WPOS_KEEP_TOP) != 0)
        y += wss->shift_y;
    else if ((c->pos_flags & WPOS_KEEP_BOTTOM) != 0)
        y += wss->shift_y + wss->scale_y;

    widget_set_size (c, y, x, lines, cols);
}

/* --------------------------------------------------------------------------------------------- */

static void
dlg_adjust_position (const WDialog * h, int *y, int *x, int *lines, int *cols)
{
    const Widget *w = CONST_WIDGET (h);

    if ((w->pos_flags & WPOS_FULLSCREEN) != 0)
    {
        *y = 0;
        *x = 0;
        *lines = LINES;
        *cols = COLS;
    }
    else
    {
        if ((w->pos_flags & WPOS_CENTER_HORZ) != 0)
            *x = (COLS - *cols) / 2;
        else
            *x = w->x;

        if ((w->pos_flags & WPOS_CENTER_VERT) != 0)
            *y = (LINES - *lines) / 2;
        else
            *y = w->y;

        if ((w->pos_flags & WPOS_TRYUP) != 0)
        {
            if (*y > 3)
                *y -= 2;
            else if (*y == 3)
                *y = 2;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Clean the dialog area, draw the frame and the title */
void
dlg_default_repaint (WDialog * h)
{
    Widget *wh = WIDGET (h);

    int space;

    if (!widget_get_state (wh, WST_ACTIVE))
        return;

    space = h->compact ? 0 : 1;

    tty_setcolor (h->color[DLG_COLOR_NORMAL]);
    dlg_erase (h);
    tty_draw_box (wh->y + space, wh->x + space, wh->lines - 2 * space, wh->cols - 2 * space, FALSE);

    if (h->title != NULL)
    {
        tty_setcolor (h->color[DLG_COLOR_TITLE]);
        widget_move (h, space, (wh->cols - str_term_width1 (h->title)) / 2);
        tty_print_string (h->title);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** this function allows to set dialog position */

void
dlg_set_position (WDialog * h, int y, int x, int lines, int cols)
{
    Widget *wh = WIDGET (h);
    widget_shift_scale_t wss;

    /* save old positions, will be used to reposition childs */
    int ox, oy, oc, ol;

    /* save old positions, will be used to reposition childs */
    ox = wh->x;
    oy = wh->y;
    oc = wh->cols;
    ol = wh->lines;

    wh->x = x;
    wh->y = y;
    wh->lines = lines;
    wh->cols = cols;

    /* dialog is empty */
    if (h->widgets == NULL)
        return;

    if (h->current == NULL)
        h->current = h->widgets;

    /* values by which controls should be moved */
    wss.shift_x = wh->x - ox;
    wss.scale_x = wh->cols - oc;
    wss.shift_y = wh->y - oy;
    wss.scale_y = wh->lines - ol;

    if (wss.shift_x != 0 || wss.shift_y != 0 || wss.scale_x != 0 || wss.scale_y != 0)
        g_list_foreach (h->widgets, dlg_widget_set_position, &wss);
}

/* --------------------------------------------------------------------------------------------- */
/** Set dialog size and position */

void
dlg_set_size (WDialog * h, int lines, int cols)
{
    int x = 0, y = 0;

    dlg_adjust_position (h, &y, &x, &lines, &cols);
    dlg_set_position (h, y, x, lines, cols);
}

/* --------------------------------------------------------------------------------------------- */
/** Default dialog callback */

cb_ret_t
dlg_default_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);

    (void) sender;
    (void) parm;
    (void) data;

    switch (msg)
    {
    case MSG_DRAW:
        if (h->color != NULL)
        {
            dlg_default_repaint (h);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case MSG_IDLE:
        /* we don't want endless loop */
        widget_idle (w, FALSE);
        return MSG_HANDLED;

    case MSG_RESIZE:
        /* this is default resizing mechanism */
        /* the main idea of this code is to resize dialog
           according to flags (if any of flags require automatic
           resizing, like WPOS_CENTER, end after that reposition
           controls in dialog according to flags of widget) */
        dlg_set_size (h, w->lines, w->cols);
        return MSG_HANDLED;

    default:
        break;
    }

    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

WDialog *
dlg_create (gboolean modal, int y1, int x1, int lines, int cols, widget_pos_flags_t pos_flags,
            gboolean compact, const int *colors, widget_cb_fn callback,
            widget_mouse_cb_fn mouse_callback, const char *help_ctx, const char *title)
{
    WDialog *new_d;
    Widget *w;

    new_d = g_new0 (WDialog, 1);
    w = WIDGET (new_d);
    w->pos_flags = pos_flags;   /* required for dlg_adjust_position() */
    dlg_adjust_position (new_d, &y1, &x1, &lines, &cols);
    widget_init (w, y1, x1, lines, cols, (callback != NULL) ? callback : dlg_default_callback,
                 mouse_callback);
    w->pos_flags = pos_flags;   /* restore after widget_init() */
    w->options |= WOP_TOP_SELECT;

    w->state |= WST_CONSTRUCT;
    if (modal)
        w->state |= WST_MODAL;

    new_d->color = colors;
    new_d->help_ctx = help_ctx;
    new_d->compact = compact;
    new_d->data = NULL;

    new_d->mouse_status = MOU_UNHANDLED;

    /* Strip existing spaces, add one space before and after the title */
    if (title != NULL && *title != '\0')
    {
        char *t;

        t = g_strstrip (g_strdup (title));
        if (*t != '\0')
            new_d->title = g_strdup_printf (" %s ", t);
        g_free (t);
    }

    /* unique name of event group for this dialog */
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

    listbox_colors[DLG_COLOR_NORMAL] = PMENU_ENTRY_COLOR;
    listbox_colors[DLG_COLOR_FOCUS] = PMENU_SELECTED_COLOR;
    listbox_colors[DLG_COLOR_HOT_NORMAL] = PMENU_ENTRY_COLOR;
    listbox_colors[DLG_COLOR_HOT_FOCUS] = PMENU_SELECTED_COLOR;
    listbox_colors[DLG_COLOR_TITLE] = PMENU_TITLE_COLOR;
}

/* --------------------------------------------------------------------------------------------- */

void
dlg_erase (WDialog * h)
{
    Widget *wh = WIDGET (h);

    if (wh != NULL && widget_get_state (wh, WST_ACTIVE))
        tty_fill_region (wh->y, wh->x, wh->lines, wh->cols, ' ');
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Insert widget to dialog before requested widget. Make the widget current. Return widget ID.
 */

unsigned long
add_widget_autopos (WDialog * h, void *w, widget_pos_flags_t pos_flags, const void *before)
{
    Widget *wh = WIDGET (h);
    Widget *widget;

    /* Don't accept 0 widgets */
    if (w == NULL)
        abort ();

    widget = WIDGET (w);

    if ((pos_flags & WPOS_CENTER_HORZ) != 0)
        widget->x = (wh->cols - widget->cols) / 2;
    widget->x += wh->x;

    if ((pos_flags & WPOS_CENTER_VERT) != 0)
        widget->y = (wh->lines - widget->lines) / 2;
    widget->y += wh->y;

    widget->owner = h;
    widget->pos_flags = pos_flags;
    widget->id = h->widget_id++;

    if (h->widgets == NULL || before == NULL)
    {
        h->widgets = g_list_append (h->widgets, widget);
        h->current = g_list_last (h->widgets);
    }
    else
    {
        GList *b;

        b = g_list_find (h->widgets, before);

        /* don't accept widget not from dialog. This shouldn't happen */
        if (b == NULL)
            abort ();

        b = g_list_next (b);
        h->widgets = g_list_insert_before (h->widgets, b, widget);
        if (b != NULL)
            h->current = g_list_previous (b);
        else
            h->current = g_list_last (h->widgets);
    }

    /* widget has been added in runtime */
    if (widget_get_state (wh, WST_ACTIVE))
    {
        send_message (widget, NULL, MSG_INIT, 0, NULL);
        send_message (widget, NULL, MSG_DRAW, 0, NULL);
        send_message (widget, NULL, MSG_FOCUS, 0, NULL);
    }

    return widget->id;
}

/* --------------------------------------------------------------------------------------------- */
/** wrapper to simply add lefttop positioned controls */

unsigned long
add_widget (WDialog * h, void *w)
{
    return add_widget_autopos (h, w, WPOS_KEEP_DEFAULT,
                               h->current != NULL ? h->current->data : NULL);
}

/* --------------------------------------------------------------------------------------------- */

unsigned long
add_widget_before (WDialog * h, void *w, void *before)
{
    return add_widget_autopos (h, w, WPOS_KEEP_DEFAULT, before);
}

/* --------------------------------------------------------------------------------------------- */

/** delete widget from dialog */
void
del_widget (void *w)
{
    WDialog *h;
    GList *d;

    /* Don't accept NULL widget. This shouldn't happen */
    if (w == NULL)
        abort ();

    h = WIDGET (w)->owner;

    d = g_list_find (h->widgets, w);
    if (d == h->current)
        h->current = dlg_widget_next (h, d);

    h->widgets = g_list_remove_link (h->widgets, d);
    send_message (d->data, NULL, MSG_DESTROY, 0, NULL);
    g_free (d->data);
    g_list_free_1 (d);

    /* widget has been deleted in runtime */
    if (widget_get_state (WIDGET (h), WST_ACTIVE))
    {
        dlg_redraw (h);
        dlg_focus (h);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
do_refresh (void)
{
    GList *d = top_dlg;

    if (fast_refresh)
    {
        if ((d != NULL) && (d->data != NULL))
            dlg_redraw (DIALOG (d->data));
    }
    else
    {
        /* Search first fullscreen dialog */
        for (; d != NULL; d = g_list_next (d))
            if (d->data != NULL && (WIDGET (d->data)->pos_flags & WPOS_FULLSCREEN) != 0)
                break;
        /* back to top dialog */
        for (; d != NULL; d = g_list_previous (d))
            if (d->data != NULL)
                dlg_redraw (DIALOG (d->data));
    }
}

/* --------------------------------------------------------------------------------------------- */
/** broadcast a message to all the widgets in a dialog */

void
dlg_broadcast_msg (WDialog * h, widget_msg_t msg)
{
    dlg_broadcast_msg_to (h, msg, FALSE, 0);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dlg_focus (WDialog * h)
{
    /* cannot focus disabled widget */
    if (h->current != NULL)
    {
        Widget *wh = WIDGET (h);

        if (widget_get_state (wh, WST_CONSTRUCT) || widget_get_state (wh, WST_ACTIVE))
        {
            Widget *current = WIDGET (h->current->data);

            if (!widget_get_state (current, WST_DISABLED)
                && (send_message (current, NULL, MSG_FOCUS, 0, NULL) == MSG_HANDLED))
            {
                send_message (h, current, MSG_FOCUS, 0, NULL);
                return TRUE;
            }
        }
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/** Find the widget with the given callback in the dialog h */

Widget *
find_widget_type (const WDialog * h, widget_cb_fn callback)
{
    GList *w;

    w = g_list_find_custom (h->widgets, (gconstpointer) callback, dlg_find_widget_callback);

    return (w == NULL) ? NULL : WIDGET (w->data);
}

/* --------------------------------------------------------------------------------------------- */
/** Find the widget with the given id */

Widget *
dlg_find_by_id (const WDialog * h, unsigned long id)
{
    GList *w;

    w = g_list_find_custom (h->widgets, GUINT_TO_POINTER (id), dlg_find_widget_by_id);
    return w != NULL ? WIDGET (w->data) : NULL;
}

/* --------------------------------------------------------------------------------------------- */
/** Find the widget with the given id in the dialog h and select it */

void
dlg_select_by_id (const WDialog * h, unsigned long id)
{
    Widget *w;

    w = dlg_find_by_id (h, id);
    if (w != NULL)
        dlg_select_widget (w);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Try to select widget in the dialog.
 */

void
dlg_select_widget (void *w)
{
    Widget *widget = WIDGET (w);
    WDialog *h = widget->owner;

    do_select_widget (h, g_list_find (h->widgets, widget), SELECT_EXACT);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Set widget at bottom of widget list.
 */

void
dlg_set_bottom_widget (void *w)
{
    Widget *widget = WIDGET (w);
    WDialog *h = widget->owner;

    dlg_reorder_widgets (g_list_find (h->widgets, widget), FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/** Try to select previous widget in the tab order */

void
dlg_one_up (WDialog * h)
{
    if (h->widgets != NULL)
        do_select_widget (h, dlg_widget_prev (h, h->current), SELECT_PREV);
}

/* --------------------------------------------------------------------------------------------- */
/** Try to select next widget in the tab order */

void
dlg_one_down (WDialog * h)
{
    if (h->widgets != NULL)
        do_select_widget (h, dlg_widget_next (h, h->current), SELECT_NEXT);
}

/* --------------------------------------------------------------------------------------------- */

void
update_cursor (WDialog * h)
{
    GList *p = h->current;

    if (p != NULL && widget_get_state (WIDGET (h), WST_ACTIVE))
    {
        Widget *w;

        w = WIDGET (p->data);

        if (!widget_get_state (w, WST_DISABLED) && widget_get_options (w, WOP_WANT_CURSOR))
            send_message (w, NULL, MSG_CURSOR, 0, NULL);
        else
            do
            {
                p = dlg_widget_next (h, p);
                if (p == h->current)
                    break;

                w = WIDGET (p->data);

                if (!widget_get_state (w, WST_DISABLED) && widget_get_options (w, WOP_WANT_CURSOR)
                    && send_message (w, NULL, MSG_CURSOR, 0, NULL) == MSG_HANDLED)
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
dlg_redraw (WDialog * h)
{
    if (!widget_get_state (WIDGET (h), WST_ACTIVE))
        return;

    if (h->winch_pending)
    {
        h->winch_pending = FALSE;
        send_message (h, NULL, MSG_RESIZE, 0, NULL);
    }

    send_message (h, NULL, MSG_DRAW, 0, NULL);
    dlg_broadcast_msg (h, MSG_DRAW);
    update_cursor (h);
}

/* --------------------------------------------------------------------------------------------- */

void
dlg_stop (WDialog * h)
{
    widget_set_state (WIDGET (h), WST_CLOSED, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
/** Init the process */

void
dlg_init (WDialog * h)
{
    Widget *wh = WIDGET (h);

    if (top_dlg != NULL && widget_get_state (WIDGET (top_dlg->data), WST_MODAL))
        widget_set_state (wh, WST_MODAL, TRUE);

    /* add dialog to the stack */
    top_dlg = g_list_prepend (top_dlg, h);

    /* Initialize dialog manager and widgets */
    if (widget_get_state (wh, WST_CONSTRUCT))
    {
        if (!widget_get_state (wh, WST_MODAL))
            dialog_switch_add (h);

        send_message (h, NULL, MSG_INIT, 0, NULL);
        dlg_broadcast_msg (h, MSG_INIT);
        dlg_read_history (h);
    }

    widget_set_state (wh, WST_ACTIVE, TRUE);

    /* first send MSG_DRAW to dialog itself and all widgets... */
    dlg_redraw (h);

    /* ...then send MSG_FOCUS to select the first widget that can take focus */
    while (h->current != NULL && !dlg_focus (h))
        h->current = dlg_widget_next (h, h->current);


    h->ret_value = 0;
}

/* --------------------------------------------------------------------------------------------- */

void
dlg_process_event (WDialog * h, int key, Gpm_Event * event)
{
    if (key == EV_NONE)
    {
        if (tty_got_interrupt ())
            if (send_message (h, NULL, MSG_ACTION, CK_Cancel, NULL) != MSG_HANDLED)
                dlg_execute_cmd (h, CK_Cancel);
    }
    else if (key == EV_MOUSE)
        h->mouse_status = dlg_mouse_event (h, event);
    else
        dlg_key_event (h, key);
}

/* --------------------------------------------------------------------------------------------- */
/** Shutdown the dlg_run */

void
dlg_run_done (WDialog * h)
{
    top_dlg = g_list_remove (top_dlg, h);

    if (widget_get_state (WIDGET (h), WST_CLOSED))
    {
        send_message (h, h->current->data, MSG_END, 0, NULL);
        if (!widget_get_state (WIDGET (h), WST_MODAL))
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
dlg_run (WDialog * h)
{
    dlg_init (h);
    frontend_dlg_run (h);
    dlg_run_done (h);
    return h->ret_value;
}

/* --------------------------------------------------------------------------------------------- */

void
dlg_destroy (WDialog * h)
{
    /* if some widgets have history, save all history at one moment here */
    dlg_save_history (h);
    dlg_broadcast_msg (h, MSG_DESTROY);
    g_list_free_full (h->widgets, g_free);
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
dlg_save_history (WDialog * h)
{
    char *profile;
    int i;

    if (num_history_items_recorded == 0)        /* this is how to disable */
        return;

    profile = mc_config_get_full_path (MC_HISTORY_FILE);
    i = open (profile, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (i != -1)
        close (i);

    /* Make sure the history is only readable by the user */
    if (chmod (profile, S_IRUSR | S_IWUSR) != -1 || errno == ENOENT)
    {
        ev_history_load_save_t event_data;

        event_data.cfg = mc_config_init (profile, FALSE);
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
dlg_get_title (const WDialog * h, size_t len)
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
