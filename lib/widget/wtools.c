/*
   Widget based utility functions.

   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011
   The Free Software Foundation, Inc.

   Authors:
   Miguel de Icaza, 1994, 1995, 1996
   Radek Doulik, 1994, 1995
   Jakub Jelinek, 1995
   Andrej Borsenkow, 1995
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

/** \file wtools.c
 *  \brief Source: widget based utility functions
 */

#include <config.h>

#include <stdarg.h>
#include <stdlib.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* tty_getch() */
#include "lib/strutil.h"
#include "lib/util.h"           /* tilde_expand() */
#include "lib/widget.h"
#include "lib/event.h"          /* mc_event_raise() */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static Dlg_head *last_query_dlg;

static int sel_pos = 0;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** default query callback, used to reposition query */

static cb_ret_t
default_query_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case DLG_RESIZE:
        if ((h->flags & DLG_CENTER) == 0)
        {
            Dlg_head *prev_dlg = NULL;
            int ypos, xpos;

            /* get dialog under h */
            if (top_dlg != NULL)
            {
                if (top_dlg->data != (void *) h)
                    prev_dlg = (Dlg_head *) top_dlg->data;
                else
                {
                    GList *p;

                    /* Top dialog is current if it is visible.
                       Get previous dialog in stack */
                    p = g_list_next (top_dlg);
                    if (p != NULL)
                        prev_dlg = (Dlg_head *) p->data;
                }
            }

            /* if previous dialog is not fullscreen'd -- overlap it */
            if (prev_dlg == NULL || prev_dlg->fullscreen)
                ypos = LINES / 3 - (h->lines - 3) / 2;
            else
                ypos = prev_dlg->y + 2;

            xpos = COLS / 2 - h->cols / 2;

            /* set position */
            dlg_set_position (h, ypos, xpos, ypos + h->lines, xpos + h->cols);

            return MSG_HANDLED;
        }
        /* fallthrough */

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Create message dialog */

