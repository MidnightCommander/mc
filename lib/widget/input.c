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

/** \file input.c
 *  \brief Source: WInput widget
 */

#include <config.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/mouse.h"
#include "lib/tty/key.h"        /* XCTRL and ALT macros  */
#include "lib/fileloc.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/keybind.h"        /* global_keymap_t */
#include "lib/widget.h"
#include "lib/event.h"          /* mc_event_raise() */

#include "input_complete.h"

/*** global variables ****************************************************************************/

int quote = 0;

const global_keymap_t *input_map = NULL;

/*** file scope macro definitions ****************************************************************/

#define LARGE_HISTORY_BUTTON 1

#ifdef LARGE_HISTORY_BUTTON
#define HISTORY_BUTTON_WIDTH 3
#else
#define HISTORY_BUTTON_WIDTH 1
#endif

#define should_show_history_button(in) \
    (in->history != NULL && in->field_width > HISTORY_BUTTON_WIDTH * 2 + 1 \
         && WIDGET (in)->owner != NULL)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* Input widgets have a global kill ring */
/* Pointer to killed data */
static char *kill_buffer = NULL;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static size_t
get_history_length (const GList * history)
{
    size_t len = 0;

    for (; history != NULL; history = g_list_previous (history))
        len++;

    return len;
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_history_button (WInput * in)
{
    char c;
    gboolean disabled = (WIDGET (in)->options & W_DISABLED) != 0;

    if (g_list_next (in->history_current) == NULL)
        c = '^';
    else if (g_list_previous (in->history_current) == NULL)
        c = 'v';
    else
        c = '|';

    widget_move (in, 0, in->field_width - HISTORY_BUTTON_WIDTH);
    tty_setcolor (disabled ? DISABLED_COLOR : in->color[WINPUTC_HISTORY]);

#ifdef LARGE_HISTORY_BUTTON
    tty_print_string ("[ ]");
    widget_move (in, 0, in->field_width - HISTORY_BUTTON_WIDTH + 1);
#endif

    tty_print_char (c);
}

/* --------------------------------------------------------------------------------------------- */

static void
input_set_markers (WInput * in, long m1)
{
    in->mark = m1;
}

/* --------------------------------------------------------------------------------------------- */

static void
input_mark_cmd (WInput * in, gboolean mark)
{
    if (mark == 0)
    {
        in->highlight = FALSE;
        input_set_markers (in, 0);
    }
    else
    {
        in->highlight = TRUE;
        input_set_markers (in, in->point);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
input_eval_marks (WInput * in, long *start_mark, long *end_mark)
{
    if (in->highlight)
    {
        *start_mark = min (in->mark, in->point);
        *end_mark = max (in->mark, in->point);
        return TRUE;
    }
    else
    {
        *start_mark = *end_mark = 0;
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
delete_region (WInput * in, int x_first, int x_last)
{
    int first = min (x_first, x_last);
    int last = max (x_first, x_last);
    size_t len;

    input_mark_cmd (in, FALSE);
    in->point = first;
    last = str_offset_to_pos (in->buffer, last);
    first = str_offset_to_pos (in->buffer, first);
    len = strlen (&in->buffer[last]) + 1;
    memmove (&in->buffer[first], &in->buffer[last], len);
    in->charpoint = 0;
    in->need_push = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
do_show_hist (WInput * in)
{
    size_t len;
    char *r;

    len = get_history_length (in->history);

    r = history_show (&in->history, WIDGET (in),
                      g_list_position (in->history_current, in->history));
    if (r != NULL)
    {
        input_assign_text (in, r);
        g_free (r);
    }

    /* Has history cleaned up or not? */
    if (len != get_history_length (in->history))
        in->history_changed = TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Strip password from incomplete url (just user:pass@host without VFS prefix).
 *
 * @param url partial URL
 * @return newly allocated string without password
 */

static char *
input_history_strip_password (char *url)
{
    char *at, *delim, *colon;

    at = strrchr (url, '@');
    if (at == NULL)
        return g_strdup (url);

    /* TODO: handle ':' and '@' in password */

    delim = strstr (url, VFS_PATH_URL_DELIMITER);
    if (delim != NULL)
        colon = strchr (delim + strlen (VFS_PATH_URL_DELIMITER), ':');
    else
        colon = strchr (url, ':');

    /* if 'colon' before 'at', 'colon' delimits user and password: user:password@host */
    /* if 'colon' after 'at', 'colon' delimits host and port: user@host:port */
    if (colon != NULL && colon > at)
        colon = NULL;

    if (colon == NULL)
        return g_strdup (url);
    *colon = '\0';

    return g_strconcat (url, at, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
push_history (WInput * in, const char *text)
{
    char *t;
    gboolean empty;

    if (text == NULL)
        return;

    t = g_strstrip (g_strdup (text));
    empty = *t == '\0';
    g_free (t);
    t = g_strdup (empty ? "" : text);

    if (!empty && in->history_name != NULL && in->strip_password)
    {
        /*
           We got string user:pass@host without any VFS prefixes
           and vfs_path_to_str_flags (t, VPF_STRIP_PASSWORD) doesn't work.
           Therefore we want to strip password in separate algorithm
         */
        char *url_with_stripped_password;

        url_with_stripped_password = input_history_strip_password (t);
        g_free (t);
        t = url_with_stripped_password;
    }

    if (in->history == NULL || in->history->data == NULL || strcmp (in->history->data, t) != 0 ||
        in->history_changed)
    {
        in->history = list_append_unique (in->history, t);
        in->history_current = in->history;
        in->history_changed = TRUE;
    }
    else
        g_free (t);

    in->need_push = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
move_buffer_backward (WInput * in, int start, int end)
{
    int i, pos, len;
    int str_len;

    str_len = str_length (in->buffer);
    if (start >= str_len || end > str_len + 1)
        return;

    pos = str_offset_to_pos (in->buffer, start);
    len = str_offset_to_pos (in->buffer, end) - pos;

    for (i = pos; in->buffer[i + len - 1]; i++)
        in->buffer[i] = in->buffer[i + len];
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
insert_char (WInput * in, int c_code)
{
    size_t i;
    int res;

    if (in->highlight)
    {
        long m1, m2;
        if (input_eval_marks (in, &m1, &m2))
            delete_region (in, m1, m2);
    }
    if (c_code == -1)
        return MSG_NOT_HANDLED;

    if (in->charpoint >= MB_LEN_MAX)
        return MSG_HANDLED;

    in->charbuf[in->charpoint] = c_code;
    in->charpoint++;

    res = str_is_valid_char (in->charbuf, in->charpoint);
    if (res < 0)
    {
        if (res != -2)
            in->charpoint = 0;  /* broken multibyte char, skip */
        return MSG_HANDLED;
    }

    in->need_push = TRUE;
    if (strlen (in->buffer) + 1 + in->charpoint >= in->current_max_size)
    {
        /* Expand the buffer */
        size_t new_length = in->current_max_size + in->field_width + in->charpoint;
        char *narea = g_try_renew (char, in->buffer, new_length);
        if (narea)
        {
            in->buffer = narea;
            in->current_max_size = new_length;
        }
    }

    if (strlen (in->buffer) + in->charpoint < in->current_max_size)
    {
        /* bytes from begin */
        size_t ins_point = str_offset_to_pos (in->buffer, in->point);
        /* move chars */
        size_t rest_bytes = strlen (in->buffer + ins_point);

        for (i = rest_bytes + 1; i > 0; i--)
            in->buffer[ins_point + i + in->charpoint - 1] = in->buffer[ins_point + i - 1];

        memcpy (in->buffer + ins_point, in->charbuf, in->charpoint);
        in->point++;
    }

    in->charpoint = 0;
    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static void
beginning_of_line (WInput * in)
{
    in->point = 0;
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
end_of_line (WInput * in)
{
    in->point = str_length (in->buffer);
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
backward_char (WInput * in)
{
    const char *act;

    act = in->buffer + str_offset_to_pos (in->buffer, in->point);
    if (in->point > 0)
        in->point -= str_cprev_noncomb_char (&act, in->buffer);
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
forward_char (WInput * in)
{
    const char *act;

    act = in->buffer + str_offset_to_pos (in->buffer, in->point);
    if (act[0] != '\0')
        in->point += str_cnext_noncomb_char (&act);
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
forward_word (WInput * in)
{
    const char *p;

    p = in->buffer + str_offset_to_pos (in->buffer, in->point);
    while (p[0] != '\0' && (str_isspace (p) || str_ispunct (p)))
    {
        str_cnext_char (&p);
        in->point++;
    }
    while (p[0] != '\0' && !str_isspace (p) && !str_ispunct (p))
    {
        str_cnext_char (&p);
        in->point++;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
backward_word (WInput * in)
{
    const char *p, *p_tmp;

    for (p = in->buffer + str_offset_to_pos (in->buffer, in->point);
         (p != in->buffer) && (p[0] == '\0'); str_cprev_char (&p), in->point--);

    while (p != in->buffer)
    {
        p_tmp = p;
        str_cprev_char (&p);
        if (!str_isspace (p) && !str_ispunct (p))
        {
            p = p_tmp;
            break;
        }
        in->point--;
    }
    while (p != in->buffer)
    {
        str_cprev_char (&p);
        if (str_isspace (p) || str_ispunct (p))
            break;

        in->point--;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
backward_delete (WInput * in)
{
    const char *act = in->buffer + str_offset_to_pos (in->buffer, in->point);
    int start;

    if (in->point == 0)
        return;

    start = in->point - str_cprev_noncomb_char (&act, in->buffer);
    move_buffer_backward (in, start, in->point);
    in->charpoint = 0;
    in->need_push = TRUE;
    in->point = start;
}

/* --------------------------------------------------------------------------------------------- */

static void
delete_char (WInput * in)
{
    const char *act;
    int end = in->point;

    act = in->buffer + str_offset_to_pos (in->buffer, in->point);
    end += str_cnext_noncomb_char (&act);

    move_buffer_backward (in, in->point, end);
    in->charpoint = 0;
    in->need_push = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
copy_region (WInput * in, int x_first, int x_last)
{
    int first = min (x_first, x_last);
    int last = max (x_first, x_last);

    if (last == first)
    {
        /* Copy selected files to clipboard */
        mc_event_raise (MCEVENT_GROUP_FILEMANAGER, "panel_save_curent_file_to_clip_file", NULL);
        /* try use external clipboard utility */
        mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_file_to_ext_clip", NULL);
        return;
    }

    g_free (kill_buffer);

    first = str_offset_to_pos (in->buffer, first);
    last = str_offset_to_pos (in->buffer, last);

    kill_buffer = g_strndup (in->buffer + first, last - first);

    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_text_to_file", kill_buffer);
    /* try use external clipboard utility */
    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_file_to_ext_clip", NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
kill_word (WInput * in)
{
    int old_point = in->point;
    int new_point;

    forward_word (in);
    new_point = in->point;
    in->point = old_point;

    delete_region (in, old_point, new_point);
    in->need_push = TRUE;
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
back_kill_word (WInput * in)
{
    int old_point = in->point;
    int new_point;

    backward_word (in);
    new_point = in->point;
    in->point = old_point;

    delete_region (in, old_point, new_point);
    in->need_push = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
yank (WInput * in)
{
    if (kill_buffer != NULL)
    {
        char *p;

        in->charpoint = 0;
        for (p = kill_buffer; *p != '\0'; p++)
            insert_char (in, *p);
        in->charpoint = 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
kill_line (WInput * in)
{
    int chp;

    chp = str_offset_to_pos (in->buffer, in->point);
    g_free (kill_buffer);
    kill_buffer = g_strdup (&in->buffer[chp]);
    in->buffer[chp] = '\0';
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
clear_line (WInput * in)
{
    in->need_push = TRUE;
    in->buffer[0] = '\0';
    in->point = 0;
    in->mark = 0;
    in->highlight = FALSE;
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
ins_from_clip (WInput * in)
{
    char *p = NULL;
    ev_clipboard_text_from_file_t event_data;

    /* try use external clipboard utility */
    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_file_from_ext_clip", NULL);

    event_data.text = &p;
    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_text_from_file", &event_data);
    if (event_data.ret)
    {
        char *pp;

        for (pp = p; *pp != '\0'; pp++)
            insert_char (in, *pp);

        g_free (p);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
hist_prev (WInput * in)
{
    GList *prev;

    if (in->history == NULL)
        return;

    if (in->need_push)
        push_history (in, in->buffer);

    prev = g_list_previous (in->history_current);
    if (prev != NULL)
    {
        input_assign_text (in, (char *) prev->data);
        in->history_current = prev;
        in->history_changed = TRUE;
        in->need_push = FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
hist_next (WInput * in)
{
    GList *next;

    if (in->need_push)
    {
        push_history (in, in->buffer);
        input_assign_text (in, "");
        return;
    }

    if (in->history == NULL)
        return;

    next = g_list_next (in->history_current);
    if (next == NULL)
    {
        input_assign_text (in, "");
        in->history_current = in->history;
    }
    else
    {
        input_assign_text (in, (char *) next->data);
        in->history_current = next;
        in->history_changed = TRUE;
        in->need_push = FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
port_region_marked_for_delete (WInput * in)
{
    in->buffer[0] = '\0';
    in->point = 0;
    in->first = FALSE;
    in->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
input_execute_cmd (WInput * in, unsigned long command)
{
    cb_ret_t res = MSG_HANDLED;

    /* a highlight command like shift-arrow */
    if (command == CK_MarkLeft || command == CK_MarkRight ||
        command == CK_MarkToWordBegin || command == CK_MarkToWordEnd ||
        command == CK_MarkToHome || command == CK_MarkToEnd)
    {
        if (!in->highlight)
        {
            input_mark_cmd (in, FALSE); /* clear */
            input_mark_cmd (in, TRUE);  /* marking on */
        }
    }

    switch (command)
    {
    case CK_WordRight:
    case CK_WordLeft:
    case CK_Right:
    case CK_Left:
        if (in->highlight)
            input_mark_cmd (in, FALSE);
    }

    switch (command)
    {
    case CK_Home:
    case CK_MarkToHome:
        beginning_of_line (in);
        break;
    case CK_End:
    case CK_MarkToEnd:
        end_of_line (in);
        break;
    case CK_Left:
    case CK_MarkLeft:
        backward_char (in);
        break;
    case CK_WordLeft:
    case CK_MarkToWordBegin:
        backward_word (in);
        break;
    case CK_Right:
    case CK_MarkRight:
        forward_char (in);
        break;
    case CK_WordRight:
    case CK_MarkToWordEnd:
        forward_word (in);
        break;
    case CK_BackSpace:
        if (in->highlight)
        {
            long m1, m2;
            if (input_eval_marks (in, &m1, &m2))
                delete_region (in, m1, m2);
        }
        else
            backward_delete (in);
        break;
    case CK_Delete:
        if (in->first)
            port_region_marked_for_delete (in);
        else if (in->highlight)
        {
            long m1, m2;
            if (input_eval_marks (in, &m1, &m2))
                delete_region (in, m1, m2);
        }
        else
            delete_char (in);
        break;
    case CK_DeleteToWordEnd:
        kill_word (in);
        break;
    case CK_DeleteToWordBegin:
        back_kill_word (in);
        break;
    case CK_Mark:
        input_mark_cmd (in, TRUE);
        break;
    case CK_Remove:
        delete_region (in, in->point, in->mark);
        break;
    case CK_DeleteToEnd:
        kill_line (in);
        break;
    case CK_Clear:
        clear_line (in);
        break;
    case CK_Store:
        copy_region (in, in->mark, in->point);
        break;
    case CK_Cut:
        copy_region (in, in->mark, in->point);
        delete_region (in, in->point, in->mark);
        break;
    case CK_Yank:
        yank (in);
        break;
    case CK_Paste:
        ins_from_clip (in);
        break;
    case CK_HistoryPrev:
        hist_prev (in);
        break;
    case CK_HistoryNext:
        hist_next (in);
        break;
    case CK_History:
        do_show_hist (in);
        break;
    case CK_Complete:
        complete (in);
        break;
    default:
        res = MSG_NOT_HANDLED;
    }

    if (command != CK_MarkLeft && command != CK_MarkRight &&
        command != CK_MarkToWordBegin && command != CK_MarkToWordEnd &&
        command != CK_MarkToHome && command != CK_MarkToEnd)
        in->highlight = FALSE;

    return res;
}

/* --------------------------------------------------------------------------------------------- */

/* "history_load" event handler */
static gboolean
input_load_history (const gchar * event_group_name, const gchar * event_name,
                    gpointer init_data, gpointer data)
{
    WInput *in = INPUT (init_data);
    ev_history_load_save_t *ev = (ev_history_load_save_t *) data;
    const char *def_text;
    size_t buffer_len;

    (void) event_group_name;
    (void) event_name;

    in->history = history_load (ev->cfg, in->history_name);
    in->history_current = in->history;

    if (in->init_text == NULL)
        def_text = "";
    else if (in->init_text == INPUT_LAST_TEXT)
    {
        if (in->history != NULL && in->history->data != NULL)
            def_text = (const char *) in->history->data;
        else
            def_text = "";

        in->init_text = NULL;
    }
    else
        def_text = in->init_text;

    buffer_len = strlen (def_text);
    buffer_len = 1 + max ((size_t) in->field_width, buffer_len);
    in->current_max_size = buffer_len;
    if (buffer_len > (size_t) in->field_width)
        in->buffer = g_realloc (in->buffer, buffer_len);
    strcpy (in->buffer, def_text);
    in->point = str_length (in->buffer);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* "history_save" event handler */
static gboolean
input_save_history (const gchar * event_group_name, const gchar * event_name,
                    gpointer init_data, gpointer data)
{
    WInput *in = INPUT (init_data);

    (void) event_group_name;
    (void) event_name;

    if (!in->is_password && (WIDGET (in)->owner->ret_value != B_CANCEL))
    {
        ev_history_load_save_t *ev = (ev_history_load_save_t *) data;

        push_history (in, in->buffer);
        if (in->history_changed)
            history_save (ev->cfg, in->history_name, in->history);
        in->history_changed = FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
input_destroy (WInput * in)
{
    if (in == NULL)
    {
        fprintf (stderr, "Internal error: null Input *\n");
        exit (EXIT_FAILURE);
    }

    input_free_completions (in);

    /* clean history */
    if (in->history != NULL)
    {
        /* history is already saved before this moment */
        in->history = g_list_first (in->history);
        g_list_foreach (in->history, (GFunc) g_free, NULL);
        g_list_free (in->history);
    }
    g_free (in->history_name);

    g_free (in->buffer);
    input_free_completions (in);
    g_free (in->init_text);

    g_free (kill_buffer);
    kill_buffer = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static int
input_event (Gpm_Event * event, void *data)
{
    WInput *in = INPUT (data);
    Widget *w = WIDGET (data);

    if (!mouse_global_in_widget (event, w))
        return MOU_UNHANDLED;

    if ((event->type & GPM_DOWN) != 0)
    {
        in->first = FALSE;
        input_mark_cmd (in, FALSE);
    }

    if ((event->type & (GPM_DOWN | GPM_DRAG)) != 0)
    {
        Gpm_Event local;

        local = mouse_get_local (event, w);

        dlg_select_widget (w);

        if (local.x >= in->field_width - HISTORY_BUTTON_WIDTH + 1
            && should_show_history_button (in))
            do_show_hist (in);
        else
        {
            in->point = str_length (in->buffer);
            if (local.x + in->term_first_shown - 1 < str_term_width1 (in->buffer))
                in->point = str_column_to_pos (in->buffer, local.x + in->term_first_shown - 1);
        }

        input_update (in, TRUE);
    }

    /* A lone up mustn't do anything */
    if (in->highlight && (event->type & (GPM_UP | GPM_DRAG)) != 0)
        return MOU_NORMAL;

    if ((event->type & GPM_DRAG) == 0)
        input_mark_cmd (in, TRUE);

    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Callback for applying new options to input widget.
 *
 * @param w       widget
 * @param options options set
 * @param enable  TRUE if specified options should be added, FALSE if options should be removed
 */
static void
input_set_options_callback (Widget * w, widget_options_t options, gboolean enable)
{
    WInput *in = INPUT (w);

    widget_default_set_options_callback (w, options, enable);
    if (in->label != NULL)
        widget_set_options (WIDGET (in->label), options, enable);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Create new instance of WInput object.
  * @param y                    Y coordinate
  * @param x                    X coordinate
  * @param input_colors         Array of used colors
  * @param width                Widget width
  * @param def_text             Default text filled in widget
  * @param histname             Name of history
  * @param completion_flags     Flags for specify type of completions
  * @return                     WInput object
  */
WInput *
input_new (int y, int x, const int *input_colors, int width, const char *def_text,
           const char *histname, input_complete_t completion_flags)
{
    WInput *in;
    Widget *w;

    in = g_new (WInput, 1);
    w = WIDGET (in);
    init_widget (w, y, x, 1, width, input_callback, input_event);
    w->options |= W_IS_INPUT;
    w->set_options = input_set_options_callback;

    memmove (in->color, input_colors, sizeof (input_colors_t));

    in->field_width = width;
    in->first = TRUE;
    in->highlight = FALSE;
    in->term_first_shown = 0;
    in->disable_update = 0;
    in->mark = 0;
    in->need_push = TRUE;
    in->is_password = FALSE;
    in->charpoint = 0;
    in->strip_password = FALSE;

    /* in->buffer will be corrected in "history_load" event handler */
    in->current_max_size = width + 1;
    in->buffer = g_new0 (char, in->current_max_size);
    in->point = 0;

    in->init_text = (def_text == INPUT_LAST_TEXT) ? INPUT_LAST_TEXT : g_strdup (def_text);

    in->completions = NULL;
    in->completion_flags = completion_flags;

    /* prepare to history setup */
    in->history = NULL;
    in->history_current = NULL;
    in->history_changed = FALSE;
    in->history_name = NULL;
    if ((histname != NULL) && (*histname != '\0'))
        in->history_name = g_strdup (histname);
    /* history will be loaded later */

    in->label = NULL;

    return in;
}

/* --------------------------------------------------------------------------------------------- */

cb_ret_t
input_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WInput *in = INPUT (w);
    cb_ret_t v;

    switch (msg)
    {
    case MSG_INIT:
        /* subscribe to "history_load" event */
        mc_event_add (w->owner->event_group, MCEVENT_HISTORY_LOAD, input_load_history, w, NULL);
        /* subscribe to "history_save" event */
        mc_event_add (w->owner->event_group, MCEVENT_HISTORY_SAVE, input_save_history, w, NULL);
        return MSG_HANDLED;

    case MSG_KEY:
        if (parm == XCTRL ('q'))
        {
            quote = 1;
            v = input_handle_char (in, ascii_alpha_to_cntrl (tty_getch ()));
            quote = 0;
            return v;
        }

        /* Keys we want others to handle */
        if (parm == KEY_UP || parm == KEY_DOWN || parm == ESC_CHAR
            || parm == KEY_F (10) || parm == '\n')
            return MSG_NOT_HANDLED;

        /* When pasting multiline text, insert literal Enter */
        if ((parm & ~KEY_M_MASK) == '\n')
        {
            quote = 1;
            v = input_handle_char (in, '\n');
            quote = 0;
            return v;
        }

        return input_handle_char (in, parm);

    case MSG_ACTION:
        return input_execute_cmd (in, parm);

    case MSG_FOCUS:
    case MSG_UNFOCUS:
    case MSG_DRAW:
    case MSG_RESIZE:
        input_update (in, FALSE);
        return MSG_HANDLED;

    case MSG_CURSOR:
        widget_move (in, 0, str_term_width2 (in->buffer, in->point) - in->term_first_shown);
        return MSG_HANDLED;

    case MSG_DESTROY:
        /* unsubscribe from "history_load" event */
        mc_event_del (w->owner->event_group, MCEVENT_HISTORY_LOAD, input_load_history, w);
        /* unsubscribe from "history_save" event */
        mc_event_del (w->owner->event_group, MCEVENT_HISTORY_SAVE, input_save_history, w);
        input_destroy (in);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

/** Get default colors for WInput widget.
  * @return default colors
  */
const int *
input_get_default_colors (void)
{
    static input_colors_t standart_colors;

    standart_colors[WINPUTC_MAIN] = INPUT_COLOR;
    standart_colors[WINPUTC_MARK] = INPUT_MARK_COLOR;
    standart_colors[WINPUTC_UNCHANGED] = INPUT_UNCHANGED_COLOR;
    standart_colors[WINPUTC_HISTORY] = INPUT_HISTORY_COLOR;

    return standart_colors;
}

/* --------------------------------------------------------------------------------------------- */

void
input_set_origin (WInput * in, int x, int field_width)
{
    WIDGET (in)->x = x;
    in->field_width = WIDGET (in)->cols = field_width;
    input_update (in, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

cb_ret_t
input_handle_char (WInput * in, int key)
{
    cb_ret_t v;
    unsigned long command;

    if (quote != 0)
    {
        input_free_completions (in);
        v = insert_char (in, key);
        input_update (in, TRUE);
        quote = 0;
        return v;
    }

    command = keybind_lookup_keymap_command (input_map, key);

    if (command == CK_IgnoreKey)
    {
        if (key > 255)
            return MSG_NOT_HANDLED;
        if (in->first)
            port_region_marked_for_delete (in);
        input_free_completions (in);
        v = insert_char (in, key);
    }
    else
    {
        if (command != CK_Complete)
            input_free_completions (in);
        input_execute_cmd (in, command);
        v = MSG_HANDLED;
        if (in->first)
            input_update (in, TRUE);    /* needed to clear in->first */
    }

    input_update (in, TRUE);
    return v;
}

/* --------------------------------------------------------------------------------------------- */

/* This function is a test for a special input key used in complete.c */
/* Returns 0 if it is not a special key, 1 if it is a non-complete key
   and 2 if it is a complete key */
int
input_key_is_in_map (WInput * in, int key)
{
    unsigned long command;

    (void) in;

    command = keybind_lookup_keymap_command (input_map, key);
    if (command == CK_IgnoreKey)
        return 0;

    return (command == CK_Complete) ? 2 : 1;
}

/* --------------------------------------------------------------------------------------------- */

void
input_assign_text (WInput * in, const char *text)
{
    input_free_completions (in);
    g_free (in->buffer);
    in->current_max_size = strlen (text) + 1;
    in->buffer = g_strndup (text, in->current_max_size);        /* was in->buffer->text */
    in->point = str_length (in->buffer);
    in->mark = 0;
    in->need_push = TRUE;
    in->charpoint = 0;
    input_update (in, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/* Inserts text in input line */
void
input_insert (WInput * in, const char *text, gboolean insert_extra_space)
{
    input_disable_update (in);
    while (*text != '\0')
        input_handle_char (in, (unsigned char) *text++);        /* unsigned extension char->int */
    if (insert_extra_space)
        input_handle_char (in, ' ');
    input_enable_update (in);
    input_update (in, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
input_set_point (WInput * in, int pos)
{
    int max_pos;

    max_pos = str_length (in->buffer);
    pos = min (pos, max_pos);
    if (pos != in->point)
        input_free_completions (in);
    in->point = pos;
    in->charpoint = 0;
    input_update (in, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
input_update (WInput * in, gboolean clear_first)
{
    Widget *w = WIDGET (in);
    int has_history = 0;
    int i;
    int buf_len;
    const char *cp;
    int pw;

    if (should_show_history_button (in))
        has_history = HISTORY_BUTTON_WIDTH;

    if (in->disable_update != 0)
        return;

    buf_len = str_length (in->buffer);
    pw = str_term_width2 (in->buffer, in->point);

    /* Make the point visible */
    if ((pw < in->term_first_shown) || (pw >= in->term_first_shown + in->field_width - has_history))
    {
        in->term_first_shown = pw - (in->field_width / 3);
        if (in->term_first_shown < 0)
            in->term_first_shown = 0;
    }

    /* Adjust the mark */
    in->mark = min (in->mark, buf_len);

    /* don't draw widget not put into dialog */
    if (w->owner == NULL || w->owner->state != DLG_ACTIVE)
        return;

    if (has_history != 0)
        draw_history_button (in);

    if ((w->options & W_DISABLED) != 0)
        tty_setcolor (DISABLED_COLOR);
    else if (in->first)
        tty_setcolor (in->color[WINPUTC_UNCHANGED]);
    else
        tty_setcolor (in->color[WINPUTC_MAIN]);

    widget_move (in, 0, 0);

    if (!in->is_password)
    {
        if (!in->highlight)
            tty_print_string (str_term_substring (in->buffer, in->term_first_shown,
                                                  in->field_width - has_history));
        else
        {
            long m1, m2;

            if (input_eval_marks (in, &m1, &m2))
            {
                tty_setcolor (in->color[WINPUTC_MAIN]);
                cp = str_term_substring (in->buffer, in->term_first_shown,
                                         in->field_width - has_history);
                tty_print_string (cp);
                tty_setcolor (in->color[WINPUTC_MARK]);
                if (m1 < in->term_first_shown)
                {
                    widget_move (in, 0, 0);
                    tty_print_string (str_term_substring
                                      (in->buffer, in->term_first_shown,
                                       m2 - in->term_first_shown));
                }
                else
                {
                    int sel_width, buf_width;

                    widget_move (in, 0, m1 - in->term_first_shown);
                    buf_width = str_term_width2 (in->buffer, m1);
                    sel_width =
                        min (m2 - m1,
                             (in->field_width - has_history) - (buf_width - in->term_first_shown));
                    tty_print_string (str_term_substring (in->buffer, m1, sel_width));
                }
            }
        }
    }
    else
    {
        cp = str_term_substring (in->buffer, in->term_first_shown, in->field_width - has_history);
        tty_setcolor (in->color[WINPUTC_MAIN]);
        for (i = 0; i < in->field_width - has_history; i++)
        {
            if (i < (buf_len - in->term_first_shown) && cp[0] != '\0')
                tty_print_char ('*');
            else
                tty_print_char (' ');
            if (cp[0] != '\0')
                str_cnext_char (&cp);
        }
    }

    if (clear_first)
        in->first = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
input_enable_update (WInput * in)
{
    in->disable_update--;
    input_update (in, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
input_disable_update (WInput * in)
{
    in->disable_update++;
}

/* --------------------------------------------------------------------------------------------- */

/**
  *  Cleans the input line and adds the current text to the history
  *
  *  @param in the input line
  */
void
input_clean (WInput * in)
{
    push_history (in, in->buffer);
    in->need_push = TRUE;
    in->buffer[0] = '\0';
    in->point = 0;
    in->charpoint = 0;
    in->mark = 0;
    in->highlight = FALSE;
    input_free_completions (in);
    input_update (in, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
input_free_completions (WInput * in)
{
    g_strfreev (in->completions);
    in->completions = NULL;
}

/* --------------------------------------------------------------------------------------------- */
