/*
   Directory tree browser for the Midnight Commander
   This module has been converted to be a widget.

   The program load and saves the tree each time the tree widget is
   created and destroyed.  This is required for the future vfs layer,
   it will be possible to have tree views over virtual file systems.

   Copyright (C) 1994-2024
   Free Software Foundation, Inc.

   Written by:
   Janne Kukonlehto, 1994, 1996
   Norbert Warmuth, 1997
   Miguel de Icaza, 1996, 1999
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2013-2022

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

/** \file tree.c
 *  \brief Source: directory tree browser
 */

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/skin.h"
#include "lib/vfs/vfs.h"
#include "lib/fileloc.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"
#include "lib/event.h"          /* mc_event_raise() */

#include "src/setup.h"          /* confirm_delete, panels_options */
#include "src/keymap.h"
#include "src/history.h"

#include "dir.h"
#include "filemanager.h"        /* the_menubar */
#include "file.h"               /* copy_dir_dir(), move_dir_dir(), erase_dir() */
#include "layout.h"             /* command_prompt */
#include "treestore.h"
#include "cmd.h"
#include "filegui.h"
#include "cd.h"                 /* cd_error_message() */

#include "tree.h"

/*** global variables ****************************************************************************/

/* The pointer to the tree */
WTree *the_tree = NULL;

/* If this is true, then when browsing the tree the other window will
 * automatically reload it's directory with the contents of the currently
 * selected directory.
 */
gboolean xtree_mode = FALSE;

/*** file scope macro definitions ****************************************************************/

#define tlines(t) (t->is_panel ? WIDGET (t)->rect.lines - 2 - \
                    (panels_options.show_mini_info ? 2 : 0) : WIDGET (t)->rect.lines)

/*** file scope type declarations ****************************************************************/

struct WTree
{
    Widget widget;
    struct TreeStore *store;
    tree_entry *selected_ptr;   /* The selected directory */
    GString *search_buffer;     /* Current search string */
    tree_entry **tree_shown;    /* Entries currently on screen */
    gboolean is_panel;          /* panel or plain widget flag */
    gboolean searching;         /* Are we on searching mode? */
    int topdiff;                /* The difference between the topmost
                                   shown and the selected */
};

/*** forward declarations (file scope functions) *************************************************/

static void tree_rescan (void *data);

/*** file scope variables ************************************************************************/

/* Specifies the display mode: 1d or 2d */
static gboolean tree_navigation_flag = FALSE;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static tree_entry *
back_ptr (tree_entry *ptr, int *count)
{
    int i;

    for (i = 0; ptr != NULL && ptr->prev != NULL && i < *count; ptr = ptr->prev, i++)
        ;

    *count = i;
    return ptr;
}

/* --------------------------------------------------------------------------------------------- */

static tree_entry *
forw_ptr (tree_entry *ptr, int *count)
{
    int i;

    for (i = 0; ptr != NULL && ptr->next != NULL && i < *count; ptr = ptr->next, i++)
        ;

    *count = i;
    return ptr;
}

/* --------------------------------------------------------------------------------------------- */