static struct Dlg_head *
do_create_message (int flags, const char *title, const char *text)
{
    char *p;
    Dlg_head *d;

    /* Add empty lines before and after the message */
    p = g_strconcat ("\n", text, "\n", (char *) NULL);
    query_dialog (title, p, flags, 0);
    d = last_query_dlg;

    /* do resize before initing and running */
    default_query_callback (d, NULL, DLG_RESIZE, 0, NULL);

    init_dlg (d);
    g_free (p);

    return d;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Show message dialog.  Dismiss it when any key is pressed.
 * Not safe to call from background.
 */

static void
fg_message (int flags, const char *title, const char *text)
{
    Dlg_head *d;

    d = do_create_message (flags, title, text);
    tty_getch ();
    dlg_run_done (d);
    destroy_dlg (d);
}


/* --------------------------------------------------------------------------------------------- */
/** Show message box from background */

#ifdef WITH_BACKGROUND
static void
bg_message (int dummy, int *flags, char *title, const char *text)
{
    (void) dummy;
    title = g_strconcat (_("Background process:"), " ", title, (char *) NULL);
    fg_message (*flags, title, text);
    g_free (title);
}
#endif /* WITH_BACKGROUND */

/* --------------------------------------------------------------------------------------------- */

/**
 * Show dialog, not background safe.
 *
 * If the arguments "header" and "text" should be translated,
 * that MUST be done by the caller of fg_input_dialog_help().
 *
 * The argument "history_name" holds the name of a section
 * in the history file. Data entered in the input field of
 * the dialog box will be stored there.
 *
 */
static char *
fg_input_dialog_help (const char *header, const char *text, const char *help,
                      const char *history_name, const char *def_text)
{
    char *my_str;

    QuickWidget quick_widgets[] = {
        /* 0 */ QUICK_BUTTON (6, 64, 1, 0, N_("&Cancel"), B_CANCEL, NULL),
        /* 1 */ QUICK_BUTTON (3, 64, 1, 0, N_("&OK"), B_ENTER, NULL),
        /* 2 */ QUICK_INPUT (3, 64, 0, 0, def_text, 58, 0, NULL, &my_str),
        /* 3 */ QUICK_LABEL (3, 64, 2, 0, ""),
        QUICK_END
    };

    int b0_len, b1_len, b_len, gap;
    char histname[64] = "inp|";
    int lines, cols;
    int len;
    int i;
    char *p_text;
    int ret;

    /* buttons */
#ifdef ENABLE_NLS
    quick_widgets[0].u.button.text = _(quick_widgets[0].u.button.text);
    quick_widgets[1].u.button.text = _(quick_widgets[1].u.button.text);
#endif /* ENABLE_NLS */

    b0_len = str_term_width1 (quick_widgets[0].u.button.text) + 3;
    b1_len = str_term_width1 (quick_widgets[1].u.button.text) + 5;      /* default button */
    b_len = b0_len + b1_len + 2;        /* including gap */

    /* input line */
    if (history_name != NULL && *history_name != '\0')
    {
        g_strlcpy (histname + 3, history_name, sizeof (histname) - 3);
        quick_widgets[2].u.input.histname = histname;
    }

    /* The special value of def_text is used to identify password boxes
       and hide characters with "*".  Don't save passwords in history! */
    if (def_text == INPUT_PASSWORD)
    {
        quick_widgets[2].u.input.flags = 1;
        histname[3] = '\0';
        quick_widgets[2].u.input.text = "";
    }

    /* text */
    p_text = g_strstrip (g_strdup (text));
    str_msg_term_size (p_text, &lines, &cols);
    quick_widgets[3].u.label.text = p_text;

    /* dialog width */
    len = str_term_width1 (header);
    len = max (max (len, cols) + 4, 64);
    len = min (max (len, b_len + 6), COLS);

    /* button locations */
    gap = (len - 8 - b_len) / 3;
    quick_widgets[1].relative_x = 3 + gap;
    quick_widgets[0].relative_x = quick_widgets[1].relative_x + b1_len + gap + 2;

    {
        QuickDialog Quick_input = {
            len, lines + 6, -1, -1, header,
            help, quick_widgets, NULL, TRUE
        };

        for (i = 0; i < 4; i++)
        {
            quick_widgets[i].x_divisions = Quick_input.xlen;
            quick_widgets[i].y_divisions = Quick_input.ylen;
        }

        for (i = 0; i < 3; i++)
            quick_widgets[i].relative_y += 2 + lines;

        /* input line length */
        quick_widgets[2].u.input.len = Quick_input.xlen - 6;

        ret = quick_dialog (&Quick_input);
    }

    g_free (p_text);

    return (ret != B_CANCEL) ? my_str : NULL;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef WITH_BACKGROUND
static int
wtools_parent_call (void *routine, gpointer ctx, int argc, ...)
{
    ev_background_parent_call_t event_data;

    event_data.routine = routine;
    event_data.ctx = ctx;
    event_data.argc = argc;
    va_start (event_data.ap, argc);
    mc_event_raise (MCEVENT_GROUP_CORE, "background_parent_call", (gpointer) & event_data);
    va_end (event_data.ap);
    return event_data.ret.i;
}

/* --------------------------------------------------------------------------------------------- */

static char *
wtools_parent_call_string (void *routine, int argc, ...)
{
    ev_background_parent_call_t event_data;

    event_data.routine = routine;
    event_data.argc = argc;
    va_start (event_data.ap, argc);
    mc_event_raise (MCEVENT_GROUP_CORE, "background_parent_call_string", (gpointer) & event_data);
    va_end (event_data.ap);
    return event_data.ret.s;
}
#endif /* WITH_BACKGROUND */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Used to ask questions to the user */
int
query_dialog (const char *header, const char *text, int flags, int count, ...)
{
    va_list ap;
    Dlg_head *query_dlg;
    WButton *button;
    WButton *defbutton = NULL;
    int win_len = 0;
    int i;
    int result = -1;
    int cols, lines;
    char *cur_name;
    const int *query_colors = (flags & D_ERROR) != 0 ? alarm_colors : dialog_colors;
    dlg_flags_t dlg_flags = (flags & D_CENTER) != 0 ? (DLG_CENTER | DLG_TRYUP) : DLG_NONE;

    if (header == MSG_ERROR)
        header = _("Error");

    if (count > 0)
    {
        va_start (ap, count);
        for (i = 0; i < count; i++)
        {
            char *cp = va_arg (ap, char *);
            win_len += str_term_width1 (cp) + 6;
            if (strchr (cp, '&') != NULL)
                win_len--;
        }
        va_end (ap);
    }

    /* count coordinates */
    str_msg_term_size (text, &lines, &cols);
    cols = 6 + max (win_len, max (str_term_width1 (header), cols));
    lines += 4 + (count > 0 ? 2 : 0);

    /* prepare dialog */
    query_dlg =
        create_dlg (TRUE, 0, 0, lines, cols, query_colors, default_query_callback,
                    "[QueryBox]", header, dlg_flags);

    if (count > 0)
    {
        cols = (cols - win_len - 2) / 2 + 2;
        va_start (ap, count);
        for (i = 0; i < count; i++)
        {
            int xpos;

            cur_name = va_arg (ap, char *);
            xpos = str_term_width1 (cur_name) + 6;
            if (strchr (cur_name, '&') != NULL)
                xpos--;

            button = button_new (lines - 3, cols, B_USER + i, NORMAL_BUTTON, cur_name, 0);
            add_widget (query_dlg, button);
            cols += xpos;
            if (i == sel_pos)
                defbutton = button;
        }
        va_end (ap);

        add_widget (query_dlg, label_new (2, 3, text));

        /* do resize before running and selecting any widget */
        default_query_callback (query_dlg, NULL, DLG_RESIZE, 0, NULL);

        if (defbutton)
            dlg_select_widget (defbutton);

        /* run dialog and make result */
        switch (run_dlg (query_dlg))
        {
        case B_CANCEL:
            break;
        default:
            result = query_dlg->ret_value - B_USER;
        }

        /* free used memory */
        destroy_dlg (query_dlg);
    }
    else
    {
        add_widget (query_dlg, label_new (2, 3, text));
        add_widget (query_dlg, button_new (0, 0, 0, HIDDEN_BUTTON, "-", 0));
        last_query_dlg = query_dlg;
    }
    sel_pos = 0;
    return result;
}

/* --------------------------------------------------------------------------------------------- */

void
query_set_sel (int new_sel)
{
    sel_pos = new_sel;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Create message dialog.  The caller must call dlg_run_done() and
 * destroy_dlg() to dismiss it.  Not safe to call from background.
 */

struct Dlg_head *
create_message (int flags, const char *title, const char *text, ...)
{
    va_list args;
    Dlg_head *d;
    char *p;

    va_start (args, text);
    p = g_strdup_vprintf (text, args);
    va_end (args);

    d = do_create_message (flags, title, p);
    g_free (p);

    return d;
}

/* --------------------------------------------------------------------------------------------- */
/** Show message box, background safe */

void
message (int flags, const char *title, const char *text, ...)
{
    char *p;
    va_list ap;

    va_start (ap, text);
    p = g_strdup_vprintf (text, ap);
    va_end (ap);

    if (title == MSG_ERROR)
        title = _("Error");

#ifdef WITH_BACKGROUND
    if (mc_global.we_are_background)
    {
        union
        {
            void *p;
            void (*f) (int, int *, char *, const char *);
        } func;
        func.f = bg_message;

        wtools_parent_call (func.p, NULL, 3, sizeof (flags), &flags, strlen (title), title,
                            strlen (p), p);
    }
    else
#endif /* WITH_BACKGROUND */
        fg_message (flags, title, p);

    g_free (p);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Show input dialog, background safe.
 *
 * If the arguments "header" and "text" should be translated,
 * that MUST be done by the caller of these wrappers.
 */

char *
input_dialog_help (const char *header, const char *text, const char *help,
                   const char *history_name, const char *def_text)
{
#ifdef WITH_BACKGROUND
    if (mc_global.we_are_background)
    {
        union
        {
            void *p;
            char *(*f) (const char *, const char *, const char *, const char *, const char *);
        } func;
        func.f = fg_input_dialog_help;
        return wtools_parent_call_string (func.p, 5,
                                          strlen (header), header, strlen (text),
                                          text, strlen (help), help,
                                          strlen (history_name), history_name,
                                          strlen (def_text), def_text);
    }
    else
#endif /* WITH_BACKGROUND */
        return fg_input_dialog_help (header, text, help, history_name, def_text);
}

/* --------------------------------------------------------------------------------------------- */
/** Show input dialog with default help, background safe */

char *
input_dialog (const char *header, const char *text, const char *history_name, const char *def_text)
{
    return input_dialog_help (header, text, "[Input Line Keys]", history_name, def_text);
}

/* --------------------------------------------------------------------------------------------- */

char *
input_expand_dialog (const char *header, const char *text,
                     const char *history_name, const char *def_text)
{
    char *result;
    char *expanded;

    result = input_dialog (header, text, history_name, def_text);
    if (result)
    {
        expanded = tilde_expand (result);
        g_free (result);
        return expanded;
    }
    return result;
}

/* --------------------------------------------------------------------------------------------- */
