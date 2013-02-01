/*
   External panelize

   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2009, 2011, 2013
   The Free Software Foundation, Inc.

   Written by:
   Janne Kukonlehto, 1995
   Jakub Jelinek, 1995

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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"

#include "lib/skin.h"
#include "lib/vfs/vfs.h"
#include "lib/mcconfig.h"       /* Load/save directories panelize */
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "src/setup.h"          /* For profile_bname */
#include "src/history.h"

#include "dir.h"
#include "midnight.h"           /* current_panel */
#include "panel.h"              /* WPanel */

#include "panelize.h"
#include "panel.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define UX 3
#define UY 2

#define LABELS   3
#define B_ADD    B_USER
#define B_REMOVE (B_USER + 1)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static WListbox *l_panelize;
static WDialog *panelize_dlg;
static int last_listitem;
static WInput *pname;

static const char *panelize_section = "Panelize";

/* Directory panelize */
static struct panelize
{
    char *command;
    char *label;
    struct panelize *next;
} *panelize = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
update_command (void)
{
    if (l_panelize->pos != last_listitem)
    {
        struct panelize *data = NULL;

        last_listitem = l_panelize->pos;
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
    case MSG_POST_KEY:
    case MSG_FOCUS:
        tty_setcolor (MENU_ENTRY_COLOR);
        update_command ();
        return MSG_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
init_panelize (void)
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

    size_t i;
    int blen;
    int panelize_cols;
    struct panelize *current;
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
    panelize_cols = max (panelize_cols, blen + 4);

    panelize_dlg =
        create_dlg (TRUE, 0, 0, 20, panelize_cols, dialog_colors, panelize_callback, NULL,
                    "[External panelize]", _("External panelize"), DLG_CENTER);

    /* add listbox to the dialogs */
    y = UY;
    add_widget (panelize_dlg, groupbox_new (y++, UX, 12, panelize_cols - UX * 2, ""));

    l_panelize = listbox_new (y, UX + 1, 10, panelize_cols - UX * 2 - 2, FALSE, NULL);
    for (current = panelize; current != NULL; current = current->next)
        listbox_add_item (l_panelize, LISTBOX_APPEND_AT_END, 0, current->label, current);
    listbox_select_entry (l_panelize, listbox_search_text (l_panelize, _("Other command")));
    add_widget (panelize_dlg, l_panelize);

    y += WIDGET (l_panelize)->lines + 1;
    add_widget (panelize_dlg, label_new (y++, UX, _("Command")));
    pname =
        input_new (y++, UX, input_get_default_colors (), panelize_cols - UX * 2, "", "in",
                   INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_HOSTNAMES | INPUT_COMPLETE_COMMANDS |
                   INPUT_COMPLETE_VARIABLES | INPUT_COMPLETE_USERNAMES | INPUT_COMPLETE_CD |
                   INPUT_COMPLETE_SHELL_ESC);
    add_widget (panelize_dlg, pname);



    add_widget (panelize_dlg, hline_new (y++, -1, -1));

    x = (panelize_cols - blen) / 2;
    for (i = 0; i < G_N_ELEMENTS (panelize_but); i++)
    {
        WButton *b;

        b = button_new (y, x,
                        panelize_but[i].ret_cmd, panelize_but[i].flags, panelize_but[i].text, NULL);
        add_widget (panelize_dlg, b);

        x += button_get_len (b) + 1;
    }

    dlg_select_widget (l_panelize);
}

/* --------------------------------------------------------------------------------------------- */

static void
panelize_done (void)
{
    destroy_dlg (panelize_dlg);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

static void
add2panelize (char *label, char *command)
{
    struct panelize *current, *old;

    old = NULL;
    current = panelize;
    while (current && strcmp (current->label, label) <= 0)
    {
        old = current;
        current = current->next;
    }

    if (old == NULL)
    {
        panelize = g_new (struct panelize, 1);
        panelize->label = label;
        panelize->command = command;
        panelize->next = current;
    }
    else
    {
        struct panelize *new;
        new = g_new (struct panelize, 1);
        new->label = label;
        new->command = command;
        old->next = new;
        new->next = current;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
add2panelize_cmd (void)
{
    char *label;

    if (pname->buffer && (*pname->buffer))
    {
        label = input_dialog (_("Add to external panelize"),
                              _("Enter command label:"), MC_HISTORY_FM_PANELIZE_ADD, "",
                              INPUT_COMPLETE_NONE);
        if (!label)
            return;
        if (!*label)
        {
            g_free (label);
            return;
        }

        add2panelize (label, g_strdup (pname->buffer));
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
remove_from_panelize (struct panelize *entry)
{
    if (strcmp (entry->label, _("Other command")) != 0)
    {
        if (entry == panelize)
        {
            panelize = panelize->next;
        }
        else
        {
            struct panelize *current = panelize;
            while (current && current->next != entry)
                current = current->next;
            if (current)
            {
                current->next = entry->next;
            }
        }

        g_free (entry->label);
        g_free (entry->command);
        g_free (entry);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
do_external_panelize (char *command)
{
    int status, link_to_dir, stale_link;
    int next_free = 0;
    struct stat st;
    dir_list *list = &current_panel->dir;
    char line[MC_MAXPATHLEN];
    char *name;
    FILE *external;

    open_error_pipe ();
    external = popen (command, "r");
    if (!external)
    {
        close_error_pipe (D_ERROR, _("Cannot invoke command."));
        return;
    }
    /* Clear the counters and the directory list */
    panel_clean_dir (current_panel);

    panelize_change_root (current_panel->cwd_vpath);

    if (set_zero_dir (list))
        next_free++;

    while (1)
    {
        clearerr (external);
        if (fgets (line, MC_MAXPATHLEN, external) == NULL)
        {
            if (ferror (external) && errno == EINTR)
                continue;
            else
                break;
        }
        if (line[strlen (line) - 1] == '\n')
            line[strlen (line) - 1] = 0;
        if (strlen (line) < 1)
            continue;
        if (line[0] == '.' && line[1] == PATH_SEP)
            name = line + 2;
        else
            name = line;
        status = handle_path (list, name, &st, next_free, &link_to_dir, &stale_link);
        if (status == 0)
            continue;
        if (status == -1)
            break;
        list->list[next_free].fnamelen = strlen (name);
        list->list[next_free].fname = g_strndup (name, list->list[next_free].fnamelen);
        file_mark (current_panel, next_free, 0);
        list->list[next_free].f.link_to_dir = link_to_dir;
        list->list[next_free].f.stale_link = stale_link;
        list->list[next_free].f.dir_size_computed = 0;
        list->list[next_free].st = st;
        list->list[next_free].sort_key = NULL;
        list->list[next_free].second_sort_key = NULL;
        next_free++;
        if (!(next_free & 32))
            rotate_dash ();
    }

    current_panel->is_panelized = TRUE;
    if (next_free)
    {
        current_panel->count = next_free;
        if (list->list[0].fname[0] == PATH_SEP)
        {
            int ret;
            panel_set_cwd (current_panel, PATH_SEP_STR);
            ret = chdir (PATH_SEP_STR);
            (void) ret;
        }
    }
    else
    {
        current_panel->count = set_zero_dir (list) ? 1 : 0;
    }
    if (pclose (external) < 0)
        message (D_NORMAL, _("External panelize"), _("Pipe close failed"));
    close_error_pipe (D_NORMAL, NULL);
    try_to_select (current_panel, NULL);
    panel_re_sort (current_panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
do_panelize_cd (struct WPanel *panel)
{
    int i;
    dir_list *list = &panel->dir;
    gboolean panelized_same;

    clean_dir (list, panel->count);
    if (panelized_panel.root_vpath == NULL)
        panelize_change_root (current_panel->cwd_vpath);

    if (panelized_panel.count < 1)
    {
        if (set_zero_dir (&panelized_panel.list))
            panelized_panel.count = 1;
    }
    else if (panelized_panel.count >= list->size)
    {
        list->list = g_try_realloc (list->list, sizeof (file_entry) * panelized_panel.count);
        list->size = panelized_panel.count;
    }
    panel->count = panelized_panel.count;
    panel->is_panelized = TRUE;

    panelized_same = (vfs_path_equal (panelized_panel.root_vpath, panel->cwd_vpath));

    for (i = 0; i < panelized_panel.count; i++)
    {
        if (panelized_same
            || (panelized_panel.list.list[i].fname[0] == '.'
                && panelized_panel.list.list[i].fname[1] == '.'
                && panelized_panel.list.list[i].fname[2] == '\0'))
        {
            list->list[i].fnamelen = panelized_panel.list.list[i].fnamelen;
            list->list[i].fname = g_strndup (panelized_panel.list.list[i].fname,
                                             panelized_panel.list.list[i].fnamelen);
        }
        else
        {
            vfs_path_t *tmp_vpath;

            tmp_vpath =
                vfs_path_append_new (panelized_panel.root_vpath, panelized_panel.list.list[i].fname,
                                     NULL);
            list->list[i].fname = vfs_path_to_str (tmp_vpath);
            vfs_path_free (tmp_vpath);
            list->list[i].fnamelen = strlen (list->list[i].fname);
        }
        list->list[i].f.link_to_dir = panelized_panel.list.list[i].f.link_to_dir;
        list->list[i].f.stale_link = panelized_panel.list.list[i].f.stale_link;
        list->list[i].f.dir_size_computed = panelized_panel.list.list[i].f.dir_size_computed;
        list->list[i].f.marked = panelized_panel.list.list[i].f.marked;
        list->list[i].st = panelized_panel.list.list[i].st;
        list->list[i].sort_key = panelized_panel.list.list[i].sort_key;
        list->list[i].second_sort_key = panelized_panel.list.list[i].second_sort_key;
    }
    try_to_select (panel, NULL);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Change root directory of panelized content.
 * @param new_root - object with new path.
 */
void
panelize_change_root (const vfs_path_t * new_root)
{
    vfs_path_free (panelized_panel.root_vpath);
    panelized_panel.root_vpath = vfs_path_clone (new_root);
}

/* --------------------------------------------------------------------------------------------- */

void
panelize_save_panel (struct WPanel *panel)
{
    int i;
    dir_list *list = &panel->dir;

    panelize_change_root (current_panel->cwd_vpath);

    if (panelized_panel.count > 0)
        clean_dir (&panelized_panel.list, panelized_panel.count);
    if (panel->count < 1)
        return;

    panelized_panel.count = panel->count;
    if (panel->count >= panelized_panel.list.size)
    {
        panelized_panel.list.list = g_try_realloc (panelized_panel.list.list,
                                                   sizeof (file_entry) * panel->count);
        panelized_panel.list.size = panel->count;
    }
    for (i = 0; i < panel->count; i++)
    {
        panelized_panel.list.list[i].fnamelen = list->list[i].fnamelen;
        panelized_panel.list.list[i].fname =
            g_strndup (list->list[i].fname, list->list[i].fnamelen);
        panelized_panel.list.list[i].f.link_to_dir = list->list[i].f.link_to_dir;
        panelized_panel.list.list[i].f.stale_link = list->list[i].f.stale_link;
        panelized_panel.list.list[i].f.dir_size_computed = list->list[i].f.dir_size_computed;
        panelized_panel.list.list[i].f.marked = list->list[i].f.marked;
        panelized_panel.list.list[i].st = list->list[i].st;
        panelized_panel.list.list[i].sort_key = list->list[i].sort_key;
        panelized_panel.list.list[i].second_sort_key = list->list[i].second_sort_key;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
cd_panelize_cmd (void)
{
    if (!SELECTED_IS_PANEL)
        set_display_type (MENU_PANEL_IDX, view_listing);

    do_panelize_cd ((struct WPanel *) get_panel_widget (MENU_PANEL_IDX));
}

/* --------------------------------------------------------------------------------------------- */

void
external_panelize (void)
{
    char *target = NULL;

    if (!vfs_current_is_local ())
    {
        message (D_ERROR, MSG_ERROR, _("Cannot run external panelize in a non-local directory"));
        return;
    }

    init_panelize ();

    /* display file info */
    tty_setcolor (SELECTED_COLOR);

    run_dlg (panelize_dlg);

    switch (panelize_dlg->ret_value)
    {
    case B_CANCEL:
        break;

    case B_ADD:
        add2panelize_cmd ();
        break;

    case B_REMOVE:
        {
            struct panelize *entry;

            listbox_get_current (l_panelize, NULL, (void **) &entry);
            remove_from_panelize (entry);
            break;
        }

    case B_ENTER:
        target = pname->buffer;
        if (target != NULL && *target)
        {
            char *cmd = g_strdup (target);
            destroy_dlg (panelize_dlg);
            do_external_panelize (cmd);
            g_free (cmd);
            repaint_screen ();
            return;
        }
        break;
    }

    panelize_done ();
}

/* --------------------------------------------------------------------------------------------- */

void
load_panelize (void)
{
    gchar **profile_keys, **keys;
    gsize len;
    GIConv conv;

    conv = str_crt_conv_from ("UTF-8");

    profile_keys = keys = mc_config_get_keys (mc_main_config, panelize_section, &len);

    add2panelize (g_strdup (_("Other command")), g_strdup (""));

    if (!profile_keys || *profile_keys == NULL)
    {
        add2panelize (g_strdup (_("Modified git files")), g_strdup ("git ls-files --modified"));
        add2panelize (g_strdup (_("Find rejects after patching")),
                      g_strdup ("find . -name \\*.rej -print"));
        add2panelize (g_strdup (_("Find *.orig after patching")),
                      g_strdup ("find . -name \\*.orig -print"));
        add2panelize (g_strdup (_("Find SUID and SGID programs")),
                      g_strdup
                      ("find . \\( \\( -perm -04000 -a -perm +011 \\) -o \\( -perm -02000 -a -perm +01 \\) \\) -print"));
        return;
    }

    while (*profile_keys)
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
                      mc_config_get_string (mc_main_config, panelize_section, *profile_keys, ""));
        profile_keys++;
    }

    g_strfreev (keys);
    str_close_conv (conv);
}

/* --------------------------------------------------------------------------------------------- */

void
save_panelize (void)
{
    struct panelize *current = panelize;

    mc_config_del_group (mc_main_config, panelize_section);
    for (; current; current = current->next)
    {
        if (strcmp (current->label, _("Other command")))
            mc_config_set_string (mc_main_config,
                                  panelize_section, current->label, current->command);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
done_panelize (void)
{
    struct panelize *current = panelize;
    struct panelize *next;

    for (; current; current = next)
    {
        next = current->next;
        g_free (current->label);
        g_free (current->command);
        g_free (current);
    }
}

/* --------------------------------------------------------------------------------------------- */
