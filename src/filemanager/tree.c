/*
   Directory tree browser for the Midnight Commander
   This module has been converted to be a widget.

   The program load and saves the tree each time the tree widget is
   created and destroyed.  This is required for the future vfs layer,
   it will be possible to have tree views over virtual file systems.

   Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
   2003, 2004, 2005, 2007, 2011
   The Free Software Foundation, Inc.

   Written by:
   Janne Kukonlehto, 1994, 1996
   Norbert Warmuth, 1997
   Miguel de Icaza, 1996, 1999

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
#include "lib/tty/mouse.h"
#include "lib/tty/key.h"
#include "lib/skin.h"
#include "lib/vfs/vfs.h"
#include "lib/fileloc.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"
#include "lib/event.h"          /* mc_event_raise() */

#include "src/setup.h"          /* confirm_delete, panels_options */
#include "src/keybind-defaults.h"
#include "src/history.h"

#include "dir.h"
#include "midnight.h"           /* the_menubar */
#include "file.h"               /* copy_dir_dir(), move_dir_dir(), erase_dir() */
#include "layout.h"             /* command_prompt */
#include "treestore.h"
#include "cmd.h"
#include "filegui.h"

#include "tree.h"

/*** global variables ****************************************************************************/

/* The pointer to the tree */
WTree *the_tree = NULL;

/* If this is true, then when browsing the tree the other window will
 * automatically reload it's directory with the contents of the currently
 * selected directory.
 */
int xtree_mode = 0;

/*** file scope macro definitions ****************************************************************/

#define tlines(t) (t->is_panel ? WIDGET (t)->lines - 2 - \
                    (panels_options.show_mini_info ? 2 : 0) : WIDGET (t)->lines)

/* Use the color of the parent widget for the unselected entries */
#define TREE_NORMALC(h) (h->color[DLG_COLOR_NORMAL])
#define TREE_CURRENTC(h) (h->color[DLG_COLOR_FOCUS])

/*** file scope type declarations ****************************************************************/

struct WTree
{
    Widget widget;
    struct TreeStore *store;
    tree_entry *selected_ptr;   /* The selected directory */
    char search_buffer[MC_MAXFILENAMELEN];      /* Current search string */
    tree_entry **tree_shown;    /* Entries currently on screen */
    int is_panel;               /* panel or plain widget flag */
    int active;                 /* if it's currently selected */
    int searching;              /* Are we on searching mode? */
    int topdiff;                /* The difference between the topmost
                                   shown and the selected */
};

/*** file scope variables ************************************************************************/

/* Specifies the display mode: 1d or 2d */
static gboolean tree_navigation_flag = FALSE;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void tree_rescan (void *data);

/* --------------------------------------------------------------------------------------------- */

static tree_entry *
back_ptr (tree_entry * ptr, int *count)
{
    int i = 0;

    while (ptr && ptr->prev && i < *count)
    {
        ptr = ptr->prev;
        i++;
    }
    *count = i;
    return ptr;
}

/* --------------------------------------------------------------------------------------------- */

static tree_entry *
forw_ptr (tree_entry * ptr, int *count)
{
    int i = 0;

    while (ptr && ptr->next && i < *count)
    {
        ptr = ptr->next;
        i++;
    }
    *count = i;
    return ptr;
}

/* --------------------------------------------------------------------------------------------- */

