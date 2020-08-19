/*
   External panelize

   Copyright (C) 1995-2020
   Free Software Foundation, Inc.

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
#include "filemanager.h"        /* current_panel */
#include "layout.h"             /* rotate_dash() */
#include "panel.h"              /* WPanel */

#include "panelize.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define UX 3
#define UY 2

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

    WGroup *g;

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
    panelize_cols = MAX (panelize_cols, blen + 4);

    panelize_dlg =
        dlg_create (TRUE, 0, 0, 20, panelize_cols, WPOS_CENTER, FALSE, dialog_colors,
                    panelize_callback, NULL, "[External panelize]", _("External panelize"));
    g = GROUP (panelize_dlg);

    /* add listbox to the dialogs */
    y = UY;
    group_add_widget (g, groupbox_new (y++, UX, 12, panelize_cols - UX * 2, ""));

    l_panelize = listbox_new (y, UX + 1, 10, panelize_cols - UX * 2 - 2, FALSE, NULL);
    for (current = panelize; current != NULL; current = current->next)
        listbox_add_item (l_panelize, LISTBOX_APPEND_AT_END, 0, current->label, current, FALSE);
    listbox_select_entry (l_panelize, listbox_search_text (l_panelize, _("Other command")));
    group_add_widget (g, l_panelize);

    y += WIDGET (l_panelize)->lines + 1;
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
panelize_done (void)
{
    dlg_destroy (panelize_dlg);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

static void
add2panelize (char *label, char *command)
{
    struct panelize *current;
    struct panelize *old = NULL;

    current = panelize;
    while (current != NULL && strcmp (current->label, label) <= 0)
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
    if (!input_is_empty (pname))
    {
        char *label;

        label = input_dialog (_("Add to external panelize"),
                              _("Enter command label:"), MC_HISTORY_FM_PANELIZE_ADD, "",
                              INPUT_COMPLETE_NONE);
        if (label == NULL || *label == '\0')
            g_free (label);
        else
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
            panelize = panelize->next;
        else
        {
            struct panelize *current = panelize;

            while (current != NULL && current->next != entry)
                current = current->next;

            if (current != NULL)
                current->next = entry->next;
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
    dir_list *list = &current_panel->dir;
    FILE *external;

    open_error_pipe ();
    external = popen (command, "r");
    if (external == NULL)
    {
        close_error_pipe (D_ERROR, _("Cannot invoke command."));
        return;
    }
    /* Clear the counters and the directory list */
    panel_clean_dir (current_panel);

    panelize_change_root (current_panel->cwd_vpath);

    dir_list_init (list);

    while (TRUE)
    {
        char line[MC_MAXPATHLEN];
        size_t len;
        char *name;
        gboolean link_to_dir, stale_link;
        struct stat st;

        clearerr (external);
        if (fgets (line, sizeof (line), external) == NULL)
        {
            if (ferror (external) != 0 && errno == EINTR)
                continue;
            break;
        }

        len = strlen (line);
        if (line[len - 1] == '\n')
            line[len - 1] = '\0';
        if (line[0] == '\0')
            continue;

        name = line;
        if (line[0] == '.' && IS_PATH_SEP (line[1]))
            name += 2;

        if (!handle_path (name, &st, &link_to_dir, &stale_link))
            continue;

        if (!dir_list_append (list, name, &st, link_to_dir, stale_link))
            break;

        file_mark (current_panel, list->len - 1, 0);

        if ((list->len & 31) == 0)
            rotate_dash (TRUE);
    }

    current_panel->is_panelized = TRUE;
    panelize_absolutize_if_needed (current_panel);

    if (pclose (external) < 0)
        message (D_NORMAL, _("External panelize"), _("Pipe close failed"));
    close_error_pipe (D_NORMAL, NULL);
    try_to_select (current_panel, NULL);
    panel_re_sort (current_panel);
    rotate_dash (FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
do_panelize_cd (WPanel * panel)
{
    int i;
    dir_list *list;
    gboolean panelized_same;

    dir_list_clean (&panel->dir);
    if (panelized_panel.root_vpath == NULL)
        panelize_change_root (current_panel->cwd_vpath);

    if (panelized_panel.list.len < 1)
        dir_list_init (&panelized_panel.list);
    else if (panelized_panel.list.len > panel->dir.size)
        dir_list_grow (&panel->dir, panelized_panel.list.len - panel->dir.size);

    list = &panel->dir;
    list->len = panelized_panel.list.len;

    panelized_same = vfs_path_equal (panelized_panel.root_vpath, panel->cwd_vpath);

    for (i = 0; i < panelized_panel.list.len; i++)
    {
        if (panelized_same || DIR_IS_DOTDOT (panelized_panel.list.list[i].fname))
        {
            list->list[i].fnamelen = panelized_panel.list.list[i].fnamelen;
            list->list[i].fname = g_strndup (panelized_panel.list.list[i].fname,
                                             panelized_panel.list.list[i].fnamelen);
        }
        else
        {
            vfs_path_t *tmp_vpath;
            const char *fname;

            tmp_vpath =
                vfs_path_append_new (panelized_panel.root_vpath, panelized_panel.list.list[i].fname,
                                     (char *) NULL);
            fname = vfs_path_as_str (tmp_vpath);
            list->list[i].fnamelen = strlen (fname);
            list->list[i].fname = g_strndup (fname, list->list[i].fnamelen);
            vfs_path_free (tmp_vpath);
        }
        list->list[i].f.link_to_dir = panelized_panel.list.list[i].f.link_to_dir;
        list->list[i].f.stale_link = panelized_panel.list.list[i].f.stale_link;
        list->list[i].f.dir_size_computed = panelized_panel.list.list[i].f.dir_size_computed;
        list->list[i].f.marked = panelized_panel.list.list[i].f.marked;
        list->list[i].st = panelized_panel.list.list[i].st;
        list->list[i].sort_key = panelized_panel.list.list[i].sort_key;
        list->list[i].second_sort_key = panelized_panel.list.list[i].second_sort_key;
    }

    panel->is_panelized = TRUE;
    panelize_absolutize_if_needed (panel);

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
panelize_save_panel (WPanel * panel)
{
    int i;
    dir_list *list = &panel->dir;

    panelize_change_root (current_panel->cwd_vpath);

    if (panelized_panel.list.len > 0)
        dir_list_clean (&panelized_panel.list);
    if (panel->dir.len == 0)
        return;

    if (panel->dir.len > panelized_panel.list.size)
        dir_list_grow (&panelized_panel.list, panel->dir.len - panelized_panel.list.size);
    panelized_panel.list.len = panel->dir.len;

    for (i = 0; i < panel->dir.len; i++)
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

/**
 * Conditionally switches a panel's directory to "/" (root).
 *
 * If a panelized panel's listing contain absolute paths, this function
 * sets the panel's directory to "/". Otherwise it does nothing.
 *
 * Rationale:
 *
 * This makes tokenized strings like "%d/%p" work. This also makes other
 * places work where such naive concatenation is done in code (e.g., when
 * pressing ctrl+shift+enter, for CK_PutCurrentFullSelected).
 *
 * When to call:
 *
 * You should always call this function after you populate the listing
 * of a panelized panel.
 */
void
panelize_absolutize_if_needed (WPanel * panel)
{
    const dir_list *const list = &panel->dir;

    /* Note: We don't support mixing of absolute and relative paths, which is
     * why it's ok for us to check only the 1st entry. */
    if (list->len > 1 && g_path_is_absolute (list->list[1].fname))
    {
        vfs_path_t *root;

        root = vfs_path_from_str (PATH_SEP_STR);
        panel_set_cwd (panel, root);
        if (panel == current_panel)
            mc_chdir (root);
        vfs_path_free (root);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
cd_panelize_cmd (void)
{
    if (!SELECTED_IS_PANEL)
        create_panel (MENU_PANEL_IDX, view_listing);

    do_panelize_cd (PANEL (get_panel_widget (MENU_PANEL_IDX)));
}

/* --------------------------------------------------------------------------------------------- */

void
external_panelize (void)
{
    if (!vfs_current_is_local ())
    {
        message (D_ERROR, MSG_ERROR, _("Cannot run external panelize in a non-local directory"));
        return;
    }

    init_panelize ();

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
            struct panelize *entry;

            listbox_get_current (l_panelize, NULL, (void **) &entry);
            remove_from_panelize (entry);
            break;
        }

    case B_ENTER:
        if (!input_is_empty (pname))
        {
            char *cmd;

            cmd = g_strdup (pname->buffer);
            dlg_destroy (panelize_dlg);
            do_external_panelize (cmd);
            g_free (cmd);
            repaint_screen ();
            return;
        }
        break;

    default:
        break;
    }

    panelize_done ();
}

/* --------------------------------------------------------------------------------------------- */

void
load_panelize (void)
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
save_panelize (void)
{
    struct panelize *current;

    mc_config_del_group (mc_global.main_config, panelize_section);

    for (current = panelize; current != NULL; current = current->next)
        if (strcmp (current->label, _("Other command")) != 0)
            mc_config_set_string (mc_global.main_config,
                                  panelize_section, current->label, current->command);
}

/* --------------------------------------------------------------------------------------------- */

void
done_panelize (void)
{
    struct panelize *current, *next;

    for (current = panelize; current != NULL; current = next)
    {
        next = current->next;
        g_free (current->label);
        g_free (current->command);
        g_free (current);
    }
}

/* --------------------------------------------------------------------------------------------- */
