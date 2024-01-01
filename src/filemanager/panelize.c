/*
   External panelize

   Copyright (C) 1995-2024
   Free Software Foundation, Inc.

   Written by:
   Janne Kukonlehto, 1995
   Jakub Jelinek, 1995
   Andrew Borodin <aborodin@vmail.ru> 2011-2023

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

/** \file panelize.c
 *  \brief Source: External panelization module
 */

#include <config.h>

#include "lib/global.h"

#include "lib/skin.h"
#include "lib/tty/tty.h"
#include "lib/vfs/vfs.h"
#include "lib/mcconfig.h"       /* Load/save directories panelize */
#include "lib/strutil.h"
#include "lib/widget.h"
#include "lib/util.h"           /* mc_pipe_t */

#include "src/history.h"

#include "filemanager.h"        /* current_panel */
#include "layout.h"             /* rotate_dash() */
#include "panel.h"              /* WPanel, dir.h */

#include "panelize.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define UX 3
#define UY 2

#define B_ADD    B_USER
#define B_REMOVE (B_USER + 1)

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *command;
    char *label;
} panelize_entry_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static WListbox *l_panelize;
static WDialog *panelize_dlg;
static int last_listitem;
static WInput *pname;
static GSList *panelize = NULL;

static const char *panelize_section = "Panelize";

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
panelize_entry_free (gpointer data)
{
    panelize_entry_t *entry = (panelize_entry_t *) data;

    g_free (entry->command);
    g_free (entry->label);
    g_free (entry);
}

/* --------------------------------------------------------------------------------------------- */

static int
panelize_entry_cmp_by_label (gconstpointer a, gconstpointer b)
{
    const panelize_entry_t *entry = (const panelize_entry_t *) a;
    const char *label = (const char *) b;

    return strcmp (entry->label, label);
}

/* --------------------------------------------------------------------------------------------- */