static void
remove_callback (tree_entry * entry, void *data)
{
    WTree *tree = data;

    if (tree->selected_ptr == entry)
    {
        if (tree->selected_ptr->next)
            tree->selected_ptr = tree->selected_ptr->next;
        else
            tree->selected_ptr = tree->selected_ptr->prev;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Save the ${XDG_CACHE_HOME}/mc/Tree file */

static void
save_tree (WTree * tree)
{
    int error;
    char *tree_name;

    (void) tree;
    error = tree_store_save ();


    if (error)
    {
        tree_name = mc_config_get_full_path (MC_TREESTORE_FILE);
        fprintf (stderr, _("Cannot open the %s file for writing:\n%s\n"), tree_name,
                 unix_error_string (error));
        g_free (tree_name);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_remove_entry (WTree * tree, const vfs_path_t * name_vpath)
{
    (void) tree;
    tree_store_remove_entry (name_vpath);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_destroy (WTree * tree)
{
    tree_store_remove_entry_remove_hook (remove_callback);
    save_tree (tree);

    g_free (tree->tree_shown);
    tree->tree_shown = 0;
    tree->selected_ptr = NULL;
}

/* --------------------------------------------------------------------------------------------- */
/** Loads the .mc.tree file */

static void
load_tree (WTree * tree)
{
    tree_store_load ();

    tree->selected_ptr = tree->store->tree_first;
    tree_chdir (tree, mc_config_get_home_dir ());
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_show_mini_info (WTree * tree, int tree_lines, int tree_cols)
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
        tty_draw_hline (w->y + line, w->x + 1, ' ', tree_cols);
        widget_move (w, line, 1);
        tty_print_char (PATH_SEP);
        tty_print_string (str_fit_to_term (tree->search_buffer, tree_cols - 2, J_LEFT_FIT));
        tty_print_char (' ');
    }
    else
    {
        /* Show full name of selected directory */
        WDialog *h = w->owner;
        char *tmp_path;

        tty_setcolor (tree->is_panel ? NORMAL_COLOR : TREE_NORMALC (h));
        tty_draw_hline (w->y + line, w->x + 1, ' ', tree_cols);
        widget_move (w, line, 1);
        tmp_path = vfs_path_to_str (tree->selected_ptr->name);
        tty_print_string (str_fit_to_term (tmp_path, tree_cols, J_LEFT_FIT));
        g_free (tmp_path);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
show_tree (WTree * tree)
{
    Widget *w = WIDGET (tree);
    WDialog *h = w->owner;
    tree_entry *current;
    int i, j, topsublevel;
    int x = 0, y = 0;
    int tree_lines, tree_cols;

    /* Initialize */
    tree_lines = tlines (tree);
    tree_cols = w->cols;

    widget_move (w, y, x);
    if (tree->is_panel)
    {
        tree_cols -= 2;
        x = y = 1;
    }

    g_free (tree->tree_shown);
    tree->tree_shown = g_new0 (tree_entry *, tree_lines);

    if (tree->store->tree_first)
        topsublevel = tree->store->tree_first->sublevel;
    else
        topsublevel = 0;
    if (!tree->selected_ptr)
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
        while (current->prev && i < tree->topdiff)
        {
            char *current_name;

            current = current->prev;
            current_name = vfs_path_to_str (current->name);

            if (current->sublevel < tree->selected_ptr->sublevel)
            {
                if (vfs_path_equal (current->name, tree->selected_ptr->name))
                    i++;
            }
            else if (current->sublevel == tree->selected_ptr->sublevel)
            {
                for (j = strlen (current_name) - 1; current_name[j] != PATH_SEP; j--);
                if (vfs_path_equal_len (current->name, tree->selected_ptr->name, j))
                    i++;
            }
            else
            {
                if (current->sublevel == tree->selected_ptr->sublevel + 1
                    && vfs_path_len (tree->selected_ptr->name) > 1)
                {
                    if (vfs_path_equal_len (current->name, tree->selected_ptr->name,
                                            vfs_path_len (tree->selected_ptr->name)))
                        i++;
                }
            }
            g_free (current_name);
        }
        tree->topdiff = i;
    }

    /* Loop for every line */
    for (i = 0; i < tree_lines; i++)
    {
        tty_setcolor (tree->is_panel ? NORMAL_COLOR : TREE_NORMALC (h));

        /* Move to the beginning of the line */
        tty_draw_hline (w->y + y + i, w->x + x, ' ', tree_cols);

        if (current == NULL)
            continue;

        if (tree->is_panel)
            tty_setcolor (tree->active && current == tree->selected_ptr
                          ? SELECTED_COLOR : NORMAL_COLOR);
        else
            tty_setcolor (current == tree->selected_ptr ? TREE_CURRENTC (h) : TREE_NORMALC (h));

        tree->tree_shown[i] = current;
        if (current->sublevel == topsublevel)
        {
            /* Show full name */
            char *current_name;

            current_name = vfs_path_to_str (current->name);
            tty_print_string (str_fit_to_term
                              (current_name, tree_cols + (tree->is_panel ? 0 : 1), J_LEFT_FIT));
            g_free (current_name);
        }
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
                if (current->submask & (1 << (j + topsublevel + 1)))
                    tty_print_char (ACS_VLINE);
                else
                    tty_print_char (' ');
                tty_print_char (' ');
            }
            tty_print_char (' ');
            j++;
            if (!current->next || !(current->next->submask & (1 << current->sublevel)))
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
        {
            while (current != NULL)
            {
                if (current->sublevel < tree->selected_ptr->sublevel)
                {
                    if (vfs_path_equal_len (current->name, tree->selected_ptr->name,
                                            vfs_path_len (current->name)))
                        break;
                }
                else if (current->sublevel == tree->selected_ptr->sublevel)
                {
                    char *current_name;

                    current_name = vfs_path_to_str (current->name);
                    for (j = strlen (current_name) - 1; current_name[j] != PATH_SEP; j--)
                        ;
                    g_free (current_name);
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
                current = current->next;
            }
        }
    }

    tree_show_mini_info (tree, tree_lines, tree_cols);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_check_focus (WTree * tree)
{
    if (tree->topdiff < 3)
        tree->topdiff = 3;
    else if (tree->topdiff >= tlines (tree) - 3)
        tree->topdiff = tlines (tree) - 3 - 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move_backward (WTree * tree, int i)
{
    if (!tree_navigation_flag)
        tree->selected_ptr = back_ptr (tree->selected_ptr, &i);
    else
    {
        tree_entry *current;
        int j = 0;

        current = tree->selected_ptr;
        while (j < i && current->prev && current->prev->sublevel >= tree->selected_ptr->sublevel)
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
tree_move_forward (WTree * tree, int i)
{
    if (!tree_navigation_flag)
        tree->selected_ptr = forw_ptr (tree->selected_ptr, &i);
    else
    {
        tree_entry *current;
        int j = 0;

        current = tree->selected_ptr;
        while (j < i && current->next && current->next->sublevel >= tree->selected_ptr->sublevel)
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
tree_move_to_child (WTree * tree)
{
    tree_entry *current;

    /* Do we have a starting point? */
    if (!tree->selected_ptr)
        return;
    /* Take the next entry */
    current = tree->selected_ptr->next;
    /* Is it the child of the selected entry */
    if (current && current->sublevel > tree->selected_ptr->sublevel)
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
        if (current && current->sublevel > tree->selected_ptr->sublevel)
        {
            tree->selected_ptr = current;
            tree->topdiff++;
            tree_check_focus (tree);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tree_move_to_parent (WTree * tree)
{
    tree_entry *current;
    tree_entry *old;

    if (!tree->selected_ptr)
        return FALSE;

    old = tree->selected_ptr;
    current = tree->selected_ptr->prev;
    while (current && current->sublevel >= tree->selected_ptr->sublevel)
    {
        current = current->prev;
        tree->topdiff--;
    }
    if (!current)
        current = tree->store->tree_first;
    tree->selected_ptr = current;
    tree_check_focus (tree);
    return tree->selected_ptr != old;
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move_to_top (WTree * tree)
{
    tree->selected_ptr = tree->store->tree_first;
    tree->topdiff = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move_to_bottom (WTree * tree)
{
    tree->selected_ptr = tree->store->tree_last;
    tree->topdiff = tlines (tree) - 3 - 1;
}

/* --------------------------------------------------------------------------------------------- */

/** Handle mouse click */
static void
tree_mouse_click (WTree * tree, int y)
{
    if (tree->tree_shown[y])
    {
        tree->selected_ptr = tree->tree_shown[y];
        tree->topdiff = y;
    }
    show_tree (tree);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_chdir_sel (WTree * tree)
{
    char *tmp_path;

    if (!tree->is_panel)
        return;

    change_panel ();

    tmp_path = vfs_path_to_str (tree->selected_ptr->name);
    if (do_cd (tree->selected_ptr->name, cd_exact))
        select_item (current_panel);
    else
        message (D_ERROR, MSG_ERROR, _("Cannot chdir to \"%s\"\n%s"),
                 tmp_path, unix_error_string (errno));

    g_free (tmp_path);
    change_panel ();
    show_tree (tree);
}

/* --------------------------------------------------------------------------------------------- */

static void
maybe_chdir (WTree * tree)
{
    if (xtree_mode && tree->is_panel && is_idle ())
        tree_chdir_sel (tree);
}

/* --------------------------------------------------------------------------------------------- */
/** Mouse callback */

static int
tree_event (Gpm_Event * event, void *data)
{
    WTree *tree = (WTree *) data;
    Widget *w = WIDGET (data);
    Gpm_Event local;

    if (!mouse_global_in_widget (event, w))
        return MOU_UNHANDLED;

    /* rest of the upper frame - call menu */
    if (tree->is_panel && (event->type & GPM_DOWN) != 0 && event->y == WIDGET (w->owner)->y + 1)
        return MOU_UNHANDLED;

    local = mouse_get_local (event, w);

    if ((local.type & GPM_UP) == 0)
        return MOU_NORMAL;

    if (tree->is_panel)
        local.y--;

    local.y--;

    if (!tree->active)
        change_panel ();

    if (local.y < 0)
    {
        tree_move_backward (tree, tlines (tree) - 1);
        show_tree (tree);
    }
    else if (local.y >= tlines (tree))
    {
        tree_move_forward (tree, tlines (tree) - 1);
        show_tree (tree);
    }
    else
    {
        tree_mouse_click (tree, local.y);
        if ((local.type & (GPM_UP | GPM_DOUBLE)) == (GPM_UP | GPM_DOUBLE))
            tree_chdir_sel (tree);
    }

    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */
/** Search tree for text */

static int
search_tree (WTree * tree, char *text)
{
    tree_entry *current;
    int len;
    int wrapped = 0;
    int found = 0;

    len = strlen (text);
    current = tree->selected_ptr;
    found = 0;
    while (!wrapped || current != tree->selected_ptr)
    {
        if (strncmp (current->subname, text, len) == 0)
        {
            tree->selected_ptr = current;
            found = 1;
            break;
        }
        current = current->next;
        if (!current)
        {
            current = tree->store->tree_first;
            wrapped = 1;
        }
        tree->topdiff++;
    }
    tree_check_focus (tree);
    return found;
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_do_search (WTree * tree, int key)
{
    size_t l;

    l = strlen (tree->search_buffer);
    if ((l != 0) && (key == KEY_BACKSPACE))
        tree->search_buffer[--l] = '\0';
    else if (key && l < sizeof (tree->search_buffer))
    {
        tree->search_buffer[l] = key;
        tree->search_buffer[++l] = '\0';
    }

    if (!search_tree (tree, tree->search_buffer))
        tree->search_buffer[--l] = 0;

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
    vfs_path_free (old_vpath);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_forget (void *data)
{
    WTree *tree = data;
    if (tree->selected_ptr)
        tree_remove_entry (tree, tree->selected_ptr->name);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_copy (WTree * tree, const char *default_dest)
{
    char msg[BUF_MEDIUM];
    char *dest, *selected_ptr_name;

    if (tree->selected_ptr == NULL)
        return;

    selected_ptr_name = vfs_path_to_str (tree->selected_ptr->name);

    g_snprintf (msg, sizeof (msg), _("Copy \"%s\" directory to:"),
                str_trunc (selected_ptr_name, 50));
    dest = input_expand_dialog (Q_ ("DialogTitle|Copy"),
                                msg, MC_HISTORY_FM_TREE_COPY, default_dest,
                                INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD);

    if (dest != NULL && *dest != '\0')
    {
        FileOpContext *ctx;
        FileOpTotalContext *tctx;

        ctx = file_op_context_new (OP_COPY);
        tctx = file_op_total_context_new ();
        file_op_context_create_ui (ctx, FALSE, FILEGUI_DIALOG_MULTI_ITEM);
        tctx->ask_overwrite = FALSE;
        copy_dir_dir (tctx, ctx, selected_ptr_name, dest, TRUE, FALSE, FALSE, NULL);
        file_op_total_context_destroy (tctx);
        file_op_context_destroy (ctx);
    }

    g_free (dest);
    g_free (selected_ptr_name);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move (WTree * tree, const char *default_dest)
{
    char msg[BUF_MEDIUM];
    char *dest, *selected_ptr_name;
    struct stat buf;
    FileOpContext *ctx;
    FileOpTotalContext *tctx;

    if (tree->selected_ptr == NULL)
        return;

    selected_ptr_name = vfs_path_to_str (tree->selected_ptr->name);

    g_snprintf (msg, sizeof (msg), _("Move \"%s\" directory to:"),
                str_trunc (selected_ptr_name, 50));
    dest =
        input_expand_dialog (Q_ ("DialogTitle|Move"), msg, MC_HISTORY_FM_TREE_MOVE, default_dest,
                             INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD);

    if (dest == NULL || *dest == '\0')
        goto ret;

    if (stat (dest, &buf))
    {
        message (D_ERROR, MSG_ERROR, _("Cannot stat the destination\n%s"),
                 unix_error_string (errno));
        goto ret;
    }

    if (!S_ISDIR (buf.st_mode))
    {
        file_error (_("Destination \"%s\" must be a directory\n%s"), dest);
        goto ret;
    }

    ctx = file_op_context_new (OP_MOVE);
    tctx = file_op_total_context_new ();
    file_op_context_create_ui (ctx, FALSE, FILEGUI_DIALOG_ONE_ITEM);
    move_dir_dir (tctx, ctx, selected_ptr_name, dest);
    file_op_total_context_destroy (tctx);
    file_op_context_destroy (ctx);

  ret:
    g_free (selected_ptr_name);
    g_free (dest);
}

/* --------------------------------------------------------------------------------------------- */

#if 0
static void
tree_mkdir (WTree * tree)
{
    char old_dir[MC_MAXPATHLEN];

    if (!tree->selected_ptr)
        return;
    if (chdir (tree->selected_ptr->name))
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
    FileOpContext *ctx;
    FileOpTotalContext *tctx;

    if (!tree->selected_ptr)
        return;

    if (confirm_delete)
    {
        char *buf;
        int result;
        char *selected_ptr_name;

        selected_ptr_name = vfs_path_to_str (tree->selected_ptr->name);
        buf = g_strdup_printf (_("Delete %s?"), selected_ptr_name);
        g_free (selected_ptr_name);

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
tree_move_up (WTree * tree)
{
    tree_move_backward (tree, 1);
    show_tree (tree);
    maybe_chdir (tree);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
tree_move_down (WTree * tree)
{
    tree_move_forward (tree, 1);
    show_tree (tree);
    maybe_chdir (tree);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
tree_move_home (WTree * tree)
{
    tree_move_to_top (tree);
    show_tree (tree);
    maybe_chdir (tree);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
tree_move_end (WTree * tree)
{
    tree_move_to_bottom (tree);
    show_tree (tree);
    maybe_chdir (tree);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move_pgup (WTree * tree)
{
    tree_move_backward (tree, tlines (tree) - 1);
    show_tree (tree);
    maybe_chdir (tree);
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_move_pgdn (WTree * tree)
{
    tree_move_forward (tree, tlines (tree) - 1);
    show_tree (tree);
    maybe_chdir (tree);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tree_move_left (WTree * tree)
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
tree_move_right (WTree * tree)
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
tree_start_search (WTree * tree)
{
    gboolean i;

    if (tree->searching)
    {
        if (tree->selected_ptr == tree->store->tree_last)
            tree_move_to_top (tree);
        else
        {
            /* set navigation mode temporarily to 'Static' because in
             * dynamic navigation mode tree_move_forward will not move
             * to a lower sublevel if necessary (sequent searches must
             * start with the directory followed the last found directory)
             */
            i = tree_navigation_flag;
            tree_navigation_flag = 0;
            tree_move_forward (tree, 1);
            tree_navigation_flag = i;
        }
        tree_do_search (tree, 0);
    }
    else
    {
        tree->searching = 1;
        tree->search_buffer[0] = 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_toggle_navig (WTree * tree)
{
    tree_navigation_flag = !tree_navigation_flag;
    buttonbar_set_label (find_buttonbar (WIDGET (tree)->owner), 4,
                         tree_navigation_flag ? Q_ ("ButtonBar|Static")
                         : Q_ ("ButtonBar|Dynamc"), tree_map, WIDGET (tree));
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
tree_execute_cmd (WTree * tree, unsigned long command)
{
    cb_ret_t res = MSG_HANDLED;

    if (command != CK_Search)
        tree->searching = 0;

    switch (command)
    {
    case CK_Help:
        {
            ev_help_t event_data = { NULL, "[Directory Tree]" };
            mc_event_raise (MCEVENT_GROUP_CORE, "help", &event_data);
        }
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
            dlg_stop (WIDGET (tree)->owner);
        return res;
    default:
        res = MSG_NOT_HANDLED;
    }

    show_tree (tree);

    return res;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
tree_key (WTree * tree, int key)
{
    size_t i;

    if (is_abort_char (key))
    {
        if (tree->is_panel)
        {
            tree->searching = 0;
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

    for (i = 0; tree_map[i].key != 0; i++)
        if (key == tree_map[i].key)
            switch (tree_map[i].command)
            {
            case CK_Left:
                return tree_move_left (tree) ? MSG_HANDLED : MSG_NOT_HANDLED;
            case CK_Right:
                return tree_move_right (tree) ? MSG_HANDLED : MSG_NOT_HANDLED;
            default:
                tree_execute_cmd (tree, tree_map[i].command);
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
tree_frame (WDialog * h, WTree * tree)
{
    Widget *w = WIDGET (tree);

    tty_setcolor (NORMAL_COLOR);
    widget_erase (w);
    if (tree->is_panel)
    {
        const char *title = _("Directory tree");
        const int len = str_term_width1 (title);

        draw_box (h, w->y, w->x, w->lines, w->cols, FALSE);

        widget_move (w, 0, (w->cols - len - 2) / 2);
        tty_printf (" %s ", title);

        if (panels_options.show_mini_info)
            widget_move (w, tlines (tree) + 1, 0);
        tty_print_alt_char (ACS_LTEE, FALSE);
        widget_move (w, tlines (tree) + 1, w->cols - 1);
        tty_print_alt_char (ACS_RTEE, FALSE);
        tty_draw_hline (w->y + tlines (tree) + 1, w->x + 1, ACS_HLINE, w->cols - 2);
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
tree_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WTree *tree = (WTree *) w;
    WDialog *h = w->owner;
    WButtonBar *b = find_buttonbar (h);

    switch (msg)
    {
    case MSG_DRAW:
        tree_frame (h, tree);
        show_tree (tree);
        return MSG_HANDLED;

    case MSG_FOCUS:
        tree->active = 1;
        buttonbar_set_label (b, 1, Q_ ("ButtonBar|Help"), tree_map, w);
        buttonbar_set_label (b, 2, Q_ ("ButtonBar|Rescan"), tree_map, w);
        buttonbar_set_label (b, 3, Q_ ("ButtonBar|Forget"), tree_map, w);
        buttonbar_set_label (b, 4, tree_navigation_flag ? Q_ ("ButtonBar|Static")
                             : Q_ ("ButtonBar|Dynamc"), tree_map, w);
        buttonbar_set_label (b, 5, Q_ ("ButtonBar|Copy"), tree_map, w);
        buttonbar_set_label (b, 6, Q_ ("ButtonBar|RenMov"), tree_map, w);
#if 0
        /* FIXME: mkdir is currently defunct */
        buttonbar_set_label (b, 7, Q_ ("ButtonBar|Mkdir"), tree_map, w);
#else
        buttonbar_clear_label (b, 7, WIDGET (tree));
#endif
        buttonbar_set_label (b, 8, Q_ ("ButtonBar|Rmdir"), tree_map, w);
        widget_redraw (WIDGET (b));

        /* FIXME: Should find a better way of only displaying the
           currently selected item */
        show_tree (tree);
        return MSG_HANDLED;

        /* FIXME: Should find a better way of changing the color of the
           selected item */

    case MSG_UNFOCUS:
        tree->active = 0;
        tree->searching = 0;
        show_tree (tree);
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
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WTree *
tree_new (int y, int x, int lines, int cols, gboolean is_panel)
{
    WTree *tree;
    Widget *w;

    tree = g_new (WTree, 1);
    w = WIDGET (tree);

    init_widget (w, y, x, lines, cols, tree_callback, tree_event);
    tree->is_panel = is_panel;
    tree->selected_ptr = 0;

    tree->store = tree_store_get ();
    tree_store_add_entry_remove_hook (remove_callback, tree);
    tree->tree_shown = 0;
    tree->search_buffer[0] = 0;
    tree->topdiff = w->lines / 2;
    tree->searching = 0;
    tree->active = 0;

    /* We do not want to keep the cursor */
    widget_want_cursor (w, FALSE);
    load_tree (tree);
    return tree;
}

/* --------------------------------------------------------------------------------------------- */

void
tree_chdir (WTree * tree, const char *dir)
{
    vfs_path_t *vpath;
    tree_entry *current;

    vpath = vfs_path_from_str (dir);
    current = tree_store_whereis (vpath);
    if (current != NULL)
    {
        tree->selected_ptr = current;
        tree_check_focus (tree);
    }
    vfs_path_free (vpath);
}

/* --------------------------------------------------------------------------------------------- */
/** Return name of the currently selected entry */

vfs_path_t *
tree_selected_name (const WTree * tree)
{
    return tree->selected_ptr->name;
}

/* --------------------------------------------------------------------------------------------- */

void
sync_tree (const char *path)
{
    tree_chdir (the_tree, path);
}

/* --------------------------------------------------------------------------------------------- */

WTree *
find_tree (struct WDialog *h)
{
    return (WTree *) find_widget_type (h, tree_callback);
}

/* --------------------------------------------------------------------------------------------- */