static void
remove_callback (tree_entry *entry, void *data)
{
    WTree *tree = data;

    if (tree->selected_ptr == entry)
    {
        if (tree->selected_ptr->next != NULL)
            tree->selected_ptr = tree->selected_ptr->next;
        else
            tree->selected_ptr = tree->selected_ptr->prev;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Save the ${XDG_CACHE_HOME}/mc/Tree file */

static void
save_tree (WTree *tree)
{
    int error;

    (void) tree;

    error = tree_store_save ();
    if (error != 0)
    {
        char *tree_name;

        tree_name = mc_config_get_full_path (MC_TREESTORE_FILE);
        fprintf (stderr, _("Cannot open the %s file for writing:\n%s\n"), tree_name,
                 unix_error_string (error));
        g_free (tree_name);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_remove_entry (WTree *tree, const vfs_path_t *name_vpath)
{
    (void) tree;
    tree_store_remove_entry (name_vpath);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_destroy (WTree *tree)
{
    tree_store_remove_entry_remove_hook (remove_callback);
    save_tree (tree);

    MC_PTR_FREE (tree->tree_shown);
    g_string_free (tree->search_buffer, TRUE);
    tree->selected_ptr = NULL;
}

/* --------------------------------------------------------------------------------------------- */
/** Loads the .mc.tree file */

static void
load_tree (WTree *tree)
{
    vfs_path_t *vpath;

    tree_store_load ();

    tree->selected_ptr = tree->store->tree_first;
    vpath = vfs_path_from_str (mc_config_get_home_dir ());
    tree_chdir (tree, vpath);
    vfs_path_free (vpath, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_show_mini_info (WTree *tree, int tree_lines, int tree_cols)
{
    Widget *w = WIDGET (tree);
    int line;

    /* Show mini info */
    if (tree->is_panel)
    {
        if (!panels_options.show_mini_info)
            return;
        line = tree_lines + 2;
    }
    else
        line = tree_lines + 1;

    if (tree->searching)
    {
        /* Show search string */
        tty_setcolor (INPUT_COLOR);
        tty_draw_hline (w->rect.y + line, w->rect.x + 1, ' ', tree_cols);
        widget_gotoyx (w, line, 1);
        tty_print_char (PATH_SEP);
        tty_print_string (str_fit_to_term (tree->search_buffer->str, tree_cols - 2, J_LEFT_FIT));
        tty_print_char (' ');
    }
    else
    {
        /* Show full name of selected directory */

        const int *colors;

        colors = widget_get_colors (w);
        tty_setcolor (tree->is_panel ? NORMAL_COLOR : colors[DLG_COLOR_NORMAL]);
        tty_draw_hline (w->rect.y + line, w->rect.x + 1, ' ', tree_cols);
        widget_gotoyx (w, line, 1);
        tty_print_string (str_fit_to_term
                          (vfs_path_as_str (tree->selected_ptr->name), tree_cols, J_LEFT_FIT));
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
show_tree (WTree *tree)
{
    Widget *w = WIDGET (tree);
    tree_entry *current;
    int i, j;
    int topsublevel = 0;
    int x = 0, y = 0;
    int tree_lines, tree_cols;

    /* Initialize */
    tree_lines = tlines (tree);
    tree_cols = w->rect.cols;

    widget_gotoyx (w, y, x);
    if (tree->is_panel)
    {
        tree_cols -= 2;
        x = y = 1;
    }

    g_free (tree->tree_shown);
    tree->tree_shown = g_new0 (tree_entry *, tree_lines);

    if (tree->store->tree_first != NULL)
        topsublevel = tree->store->tree_first->sublevel;

    if (tree->selected_ptr == NULL)
    {
        tree->selected_ptr = tree->store->tree_first;
        tree->topdiff = 0;
    }
    current = tree->selected_ptr;

    /* Calculate the directory which is to be shown on the topmost line */
    if (!tree_navigation_flag)
        current = back_ptr (current, &tree->topdiff);
    else
    {
        i = 0;

        while (current->prev != NULL && i < tree->topdiff)
        {
            current = current->prev;

            if (current->sublevel < tree->selected_ptr->sublevel)
            {
                if (vfs_path_equal (current->name, tree->selected_ptr->name))
                    i++;
            }
            else if (current->sublevel == tree->selected_ptr->sublevel)
            {
                const char *cname;

                cname = vfs_path_as_str (current->name);
                for (j = strlen (cname) - 1; !IS_PATH_SEP (cname[j]); j--)
                    ;
                if (vfs_path_equal_len (current->name, tree->selected_ptr->name, j))
                    i++;
            }
            else if (current->sublevel == tree->selected_ptr->sublevel + 1)
            {
                j = vfs_path_len (tree->selected_ptr->name);
                if (j > 1 && vfs_path_equal_len (current->name, tree->selected_ptr->name, j))
                    i++;
            }
        }
        tree->topdiff = i;
    }

    /* Loop for every line */
    for (i = 0; i < tree_lines; i++)
    {
        const int *colors;

        colors = widget_get_colors (w);
        tty_setcolor (tree->is_panel ? NORMAL_COLOR : colors[DLG_COLOR_NORMAL]);

        /* Move to the beginning of the line */
        tty_draw_hline (w->rect.y + y + i, w->rect.x + x, ' ', tree_cols);

        if (current == NULL)
            continue;

        if (tree->is_panel)
        {
            gboolean selected;

            selected = widget_get_state (w, WST_FOCUSED) && current == tree->selected_ptr;
            tty_setcolor (selected ? SELECTED_COLOR : NORMAL_COLOR);
        }
        else
        {
            int idx = current == tree->selected_ptr ? DLG_COLOR_FOCUS : DLG_COLOR_NORMAL;

            tty_setcolor (colors[idx]);
        }

        tree->tree_shown[i] = current;
        if (current->sublevel == topsublevel)
            /* Show full name */
            tty_print_string (str_fit_to_term
                              (vfs_path_as_str (current->name),
                               tree_cols + (tree->is_panel ? 0 : 1), J_LEFT_FIT));
        else
        {
            /* Sub level directory */
            tty_set_alt_charset (TRUE);

            /* Output branch parts */
            for (j = 0; j < current->sublevel - topsublevel - 1; j++)
            {
                if (tree_cols - 8 - 3 * j < 9)
                    break;
                tty_print_char (' ');
                if ((current->submask & (1 << (j + topsublevel + 1))) != 0)
                    tty_print_char (ACS_VLINE);
                else
                    tty_print_char (' ');
                tty_print_char (' ');
            }
            tty_print_char (' ');
            j++;
            if (current->next == NULL || (current->next->submask & (1 << current->sublevel)) == 0)
                tty_print_char (ACS_LLCORNER);
            else
                tty_print_char (ACS_LTEE);
            tty_print_char (ACS_HLINE);
            tty_set_alt_charset (FALSE);

            /* Show sub-name */
            tty_print_char (' ');
            tty_print_string (str_fit_to_term
                              (current->subname, tree_cols - x - 3 * j, J_LEFT_FIT));
        }

        /* Calculate the next value for current */
        current = current->next;
        if (tree_navigation_flag)
            for (; current != NULL; current = current->next)
            {
                if (current->sublevel < tree->selected_ptr->sublevel)
                {
                    if (vfs_path_equal_len (current->name, tree->selected_ptr->name,
                                            vfs_path_len (current->name)))
                        break;
                }
                else if (current->sublevel == tree->selected_ptr->sublevel)
                {
                    const char *cname;

                    cname = vfs_path_as_str (current->name);
                    for (j = strlen (cname) - 1; !IS_PATH_SEP (cname[j]); j--)
                        ;
                    if (vfs_path_equal_len (current->name, tree->selected_ptr->name, j))
                        break;
                }
                else if (current->sublevel == tree->selected_ptr->sublevel + 1
                         && vfs_path_len (tree->selected_ptr->name) > 1)
                {
                    if (vfs_path_equal_len (current->name, tree->selected_ptr->name,
                                            vfs_path_len (tree->selected_ptr->name)))
                        break;
                }
            }
    }

    tree_show_mini_info (tree, tree_lines, tree_cols);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_check_focus (WTree *tree)
{
    if (tree->topdiff < 3)
        tree->topdiff = 3;
    else if (tree->topdiff >= tlines (tree) - 3)
        tree->topdiff = tlines (tree) - 3 - 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move_backward (WTree *tree, int i)
{
    if (!tree_navigation_flag)
        tree->selected_ptr = back_ptr (tree->selected_ptr, &i);
    else
    {
        tree_entry *current;
        int j = 0;

        current = tree->selected_ptr;
        while (j < i && current->prev != NULL
               && current->prev->sublevel >= tree->selected_ptr->sublevel)
        {
            current = current->prev;
            if (current->sublevel == tree->selected_ptr->sublevel)
            {
                tree->selected_ptr = current;
                j++;
            }
        }
        i = j;
    }

    tree->topdiff -= i;
    tree_check_focus (tree);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move_forward (WTree *tree, int i)
{
    if (!tree_navigation_flag)
        tree->selected_ptr = forw_ptr (tree->selected_ptr, &i);
    else
    {
        tree_entry *current;
        int j = 0;

        current = tree->selected_ptr;
        while (j < i && current->next != NULL
               && current->next->sublevel >= tree->selected_ptr->sublevel)
        {
            current = current->next;
            if (current->sublevel == tree->selected_ptr->sublevel)
            {
                tree->selected_ptr = current;
                j++;
            }
        }
        i = j;
    }

    tree->topdiff += i;
    tree_check_focus (tree);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move_to_child (WTree *tree)
{
    tree_entry *current;

    /* Do we have a starting point? */
    if (tree->selected_ptr == NULL)
        return;

    /* Take the next entry */
    current = tree->selected_ptr->next;
    /* Is it the child of the selected entry */
    if (current != NULL && current->sublevel > tree->selected_ptr->sublevel)
    {
        /* Yes -> select this entry */
        tree->selected_ptr = current;
        tree->topdiff++;
        tree_check_focus (tree);
    }
    else
    {
        /* No -> rescan and try again */
        tree_rescan (tree);
        current = tree->selected_ptr->next;
        if (current != NULL && current->sublevel > tree->selected_ptr->sublevel)
        {
            tree->selected_ptr = current;
            tree->topdiff++;
            tree_check_focus (tree);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tree_move_to_parent (WTree *tree)
{
    tree_entry *current;
    tree_entry *old;

    if (tree->selected_ptr == NULL)
        return FALSE;

    old = tree->selected_ptr;

    for (current = tree->selected_ptr->prev;
         current != NULL && current->sublevel >= tree->selected_ptr->sublevel;
         current = current->prev)
        tree->topdiff--;

    if (current == NULL)
        current = tree->store->tree_first;
    tree->selected_ptr = current;
    tree_check_focus (tree);
    return tree->selected_ptr != old;
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move_to_top (WTree *tree)
{
    tree->selected_ptr = tree->store->tree_first;
    tree->topdiff = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move_to_bottom (WTree *tree)
{
    tree->selected_ptr = tree->store->tree_last;
    tree->topdiff = tlines (tree) - 3 - 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_chdir_sel (WTree *tree)
{
    if (tree->is_panel)
    {
        WPanel *p;

        p = change_panel ();

        if (panel_cd (p, tree->selected_ptr->name, cd_exact))
            select_item (p);
        else
            cd_error_message (vfs_path_as_str (tree->selected_ptr->name));

        widget_draw (WIDGET (p));
        (void) change_panel ();
        show_tree (tree);
    }
    else
    {
        WDialog *h = DIALOG (WIDGET (tree)->owner);

        h->ret_value = B_ENTER;
        dlg_close (h);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
maybe_chdir (WTree *tree)
{
    if (xtree_mode && tree->is_panel && is_idle ())
        tree_chdir_sel (tree);
}

/* --------------------------------------------------------------------------------------------- */
/** Search tree for text */

static gboolean
search_tree (WTree *tree, const GString *text)
{
    tree_entry *current = tree->selected_ptr;
    gboolean wrapped = FALSE;
    gboolean found = FALSE;

    while (!found && (!wrapped || current != tree->selected_ptr))
        if (strncmp (current->subname, text->str, text->len) == 0)
        {
            tree->selected_ptr = current;
            found = TRUE;
        }
        else
        {
            current = current->next;
            if (current == NULL)
            {
                current = tree->store->tree_first;
                wrapped = TRUE;
            }

            tree->topdiff++;
        }

    tree_check_focus (tree);
    return found;
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_do_search (WTree *tree, int key)
{
    /* TODO: support multi-byte characters, see do_search() in panel.c */

    if (tree->search_buffer->len != 0 && key == KEY_BACKSPACE)
        g_string_set_size (tree->search_buffer, tree->search_buffer->len - 1);
    else if (key != 0)
        g_string_append_c (tree->search_buffer, (gchar) key);

    if (!search_tree (tree, tree->search_buffer))
        g_string_set_size (tree->search_buffer, tree->search_buffer->len - 1);

    show_tree (tree);
    maybe_chdir (tree);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_rescan (void *data)
{
    WTree *tree = data;
    vfs_path_t *old_vpath;

    old_vpath = vfs_path_clone (vfs_get_raw_current_dir ());
    if (old_vpath == NULL)
        return;

    if (tree->selected_ptr != NULL && mc_chdir (tree->selected_ptr->name) == 0)
    {
        int ret;

        tree_store_rescan (tree->selected_ptr->name);
        ret = mc_chdir (old_vpath);
        (void) ret;
    }
    vfs_path_free (old_vpath, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_forget (void *data)
{
    WTree *tree = data;

    if (tree->selected_ptr != NULL)
        tree_remove_entry (tree, tree->selected_ptr->name);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_copy (WTree *tree, const char *default_dest)
{
    char msg[BUF_MEDIUM];
    char *dest;

    if (tree->selected_ptr == NULL)
        return;

    g_snprintf (msg, sizeof (msg), _("Copy \"%s\" directory to:"),
                str_trunc (vfs_path_as_str (tree->selected_ptr->name), 50));
    dest = input_expand_dialog (Q_ ("DialogTitle|Copy"),
                                msg, MC_HISTORY_FM_TREE_COPY, default_dest,
                                INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD);

    if (dest != NULL && *dest != '\0')
    {
        file_op_context_t *ctx;
        file_op_total_context_t *tctx;

        ctx = file_op_context_new (OP_COPY);
        tctx = file_op_total_context_new ();
        file_op_context_create_ui (ctx, FALSE, FILEGUI_DIALOG_MULTI_ITEM);
        tctx->ask_overwrite = FALSE;
        copy_dir_dir (tctx, ctx, vfs_path_as_str (tree->selected_ptr->name), dest, TRUE, FALSE,
                      FALSE, NULL);
        file_op_total_context_destroy (tctx);
        file_op_context_destroy (ctx);
    }

    g_free (dest);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move (WTree *tree, const char *default_dest)
{
    char msg[BUF_MEDIUM];
    char *dest;

    if (tree->selected_ptr == NULL)
        return;

    g_snprintf (msg, sizeof (msg), _("Move \"%s\" directory to:"),
                str_trunc (vfs_path_as_str (tree->selected_ptr->name), 50));
    dest =
        input_expand_dialog (Q_ ("DialogTitle|Move"), msg, MC_HISTORY_FM_TREE_MOVE, default_dest,
                             INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD);

    if (dest != NULL && *dest != '\0')
    {
        vfs_path_t *dest_vpath;
        struct stat buf;

        dest_vpath = vfs_path_from_str (dest);

        if (mc_stat (dest_vpath, &buf) != 0)
            message (D_ERROR, MSG_ERROR, _("Cannot stat the destination\n%s"),
                     unix_error_string (errno));
        else if (!S_ISDIR (buf.st_mode))
            file_error (TRUE, _("Destination \"%s\" must be a directory\n%s"), dest);
        else
        {
            file_op_context_t *ctx;
            file_op_total_context_t *tctx;

            ctx = file_op_context_new (OP_MOVE);
            tctx = file_op_total_context_new ();
            file_op_context_create_ui (ctx, FALSE, FILEGUI_DIALOG_ONE_ITEM);
            move_dir_dir (tctx, ctx, vfs_path_as_str (tree->selected_ptr->name), dest);
            file_op_total_context_destroy (tctx);
            file_op_context_destroy (ctx);
        }

        vfs_path_free (dest_vpath, TRUE);
    }

    g_free (dest);
}

/* --------------------------------------------------------------------------------------------- */

#if 0
static void
tree_mkdir (WTree *tree)
{
    char old_dir[MC_MAXPATHLEN];

    if (tree->selected_ptr == NULL || chdir (tree->selected_ptr->name) != 0)
        return;
    /* FIXME
       mkdir_cmd (tree);
     */
    tree_rescan (tree);
    chdir (old_dir);
}
#endif

/* --------------------------------------------------------------------------------------------- */

static void
tree_rmdir (void *data)
{
    WTree *tree = data;
    file_op_context_t *ctx;
    file_op_total_context_t *tctx;

    if (tree->selected_ptr == NULL)
        return;

    if (confirm_delete)
    {
        char *buf;
        int result;

        buf = g_strdup_printf (_("Delete %s?"), vfs_path_as_str (tree->selected_ptr->name));

        result = query_dialog (Q_ ("DialogTitle|Delete"), buf, D_ERROR, 2, _("&Yes"), _("&No"));
        g_free (buf);
        if (result != 0)
            return;
    }

    ctx = file_op_context_new (OP_DELETE);
    tctx = file_op_total_context_new ();

    file_op_context_create_ui (ctx, FALSE, FILEGUI_DIALOG_ONE_ITEM);
    if (erase_dir (tctx, ctx, tree->selected_ptr->name) == FILE_CONT)
        tree_forget (tree);
    file_op_total_context_destroy (tctx);
    file_op_context_destroy (ctx);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
tree_move_up (WTree *tree)
{
    tree_move_backward (tree, 1);
    show_tree (tree);
    maybe_chdir (tree);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
tree_move_down (WTree *tree)
{
    tree_move_forward (tree, 1);
    show_tree (tree);
    maybe_chdir (tree);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
tree_move_home (WTree *tree)
{
    tree_move_to_top (tree);
    show_tree (tree);
    maybe_chdir (tree);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
tree_move_end (WTree *tree)
{
    tree_move_to_bottom (tree);
    show_tree (tree);
    maybe_chdir (tree);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move_pgup (WTree *tree)
{
    tree_move_backward (tree, tlines (tree) - 1);
    show_tree (tree);
    maybe_chdir (tree);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move_pgdn (WTree *tree)
{
    tree_move_forward (tree, tlines (tree) - 1);
    show_tree (tree);
    maybe_chdir (tree);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tree_move_left (WTree *tree)
{
    gboolean v = FALSE;

    if (tree_navigation_flag)
    {
        v = tree_move_to_parent (tree);
        show_tree (tree);
        maybe_chdir (tree);
    }

    return v;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tree_move_right (WTree *tree)
{
    gboolean v = FALSE;

    if (tree_navigation_flag)
    {
        tree_move_to_child (tree);
        show_tree (tree);
        maybe_chdir (tree);
        v = TRUE;
    }

    return v;
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_start_search (WTree *tree)
{
    if (tree->searching)
    {
        if (tree->selected_ptr == tree->store->tree_last)
            tree_move_to_top (tree);
        else
        {
            gboolean i;

            /* set navigation mode temporarily to 'Static' because in
             * dynamic navigation mode tree_move_forward will not move
             * to a lower sublevel if necessary (sequent searches must
             * start with the directory followed the last found directory)
             */
            i = tree_navigation_flag;
            tree_navigation_flag = FALSE;
            tree_move_forward (tree, 1);
            tree_navigation_flag = i;
        }
        tree_do_search (tree, 0);
    }
    else
    {
        tree->searching = TRUE;
        g_string_set_size (tree->search_buffer, 0);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_toggle_navig (WTree *tree)
{
    Widget *w = WIDGET (tree);
    WButtonBar *b;

    tree_navigation_flag = !tree_navigation_flag;

    b = buttonbar_find (DIALOG (w->owner));
    buttonbar_set_label (b, 4,
                         tree_navigation_flag ? Q_ ("ButtonBar|Static") : Q_ ("ButtonBar|Dynamc"),
                         w->keymap, w);
    widget_draw (WIDGET (b));
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_help (void)
{
    ev_help_t event_data = { NULL, "[Directory Tree]" };

    mc_event_raise (MCEVENT_GROUP_CORE, "help", &event_data);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
tree_execute_cmd (WTree *tree, long command)
{
    cb_ret_t res = MSG_HANDLED;

    if (command != CK_Search)
        tree->searching = FALSE;

    switch (command)
    {
    case CK_Help:
        tree_help ();
        break;
    case CK_Forget:
        tree_forget (tree);
        break;
    case CK_ToggleNavigation:
        tree_toggle_navig (tree);
        break;
    case CK_Copy:
        tree_copy (tree, "");
        break;
    case CK_Move:
        tree_move (tree, "");
        break;
    case CK_Up:
        tree_move_up (tree);
        break;
    case CK_Down:
        tree_move_down (tree);
        break;
    case CK_Top:
        tree_move_home (tree);
        break;
    case CK_Bottom:
        tree_move_end (tree);
        break;
    case CK_PageUp:
        tree_move_pgup (tree);
        break;
    case CK_PageDown:
        tree_move_pgdn (tree);
        break;
    case CK_Enter:
        tree_chdir_sel (tree);
        break;
    case CK_Reread:
        tree_rescan (tree);
        break;
    case CK_Search:
        tree_start_search (tree);
        break;
    case CK_Delete:
        tree_rmdir (tree);
        break;
    case CK_Quit:
        if (!tree->is_panel)
            dlg_close (DIALOG (WIDGET (tree)->owner));
        return res;
    default:
        res = MSG_NOT_HANDLED;
    }

    show_tree (tree);

    return res;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
tree_key (WTree *tree, int key)
{
    long command;

    if (is_abort_char (key))
    {
        if (tree->is_panel)
        {
            tree->searching = FALSE;
            show_tree (tree);
            return MSG_HANDLED; /* eat abort char */
        }
        /* modal tree dialog: let upper layer see the
           abort character and close the dialog */
        return MSG_NOT_HANDLED;
    }

    if (tree->searching && ((key >= ' ' && key <= 255) || key == KEY_BACKSPACE))
    {
        tree_do_search (tree, key);
        show_tree (tree);
        return MSG_HANDLED;
    }

    command = widget_lookup_key (WIDGET (tree), key);
    switch (command)
    {
    case CK_IgnoreKey:
        break;
    case CK_Left:
        return tree_move_left (tree) ? MSG_HANDLED : MSG_NOT_HANDLED;
    case CK_Right:
        return tree_move_right (tree) ? MSG_HANDLED : MSG_NOT_HANDLED;
    default:
        tree_execute_cmd (tree, command);
        return MSG_HANDLED;
    }

    /* Do not eat characters not meant for the tree below ' ' (e.g. C-l). */
    if (!command_prompt && ((key >= ' ' && key <= 255) || key == KEY_BACKSPACE))
    {
        tree_start_search (tree);
        tree_do_search (tree, key);
        return MSG_HANDLED;
    }

    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_frame (WDialog *h, WTree *tree)
{
    Widget *w = WIDGET (tree);

    (void) h;

    tty_setcolor (NORMAL_COLOR);
    widget_erase (w);
    if (tree->is_panel)
    {
        const char *title = _("Directory tree");
        const int len = str_term_width1 (title);

        tty_draw_box (w->rect.y, w->rect.x, w->rect.lines, w->rect.cols, FALSE);

        widget_gotoyx (w, 0, (w->rect.cols - len - 2) / 2);
        tty_printf (" %s ", title);

        if (panels_options.show_mini_info)
        {
            int y;

            y = w->rect.lines - 3;
            widget_gotoyx (w, y, 0);
            tty_print_alt_char (ACS_LTEE, FALSE);
            widget_gotoyx (w, y, w->rect.cols - 1);
            tty_print_alt_char (ACS_RTEE, FALSE);
            tty_draw_hline (w->rect.y + y, w->rect.x + 1, ACS_HLINE, w->rect.cols - 2);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
tree_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WTree *tree = (WTree *) w;
    WDialog *h = DIALOG (w->owner);
    WButtonBar *b;

    switch (msg)
    {
    case MSG_DRAW:
        tree_frame (h, tree);
        show_tree (tree);
        if (widget_get_state (w, WST_FOCUSED))
        {
            b = buttonbar_find (h);
            widget_draw (WIDGET (b));
        }
        return MSG_HANDLED;

    case MSG_FOCUS:
        b = buttonbar_find (h);
        buttonbar_set_label (b, 1, Q_ ("ButtonBar|Help"), w->keymap, w);
        buttonbar_set_label (b, 2, Q_ ("ButtonBar|Rescan"), w->keymap, w);
        buttonbar_set_label (b, 3, Q_ ("ButtonBar|Forget"), w->keymap, w);
        buttonbar_set_label (b, 4, tree_navigation_flag ? Q_ ("ButtonBar|Static")
                             : Q_ ("ButtonBar|Dynamc"), w->keymap, w);
        buttonbar_set_label (b, 5, Q_ ("ButtonBar|Copy"), w->keymap, w);
        buttonbar_set_label (b, 6, Q_ ("ButtonBar|RenMov"), w->keymap, w);
#if 0
        /* FIXME: mkdir is currently defunct */
        buttonbar_set_label (b, 7, Q_ ("ButtonBar|Mkdir"), w->keymap, w);
#else
        buttonbar_clear_label (b, 7, w);
#endif
        buttonbar_set_label (b, 8, Q_ ("ButtonBar|Rmdir"), w->keymap, w);

        return MSG_HANDLED;

    case MSG_UNFOCUS:
        tree->searching = FALSE;
        return MSG_HANDLED;

    case MSG_KEY:
        return tree_key (tree, parm);

    case MSG_ACTION:
        /* command from buttonbar */
        return tree_execute_cmd (tree, parm);

    case MSG_DESTROY:
        tree_destroy (tree);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
  * Mouse callback
  */
static void
tree_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    WTree *tree = (WTree *) w;
    int y;

    y = event->y;
    if (tree->is_panel)
        y--;

    switch (msg)
    {
    case MSG_MOUSE_DOWN:
        /* rest of the upper frame - call menu */
        if (tree->is_panel && event->y == WIDGET (w->owner)->rect.y)
        {
            /* return MOU_UNHANDLED */
            event->result.abort = TRUE;
        }
        else if (!widget_get_state (w, WST_FOCUSED))
            (void) change_panel ();
        break;

    case MSG_MOUSE_CLICK:
        {
            int lines;

            lines = tlines (tree);

            if (y < 0)
            {
                tree_move_backward (tree, lines - 1);
                show_tree (tree);
            }
            else if (y >= lines)
            {
                tree_move_forward (tree, lines - 1);
                show_tree (tree);
            }
            else if ((event->count & GPM_DOUBLE) != 0)
            {
                if (tree->tree_shown[y] != NULL)
                {
                    tree->selected_ptr = tree->tree_shown[y];
                    tree->topdiff = y;
                }

                tree_chdir_sel (tree);
            }
        }
        break;

    case MSG_MOUSE_SCROLL_UP:
    case MSG_MOUSE_SCROLL_DOWN:
        /* TODO: Ticket #2218 */
        break;

    default:
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WTree *
tree_new (const WRect *r, gboolean is_panel)
{
    WTree *tree;
    Widget *w;

    tree = g_new (WTree, 1);

    w = WIDGET (tree);
    widget_init (w, r, tree_callback, tree_mouse_callback);
    w->options |= WOP_SELECTABLE | WOP_TOP_SELECT;
    w->keymap = tree_map;

    tree->is_panel = is_panel;
    tree->selected_ptr = NULL;

    tree->store = tree_store_get ();
    tree_store_add_entry_remove_hook (remove_callback, tree);
    tree->tree_shown = NULL;
    tree->search_buffer = g_string_sized_new (MC_MAXPATHLEN);
    tree->topdiff = w->rect.lines / 2;
    tree->searching = FALSE;

    load_tree (tree);
    return tree;
}

/* --------------------------------------------------------------------------------------------- */

void
tree_chdir (WTree *tree, const vfs_path_t *dir)
{
    tree_entry *current;

    current = tree_store_whereis (dir);
    if (current != NULL)
    {
        tree->selected_ptr = current;
        tree_check_focus (tree);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Return name of the currently selected entry */

const vfs_path_t *
tree_selected_name (const WTree *tree)
{
    return tree->selected_ptr->name;
}

/* --------------------------------------------------------------------------------------------- */

void
sync_tree (const vfs_path_t *vpath)
{
    tree_chdir (the_tree, vpath);
}

/* --------------------------------------------------------------------------------------------- */

WTree *
find_tree (const WDialog *h)
{
    return (WTree *) widget_find_by_type (CONST_WIDGET (h), tree_callback);
}

/* --------------------------------------------------------------------------------------------- */
