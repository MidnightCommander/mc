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

/** \file history.c
 *  \brief Source: save, load and show history
 */

#include <config.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lib/global.h"

#include "lib/tty/tty.h"        /* LINES, COLS */
#include "lib/mcconfig.h"       /* for history loading and saving */
#include "lib/fileloc.h"
#include "lib/strutil.h"
#include "lib/util.h"           /* list_append_unique */
#include "lib/widget.h"

/*** global variables ****************************************************************************/

/* how much history items are used */
int num_history_items_recorded = 60;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct
{
    Widget *widget;
    size_t count;
    size_t maxlen;
} history_dlg_data;

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static cb_ret_t
history_dlg_reposition (Dlg_head * dlg_head)
{
    history_dlg_data *data;
    int x = 0, y, he, wi;

    /* guard checks */
    if ((dlg_head == NULL) || (dlg_head->data == NULL))
        return MSG_NOT_HANDLED;

    data = (history_dlg_data *) dlg_head->data;

    y = data->widget->y;
    he = data->count + 2;

    if (he <= y || y > (LINES - 6))
    {
        he = min (he, y - 1);
        y -= he;
    }
    else
    {
        y++;
        he = min (he, LINES - y);
    }

    if (data->widget->x > 2)
        x = data->widget->x - 2;

    wi = data->maxlen + 4;

    if ((wi + x) > COLS)
    {
        wi = min (wi, COLS);
        x = COLS - wi;
    }

    dlg_set_position (dlg_head, y, x, y + he, x + wi);

    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
history_dlg_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case DLG_RESIZE:
        return history_dlg_reposition (h);

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Load the history from the ${XDG_CACHE_HOME}/mc/history file.
 * It is called with the widgets history name and returns the GList list.
 */

GList *
history_get (const char *input_name)
{
    GList *hist = NULL;
    char *profile;
    mc_config_t *cfg;

    if (num_history_items_recorded == 0)        /* this is how to disable */
        return NULL;
    if ((input_name == NULL) || (*input_name == '\0'))
        return NULL;

    profile = g_build_filename (mc_config_get_cache_path (), MC_HISTORY_FILE, NULL);
    cfg = mc_config_init (profile);

    hist = history_load (cfg, input_name);

    mc_config_deinit (cfg);
    g_free (profile);

    return hist;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Load history form the mc_config
 */
GList *
history_load (struct mc_config_t * cfg, const char *name)
{
    size_t i;
    GList *hist = NULL;
    char **keys;
    size_t keys_num = 0;
    GIConv conv = INVALID_CONV;
    GString *buffer;

    if (name == NULL || *name == '\0')
        return NULL;

    /* get number of keys */
    keys = mc_config_get_keys (cfg, name, &keys_num);
    g_strfreev (keys);

    /* create charset conversion handler to convert strings
       from utf-8 to system codepage */
    if (!mc_global.utf8_display)
        conv = str_crt_conv_from ("UTF-8");

    buffer = g_string_sized_new (64);

    for (i = 0; i < keys_num; i++)
    {
        char key[BUF_TINY];
        char *this_entry;

        g_snprintf (key, sizeof (key), "%lu", (unsigned long) i);
        this_entry = mc_config_get_string_raw (cfg, name, key, "");

        if (this_entry == NULL)
            continue;

        if (conv == INVALID_CONV)
            hist = list_append_unique (hist, this_entry);
        else
        {
            g_string_set_size (buffer, 0);
            if (str_convert (conv, this_entry, buffer) == ESTR_FAILURE)
                hist = list_append_unique (hist, this_entry);
            else
            {
                hist = list_append_unique (hist, g_strdup (buffer->str));
                g_free (this_entry);
            }
        }
    }

    g_string_free (buffer, TRUE);
    if (conv != INVALID_CONV)
        str_close_conv (conv);

    /* return pointer to the last entry in the list */
    return g_list_last (hist);
}

/* --------------------------------------------------------------------------------------------- */

/**
  * Save history to the mc_config, but don't save config to file
  */
void
history_save (struct mc_config_t *cfg, const char *name, GList * h)
{
    GIConv conv = INVALID_CONV;
    GString *buffer;
    int i;

    if (name == NULL || *name == '\0' || h == NULL)
        return;

    /* go to end of list */
    h = g_list_last (h);

    /* go back 60 places */
    for (i = 0; (i < num_history_items_recorded - 1) && (h->prev != NULL); i++)
        h = g_list_previous (h);

    if (name != NULL)
        mc_config_del_group (cfg, name);

    /* create charset conversion handler to convert strings
       from system codepage to UTF-8 */
    if (!mc_global.utf8_display)
        conv = str_crt_conv_to ("UTF-8");

    buffer = g_string_sized_new (64);
    /* dump history into profile */
    for (i = 0; h != NULL; h = g_list_next (h))
    {
        char key[BUF_TINY];
        char *text = (char *) h->data;

        /* We shouldn't have null entries, but let's be sure */
        if (text == NULL)
            continue;

        g_snprintf (key, sizeof (key), "%d", i++);

        if (conv == INVALID_CONV)
            mc_config_set_string_raw (cfg, name, key, text);
        else
        {
            g_string_set_size (buffer, 0);
            if (str_convert (conv, text, buffer) == ESTR_FAILURE)
                mc_config_set_string_raw (cfg, name, key, text);
            else
                mc_config_set_string_raw (cfg, name, key, buffer->str);
        }
    }

    g_string_free (buffer, TRUE);
    if (conv != INVALID_CONV)
        str_close_conv (conv);
}

/* --------------------------------------------------------------------------------------------- */

#if 0
/**
  * Write the history to the ${XDG_CACHE_HOME}/mc/history file.
 */
void
history_put (const char *input_name, GList * h)
{
    char *profile;
    int i;

    if (num_history_items_recorded == 0)        /* this is how to disable */
        return;
    if ((input_name == NULL) || (*input_name == '\0'))
        return;
    if (h == NULL)
        return;

    profile = g_build_filename (mc_config_get_cache_path (), MC_HISTORY_FILE, (char *) NULL);

    i = open (profile, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (i != -1)
        close (i);

    /* Make sure the history is only readable by the user */
    if (chmod (profile, S_IRUSR | S_IWUSR) != -1 || errno == ENOENT)
    {
        mc_config_t *cfg;

        cfg = mc_config_init (profile);
        history_save (cfg, input_name, h);
        mc_config_save_file (cfg, NULL);
        mc_config_deinit (cfg);
    }

    g_free (profile);
}
#endif

/* --------------------------------------------------------------------------------------------- */

char *
history_show (GList ** history, Widget * widget)
{
    GList *z, *hlist = NULL, *hi;
    size_t maxlen, i, count = 0;
    char *r = NULL;
    Dlg_head *query_dlg;
    WListbox *query_list;
    history_dlg_data hist_data;

    if (*history == NULL)
        return NULL;

    maxlen = str_term_width1 (_("History")) + 2;

    for (z = *history; z != NULL; z = g_list_previous (z))
    {
        WLEntry *entry;

        i = str_term_width1 ((char *) z->data);
        maxlen = max (maxlen, i);
        count++;

        entry = g_new0 (WLEntry, 1);
        /* history is being reverted here */
        entry->text = g_strdup ((char *) z->data);
        hlist = g_list_prepend (hlist, entry);
    }

    hist_data.widget = widget;
    hist_data.count = count;
    hist_data.maxlen = maxlen;

    query_dlg =
        create_dlg (TRUE, 0, 0, 4, 4, dialog_colors, history_dlg_callback,
                    "[History-query]", _("History"), DLG_COMPACT);
    query_dlg->data = &hist_data;

    query_list = listbox_new (1, 1, 2, 2, TRUE, NULL);

    /* this call makes list stick to all sides of dialog, effectively make
       it be resized with dialog */
    add_widget_autopos (query_dlg, query_list, WPOS_KEEP_ALL, NULL);

    /* to avoid diplicating of (calculating sizes in two places)
       code, call dlg_hist_callback function here, to set dialog and
       controls positions.
       The main idea - create 4x4 dialog and add 2x2 list in
       center of it, and let dialog function resize it to needed
       size. */
    history_dlg_callback (query_dlg, NULL, DLG_RESIZE, 0, NULL);

    if (query_dlg->y < widget->y)
    {
        /* draw list entries from bottom upto top */
        listbox_set_list (query_list, hlist);
        listbox_select_last (query_list);
    }
    else
    {
        /* draw list entries from top downto bottom */
        /* revert history direction */
        hlist = g_list_reverse (hlist);
        listbox_set_list (query_list, hlist);
    }

    if (run_dlg (query_dlg) != B_CANCEL)
    {
        char *q;

        listbox_get_current (query_list, &q, NULL);
        r = g_strdup (q);
    }

    /* get modified history from dialog */
    z = NULL;
    for (hi = query_list->list; hi != NULL; hi = g_list_next (hi))
    {
        WLEntry *entry;

        entry = (WLEntry *) hi->data;
        /* history is being reverted here again */
        z = g_list_prepend (z, entry->text);
        entry->text = NULL;
    }

    /* restore history direction */
    if (query_dlg->y < widget->y)
        z = g_list_reverse (z);

    destroy_dlg (query_dlg);

    g_list_foreach (*history, (GFunc) g_free, NULL);
    g_list_free (*history);
    *history = g_list_last (z);

    return r;
}

/* --------------------------------------------------------------------------------------------- */