static void
panelize_entry_add_to_listbox (gpointer data, gpointer user_data)
{
    panelize_entry_t *entry = (panelize_entry_t *) data;

    (void) user_data;

    listbox_add_item (l_panelize, LISTBOX_APPEND_AT_END, 0, entry->label, entry, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
update_command (void)
{
    if (l_panelize->current != last_listitem)
    {
        panelize_entry_t *data = NULL;

        last_listitem = l_panelize->current;
        listbox_get_current (l_panelize, NULL, (void **) &data);
        input_assign_text (pname, data->command);
        pname->point = 0;
        input_update (pname, TRUE);
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
panelize_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_INIT:
        group_default_callback (w, NULL, MSG_INIT, 0, NULL);
        MC_FALLTHROUGH;

    case MSG_NOTIFY:           /* MSG_NOTIFY is fired by the listbox to tell us the item has changed. */
        update_command ();
        return MSG_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
external_panelize_init (void)
{
    struct
    {
        int ret_cmd;
        button_flags_t flags;
        const char *text;
    } panelize_but[] =
    {
        /* *INDENT-OFF* */
        { B_ENTER, DEFPUSH_BUTTON, N_("Pane&lize") },
        { B_REMOVE, NORMAL_BUTTON, N_("&Remove") },
        { B_ADD, NORMAL_BUTTON, N_("&Add new") },
        { B_CANCEL, NORMAL_BUTTON, N_("&Cancel") }
        /* *INDENT-ON* */
    };

    WGroup *g;

    size_t i;
    int blen;
    int panelize_cols;
    int x, y;

    last_listitem = 0;

    do_refresh ();

    i = G_N_ELEMENTS (panelize_but);
    blen = i - 1;               /* gaps between buttons */
    while (i-- != 0)
    {
#ifdef ENABLE_NLS
        panelize_but[i].text = _(panelize_but[i].text);
#endif
        blen += str_term_width1 (panelize_but[i].text) + 3 + 1;
        if (panelize_but[i].flags == DEFPUSH_BUTTON)
            blen += 2;
    }

    panelize_cols = COLS - 6;
    panelize_cols = MAX (panelize_cols, blen + 4);

    panelize_dlg =
        dlg_create (TRUE, 0, 0, 20, panelize_cols, WPOS_CENTER, FALSE, dialog_colors,
                    panelize_callback, NULL, "[External panelize]", _("External panelize"));
    g = GROUP (panelize_dlg);

    /* add listbox to the dialogs */
    y = UY;
    group_add_widget (g, groupbox_new (y++, UX, 12, panelize_cols - UX * 2, ""));

    l_panelize = listbox_new (y, UX + 1, 10, panelize_cols - UX * 2 - 2, FALSE, NULL);
    g_slist_foreach (panelize, panelize_entry_add_to_listbox, NULL);
    listbox_set_current (l_panelize, listbox_search_text (l_panelize, _("Other command")));
    group_add_widget (g, l_panelize);

    y += WIDGET (l_panelize)->rect.lines + 1;
    group_add_widget (g, label_new (y++, UX, _("Command")));
    pname =
        input_new (y++, UX, input_colors, panelize_cols - UX * 2, "", "in",
                   INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_HOSTNAMES | INPUT_COMPLETE_COMMANDS |
                   INPUT_COMPLETE_VARIABLES | INPUT_COMPLETE_USERNAMES | INPUT_COMPLETE_CD |
                   INPUT_COMPLETE_SHELL_ESC);
    group_add_widget (g, pname);

    group_add_widget (g, hline_new (y++, -1, -1));

    x = (panelize_cols - blen) / 2;
    for (i = 0; i < G_N_ELEMENTS (panelize_but); i++)
    {
        WButton *b;

        b = button_new (y, x,
                        panelize_but[i].ret_cmd, panelize_but[i].flags, panelize_but[i].text, NULL);
        group_add_widget (g, b);

        x += button_get_len (b) + 1;
    }

    widget_select (WIDGET (l_panelize));
}

/* --------------------------------------------------------------------------------------------- */

static void
external_panelize_done (void)
{
    widget_destroy (WIDGET (panelize_dlg));
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

static void
add2panelize (char *label, char *command)
{
    panelize_entry_t *entry;

    entry = g_try_new (panelize_entry_t, 1);
    if (entry != NULL)
    {
        entry->label = label;
        entry->command = command;

        panelize = g_slist_insert_sorted (panelize, entry, panelize_entry_cmp_by_label);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
add2panelize_cmd (void)
{
    if (!input_is_empty (pname))
    {
        char *label;

        label = input_dialog (_("Add to external panelize"),
                              _("Enter command label:"), MC_HISTORY_FM_PANELIZE_ADD, "",
                              INPUT_COMPLETE_NONE);
        if (label == NULL || *label == '\0')
            g_free (label);
        else
            add2panelize (label, input_get_text (pname));
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
remove_from_panelize (panelize_entry_t * entry)
{
    if (strcmp (entry->label, _("Other command")) != 0)
    {
        panelize = g_slist_remove (panelize, entry);
        panelize_entry_free (entry);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
do_external_panelize (const char *command)
{
    dir_list *list = &current_panel->dir;
    mc_pipe_t *external;
    GError *error = NULL;
    GString *remain_file_name = NULL;

    external = mc_popen (command, TRUE, TRUE, &error);
    if (external == NULL)
    {
        message (D_ERROR, _("External panelize"), "%s", error->message);
        g_error_free (error);
        return;
    }

    /* Clear the counters and the directory list */
    panel_clean_dir (current_panel);

    panel_panelize_change_root (current_panel, current_panel->cwd_vpath);

    dir_list_init (list);

    while (TRUE)
    {
        GString *line;
        gboolean ok;

        /* init buffers before call of mc_pread() */
        external->out.len = MC_PIPE_BUFSIZE;
        external->err.len = MC_PIPE_BUFSIZE;
        external->err.null_term = TRUE;

        mc_pread (external, &error);

        if (error != NULL)
        {
            message (D_ERROR, MSG_ERROR, _("External panelize:\n%s"), error->message);
            g_error_free (error);
            break;
        }

        if (external->err.len > 0)
            message (D_ERROR, MSG_ERROR, _("External panelize:\n%s"), external->err.buf);

        if (external->out.len == MC_PIPE_STREAM_EOF)
            break;

        if (external->out.len == 0)
            continue;

        if (external->out.len == MC_PIPE_ERROR_READ)
        {
            message (D_ERROR, MSG_ERROR,
                     _("External panelize:\nfailed to read data from child stdout:\n%s"),
                     unix_error_string (external->out.error));
            break;
        }

        ok = TRUE;

        while (ok && (line = mc_pstream_get_string (&external->out)) != NULL)
        {
            char *name;
            gboolean link_to_dir, stale_link;
            struct stat st;

            /* handle a \n-separated file list */

            if (line->str[line->len - 1] == '\n')
            {
                /* entire file name or last chunk */

                g_string_truncate (line, line->len - 1);

                /* join filename chunks */
                if (remain_file_name != NULL)
                {
                    g_string_append_len (remain_file_name, line->str, line->len);
                    g_string_free (line, TRUE);
                    line = remain_file_name;
                    remain_file_name = NULL;
                }
            }
            else
            {
                /* first or middle chunk of file name */

                if (remain_file_name == NULL)
                    remain_file_name = line;
                else
                {
                    g_string_append_len (remain_file_name, line->str, line->len);
                    g_string_free (line, TRUE);
                }

                continue;
            }

            name = line->str;

            if (name[0] == '.' && IS_PATH_SEP (name[1]))
                name += 2;

            if (handle_path (name, &st, &link_to_dir, &stale_link))
            {
                ok = dir_list_append (list, name, &st, link_to_dir, stale_link);

                if (ok)
                {
                    file_mark (current_panel, list->len - 1, 0);

                    if ((list->len & 31) == 0)
                        rotate_dash (TRUE);
                }
            }

            g_string_free (line, TRUE);
        }
    }

    if (remain_file_name != NULL)
        g_string_free (remain_file_name, TRUE);

    mc_pclose (external, NULL);

    current_panel->is_panelized = TRUE;
    panel_panelize_absolutize_if_needed (current_panel);

    panel_set_current_by_name (current_panel, NULL);
    panel_re_sort (current_panel);
    rotate_dash (FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
external_panelize_cmd (void)
{
    if (!vfs_current_is_local ())
    {
        message (D_ERROR, MSG_ERROR, _("Cannot run external panelize in a non-local directory"));
        return;
    }

    external_panelize_init ();

    /* display file info */
    tty_setcolor (SELECTED_COLOR);

    switch (dlg_run (panelize_dlg))
    {
    case B_CANCEL:
        break;

    case B_ADD:
        add2panelize_cmd ();
        break;

    case B_REMOVE:
        {
            panelize_entry_t *entry;

            listbox_get_current (l_panelize, NULL, (void **) &entry);
            remove_from_panelize (entry);
            break;
        }

    case B_ENTER:
        if (!input_is_empty (pname))
        {
            char *cmd;

            cmd = input_get_text (pname);
            widget_destroy (WIDGET (panelize_dlg));
            do_external_panelize (cmd);
            g_free (cmd);
            repaint_screen ();
            return;
        }
        break;

    default:
        break;
    }

    external_panelize_done ();
}

/* --------------------------------------------------------------------------------------------- */

void
external_panelize_load (void)
{
    char **keys;

    keys = mc_config_get_keys (mc_global.main_config, panelize_section, NULL);

    add2panelize (g_strdup (_("Other command")), g_strdup (""));

    if (*keys == NULL)
    {
        add2panelize (g_strdup (_("Modified git files")), g_strdup ("git ls-files --modified"));
        add2panelize (g_strdup (_("Find rejects after patching")),
                      g_strdup ("find . -name \\*.rej -print"));
        add2panelize (g_strdup (_("Find *.orig after patching")),
                      g_strdup ("find . -name \\*.orig -print"));
        add2panelize (g_strdup (_("Find SUID and SGID programs")),
                      g_strdup
                      ("find . \\( \\( -perm -04000 -a -perm /011 \\) -o \\( -perm -02000 -a -perm /01 \\) \\) -print"));
    }
    else
    {
        GIConv conv;
        char **profile_keys;

        conv = str_crt_conv_from ("UTF-8");

        for (profile_keys = keys; *profile_keys != NULL; profile_keys++)
        {
            GString *buffer;

            if (mc_global.utf8_display || conv == INVALID_CONV)
                buffer = g_string_new (*profile_keys);
            else
            {
                buffer = g_string_new ("");
                if (str_convert (conv, *profile_keys, buffer) == ESTR_FAILURE)
                    g_string_assign (buffer, *profile_keys);
            }

            add2panelize (g_string_free (buffer, FALSE),
                          mc_config_get_string (mc_global.main_config, panelize_section,
                                                *profile_keys, ""));
        }

        str_close_conv (conv);
    }

    g_strfreev (keys);
}

/* --------------------------------------------------------------------------------------------- */

void
external_panelize_save (void)
{
    GSList *l;

    mc_config_del_group (mc_global.main_config, panelize_section);

    for (l = panelize; l != NULL; l = g_slist_next (l))
    {
        panelize_entry_t *current = (panelize_entry_t *) l->data;

        if (strcmp (current->label, _("Other command")) != 0)
            mc_config_set_string (mc_global.main_config,
                                  panelize_section, current->label, current->command);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
external_panelize_free (void)
{
    g_clear_slist (&panelize, panelize_entry_free);
    panelize = NULL;
}

/* --------------------------------------------------------------------------------------------- */
