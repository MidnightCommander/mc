/*
   Routines invoked by a function key
   They normally operate on the current panel.

   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2009, 2011
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

/** \file cmd.c
 *  \brief Source: routines invoked by a function key
 *
 *  They normally operate on the current panel.
 */

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#ifdef ENABLE_VFS_NET
#include <netdb.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <sys/time.h>

#include "lib/global.h"

#include "lib/tty/tty.h"        /* LINES, tty_touch_screen() */
#include "lib/tty/key.h"        /* ALT() macro */
#include "lib/tty/win.h"        /* do_enter_ca_mode() */
#include "lib/mcconfig.h"
#include "lib/search.h"
#include "lib/filehighlight.h"  /* MC_FHL_INI_FILE */
#include "lib/vfs/vfs.h"
#include "lib/fileloc.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"
#include "lib/keybind.h"        /* CK_Down, CK_History */
#include "lib/event.h"          /* mc_event_raise() */

#include "src/viewer/mcviewer.h"
#include "src/setup.h"
#include "src/execute.h"        /* toggle_panels() */
#include "src/history.h"
#include "src/util.h"           /* check_for_default() */

#ifdef USE_INTERNAL_EDIT
#include "src/editor/edit.h"
#endif

#ifdef USE_DIFF_VIEW
#include "src/diffviewer/ydiff.h"
#endif

#include "fileopctx.h"
#include "file.h"               /* file operation routines */
#include "find.h"               /* find_file() */
#include "hotlist.h"            /* hotlist_show() */
#include "tree.h"               /* tree_chdir() */
#include "midnight.h"           /* change_panel() */
#include "usermenu.h"           /* MC_GLOBAL_MENU */
#include "command.h"            /* cmdline */
#include "layout.h"             /* get_current_type() */
#include "ext.h"                /* regex_command() */
#include "boxes.h"              /* cd_dialog() */
#include "dir.h"

#include "cmd.h"                /* Our definitions */

/*** global variables ****************************************************************************/

int select_flags = SELECT_MATCH_CASE | SELECT_SHELL_PATTERNS;

/*** file scope macro definitions ****************************************************************/

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

/*** file scope type declarations ****************************************************************/

enum CompareMode
{
    compare_quick, compare_size_only, compare_thourough
};

/*** file scope variables ************************************************************************/

#ifdef ENABLE_VFS_NET
static const char *machine_str = N_("Enter machine name (F1 for details):");
#endif /* ENABLE_VFS_NET */

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** scan_for_file (panel, idx, direction)
 *
 * Inputs:
 *   panel:     pointer to the panel on which we operate
 *   idx:       starting file.
 *   direction: 1, or -1
 */

static int
scan_for_file (WPanel * panel, int idx, int direction)
{
    int i = idx + direction;

    while (i != idx)
    {
        if (i < 0)
            i = panel->count - 1;
        if (i == panel->count)
            i = 0;
        if (!S_ISDIR (panel->dir.list[i].st.st_mode))
            return i;
        i += direction;
    }
    return i;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Run viewer (internal or external) on the currently selected file.
 * If normal is TRUE, force internal viewer and raw mode (used for F13).
 */
static void
do_view_cmd (gboolean normal)
{
    /* Directories are viewed by changing to them */
    if (S_ISDIR (selection (current_panel)->st.st_mode) || link_isdir (selection (current_panel)))
    {
        if (confirm_view_dir && (current_panel->marked || current_panel->dirs_marked))
        {
            if (query_dialog
                (_("Confirmation"), _("Files tagged, want to cd?"), D_NORMAL, 2,
                 _("&Yes"), _("&No")) != 0)
            {
                return;
            }
        }
        if (!do_cd (selection (current_panel)->fname, cd_exact))
            message (D_ERROR, MSG_ERROR, _("Cannot change directory"));
    }
    else
    {
        int file_idx;

        file_idx = current_panel->selected;

        while (TRUE)
        {
            char *filename;
            int dir;

            filename = current_panel->dir.list[file_idx].fname;
            dir = view_file (filename, normal, use_internal_view);
            if (dir == 0)
                break;
            file_idx = scan_for_file (current_panel, file_idx, dir);
        }
    }

    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

static inline void
do_edit (const char *what)
{
    do_edit_at_line (what, use_internal_edit, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
set_panel_filter_to (WPanel * p, char *allocated_filter_string)
{
    g_free (p->filter);
    p->filter = 0;

    if (!(allocated_filter_string[0] == '*' && allocated_filter_string[1] == 0))
        p->filter = allocated_filter_string;
    else
        g_free (allocated_filter_string);
    reread_cmd ();
}

/* --------------------------------------------------------------------------------------------- */
/** Set a given panel filter expression */

static void
set_panel_filter (WPanel * p)
{
    char *reg_exp;
    const char *x;

    x = p->filter ? p->filter : easy_patterns ? "*" : ".";

    reg_exp = input_dialog_help (_("Filter"),
                                 _("Set expression for filtering filenames"),
                                 "[Filter...]", MC_HISTORY_FM_PANEL_FILTER, x);
    if (!reg_exp)
        return;
    set_panel_filter_to (p, reg_exp);
}

/* --------------------------------------------------------------------------------------------- */

static void
select_unselect_cmd (const char *title, const char *history_name, gboolean do_select)
{
    /* dialog sizes */
    const int DX = 50;
    const int DY = 7;

    int files_only = (select_flags & SELECT_FILES_ONLY) != 0;
    int case_sens = (select_flags & SELECT_MATCH_CASE) != 0;
    int shell_patterns = (select_flags & SELECT_SHELL_PATTERNS) != 0;

    char *reg_exp;
    mc_search_t *search;
    int i;

    QuickWidget quick_widgets[] = {
        QUICK_CHECKBOX (3, DX, DY - 3, DY, N_("&Using shell patterns"), &shell_patterns),
        QUICK_CHECKBOX (DX / 2 + 1, DX, DY - 4, DY, N_("&Case sensitive"), &case_sens),
        QUICK_CHECKBOX (3, DX, DY - 4, DY, N_("&Files only"), &files_only),
        QUICK_INPUT (3, DX, DY - 5, DY, INPUT_LAST_TEXT, DX - 6, 0, history_name, &reg_exp),
        QUICK_END
    };

    QuickDialog quick_dlg = {
        DX, DY, -1, -1, title,
        "[Select/Unselect Files]", quick_widgets, NULL, FALSE
    };

    if (quick_dialog (&quick_dlg) == B_CANCEL)
        return;

    if (!reg_exp)
        return;
    if (!*reg_exp)
    {
        g_free (reg_exp);
        return;
    }
    search = mc_search_new (reg_exp, -1);
    search->search_type = (shell_patterns != 0) ? MC_SEARCH_T_GLOB : MC_SEARCH_T_REGEX;
    search->is_entire_line = TRUE;
    search->is_case_sensitive = case_sens != 0;

    for (i = 0; i < current_panel->count; i++)
    {
        if (strcmp (current_panel->dir.list[i].fname, "..") == 0)
            continue;
        if (S_ISDIR (current_panel->dir.list[i].st.st_mode) && files_only != 0)
            continue;

        if (mc_search_run (search, current_panel->dir.list[i].fname,
                           0, current_panel->dir.list[i].fnamelen, NULL))
            do_file_mark (current_panel, i, do_select);
    }

    mc_search_free (search);
    g_free (reg_exp);

    /* result flags */
    select_flags = 0;
    if (case_sens != 0)
        select_flags |= SELECT_MATCH_CASE;
    if (files_only != 0)
        select_flags |= SELECT_FILES_ONLY;
    if (shell_patterns != 0)
        select_flags |= SELECT_SHELL_PATTERNS;
}

/* --------------------------------------------------------------------------------------------- */

static int
compare_files (char *name1, char *name2, off_t size)
{
    int file1, file2;
    int result = -1;            /* Different by default */

    if (size == 0)
        return 0;

    file1 = open (name1, O_RDONLY);
    if (file1 >= 0)
    {
        file2 = open (name2, O_RDONLY);
        if (file2 >= 0)
        {
#ifdef HAVE_MMAP
            char *data1, *data2;
            /* Ugly if jungle */
            data1 = mmap (0, size, PROT_READ, MAP_FILE | MAP_PRIVATE, file1, 0);
            if (data1 != (char *) -1)
            {
                data2 = mmap (0, size, PROT_READ, MAP_FILE | MAP_PRIVATE, file2, 0);
                if (data2 != (char *) -1)
                {
                    rotate_dash ();
                    result = memcmp (data1, data2, size);
                    munmap (data2, size);
                }
                munmap (data1, size);
            }
#else
            /* Don't have mmap() :( Even more ugly :) */
            char buf1[BUFSIZ], buf2[BUFSIZ];
            int n1, n2;
            rotate_dash ();
            do
            {
                while ((n1 = read (file1, buf1, BUFSIZ)) == -1 && errno == EINTR);
                while ((n2 = read (file2, buf2, BUFSIZ)) == -1 && errno == EINTR);
            }
            while (n1 == n2 && n1 == BUFSIZ && !memcmp (buf1, buf2, BUFSIZ));
            result = (n1 != n2) || memcmp (buf1, buf2, n1);
#endif /* !HAVE_MMAP */
            close (file2);
        }
        close (file1);
    }
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static void
compare_dir (WPanel * panel, WPanel * other, enum CompareMode mode)
{
    int i, j;
    char *src_name, *dst_name;

    /* No marks by default */
    panel->marked = 0;
    panel->total = 0;
    panel->dirs_marked = 0;

    /* Handle all files in the panel */
    for (i = 0; i < panel->count; i++)
    {
        file_entry *source = &panel->dir.list[i];

        /* Default: unmarked */
        file_mark (panel, i, 0);

        /* Skip directories */
        if (S_ISDIR (source->st.st_mode))
            continue;

        /* Search the corresponding entry from the other panel */
        for (j = 0; j < other->count; j++)
        {
            if (strcmp (source->fname, other->dir.list[j].fname) == 0)
                break;
        }
        if (j >= other->count)
            /* Not found -> mark */
            do_file_mark (panel, i, 1);
        else
        {
            /* Found */
            file_entry *target = &other->dir.list[j];

            if (mode != compare_size_only)
            {
                /* Older version is not marked */
                if (source->st.st_mtime < target->st.st_mtime)
                    continue;
            }

            /* Newer version with different size is marked */
            if (source->st.st_size != target->st.st_size)
            {
                do_file_mark (panel, i, 1);
                continue;

            }
            if (mode == compare_size_only)
                continue;

            if (mode == compare_quick)
            {
                /* Thorough compare off, compare only time stamps */
                /* Mark newer version, don't mark version with the same date */
                if (source->st.st_mtime > target->st.st_mtime)
                {
                    do_file_mark (panel, i, 1);
                }
                continue;
            }

            /* Thorough compare on, do byte-by-byte comparison */
            src_name = concat_dir_and_file (panel->cwd, source->fname);
            dst_name = concat_dir_and_file (other->cwd, target->fname);
            if (compare_files (src_name, dst_name, source->st.st_size))
                do_file_mark (panel, i, 1);
            g_free (src_name);
            g_free (dst_name);
        }
    }                           /* for (i ...) */
}

/* --------------------------------------------------------------------------------------------- */

static void
do_link (link_type_t link_type, const char *fname)
{
    char *dest = NULL, *src = NULL;

    if (link_type == LINK_HARDLINK)
    {
        src = g_strdup_printf (_("Link %s to:"), str_trunc (fname, 46));
        dest = input_expand_dialog (_("Link"), src, MC_HISTORY_FM_LINK, "");
        if (!dest || !*dest)
            goto cleanup;
        save_cwds_stat ();
        if (-1 == mc_link (fname, dest))
            message (D_ERROR, MSG_ERROR, _("link: %s"), unix_error_string (errno));
    }
    else
    {
        char *s;
        char *d;

        /* suggest the full path for symlink, and either the full or
           relative path to the file it points to  */
        s = concat_dir_and_file (current_panel->cwd, fname);

        if (get_other_type () == view_listing)
            d = concat_dir_and_file (other_panel->cwd, fname);
        else
            d = g_strdup (fname);

        if (link_type == LINK_SYMLINK_RELATIVE)
            s = diff_two_paths (other_panel->cwd, s);

        symlink_dialog (s, d, &dest, &src);
        g_free (d);
        g_free (s);

        if (!dest || !*dest || !src || !*src)
            goto cleanup;
        save_cwds_stat ();
        if (-1 == mc_symlink (dest, src))
            message (D_ERROR, MSG_ERROR, _("symlink: %s"), unix_error_string (errno));
    }

    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();

  cleanup:
    g_free (src);
    g_free (dest);
}

/* --------------------------------------------------------------------------------------------- */

#if defined(ENABLE_VFS_UNDELFS) || defined(ENABLE_VFS_NET)

static const char *
transform_prefix (const char *prefix)
{
    static char buffer[BUF_TINY];
    size_t prefix_len = strlen (prefix);

    if (prefix_len < 3)
        return prefix;

    strcpy (buffer, prefix + 2);
    strcpy (buffer + prefix_len - 3, VFS_PATH_URL_DELIMITER);
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */

static void
nice_cd (const char *text, const char *xtext, const char *help,
         const char *history_name, const char *prefix, int to_home)
{
    char *machine;
    char *cd_path;

    if (!SELECTED_IS_PANEL)
        return;

    machine = input_dialog_help (text, xtext, help, history_name, "");
    if (!machine)
        return;

    to_home = 0;                /* FIXME: how to solve going to home nicely? /~/ is
                                   ugly as hell and leads to problems in vfs layer */

    /* default prefix in old-style format. */
    if (strncmp (prefix, machine, strlen (prefix)) != 0)
        prefix = transform_prefix (prefix);     /* Convert prefix to URL-style format */

    if (strncmp (prefix, machine, strlen (prefix)) == 0)
        cd_path = g_strconcat (machine, to_home ? "/~/" : (char *) NULL, (char *) NULL);
    else
        cd_path = g_strconcat (prefix, machine, to_home ? "/~/" : (char *) NULL, (char *) NULL);

    if (*cd_path != PATH_SEP)
    {
        char *tmp = cd_path;
        cd_path = g_strconcat (PATH_SEP_STR, tmp, (char *) NULL);
        g_free (tmp);
    }

    if (!do_panel_cd (MENU_PANEL, cd_path, cd_parse_command))
        message (D_ERROR, MSG_ERROR, _("Cannot chdir to \"%s\""), cd_path);
    g_free (cd_path);
    g_free (machine);
}
#endif /* ENABLE_VFS_UNDELFS || ENABLE_VFS_NET */

/* --------------------------------------------------------------------------------------------- */

static void
configure_panel_listing (WPanel * p, int list_type, int use_msformat, char *user, char *status)
{
    p->user_mini_status = use_msformat;
    p->list_type = list_type;

    if (list_type == list_user || use_msformat)
    {
        g_free (p->user_format);
        p->user_format = user;

        g_free (p->user_status_format[list_type]);
        p->user_status_format[list_type] = status;

        set_panel_formats (p);
    }
    else
    {
        g_free (user);
        g_free (status);
    }

    set_panel_formats (p);
    do_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

static void
switch_to_listing (int panel_index)
{
    if (get_display_type (panel_index) != view_listing)
        set_display_type (panel_index, view_listing);
}

/* --------------------------------------------------------------------------------------------- */
/** Handle the tree internal listing modes switching */

static gboolean
set_basic_panel_listing_to (int panel_index, int listing_mode)
{
    WPanel *p = (WPanel *) get_panel_widget (panel_index);
    gboolean ok;

    switch_to_listing (panel_index);
    p->list_type = listing_mode;

    ok = set_panel_formats (p) == 0;

    if (ok)
        do_refresh ();

    return ok;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
view_file_at_line (const char *filename, int plain_view, int internal, int start_line)
{
    static const char *viewer = NULL;
    int move_dir = 0;

    if (plain_view)
    {
        int changed_hex_mode = 0;
        int changed_nroff_flag = 0;
        int changed_magic_flag = 0;

        mcview_altered_hex_mode = 0;
        mcview_altered_nroff_flag = 0;
        mcview_altered_magic_flag = 0;
        if (mcview_default_hex_mode)
            changed_hex_mode = 1;
        if (mcview_default_nroff_flag)
            changed_nroff_flag = 1;
        if (mcview_default_magic_flag)
            changed_magic_flag = 1;
        mcview_default_hex_mode = 0;
        mcview_default_nroff_flag = 0;
        mcview_default_magic_flag = 0;

        switch (mcview_viewer (NULL, filename, start_line))
        {
        case MCVIEW_WANT_NEXT:
            move_dir = 1;
            break;
        case MCVIEW_WANT_PREV:
            move_dir = -1;
            break;
        default:
            move_dir = 0;
        }

        if (changed_hex_mode && !mcview_altered_hex_mode)
            mcview_default_hex_mode = 1;
        if (changed_nroff_flag && !mcview_altered_nroff_flag)
            mcview_default_nroff_flag = 1;
        if (changed_magic_flag && !mcview_altered_magic_flag)
            mcview_default_magic_flag = 1;

        dialog_switch_process_pending ();
    }
    else if (internal)
    {
        char view_entry[BUF_TINY];

        if (start_line != 0)
            g_snprintf (view_entry, sizeof (view_entry), "View:%d", start_line);
        else
            strcpy (view_entry, "View");

        if (regex_command (filename, view_entry, &move_dir) == 0)
        {
            switch (mcview_viewer (NULL, filename, start_line))
            {
            case MCVIEW_WANT_NEXT:
                move_dir = 1;
                break;
            case MCVIEW_WANT_PREV:
                move_dir = -1;
                break;
            default:
                move_dir = 0;
            }

            dialog_switch_process_pending ();
        }
    }
    else
    {
        if (viewer == NULL)
        {
            viewer = getenv ("VIEWER");
            if (viewer == NULL)
                viewer = getenv ("PAGER");
            if (viewer == NULL)
                viewer = "view";
        }

        execute_with_vfs_arg (viewer, filename);
    }

    return move_dir;
}

/* --------------------------------------------------------------------------------------------- */
/** view_file (filename, plain_view, internal)
 *
 * Inputs:
 *   filename:   The file name to view
 *   plain_view: If set does not do any fancy pre-processing (no filtering) and
 *               always invokes the internal viewer.
 *   internal:   If set uses the internal viewer, otherwise an external viewer.
 */

int
view_file (const char *filename, int plain_view, int internal)
{
    return view_file_at_line (filename, plain_view, internal, 0);
}


/* --------------------------------------------------------------------------------------------- */
/** Run user's preferred viewer on the currently selected file */

void
view_cmd (void)
{
    do_view_cmd (FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/** Ask for file and run user's preferred viewer on it */

void
view_file_cmd (void)
{
    char *filename;

    filename =
        input_expand_dialog (_("View file"), _("Filename:"),
                             MC_HISTORY_FM_VIEW_FILE, selection (current_panel)->fname);
    if (!filename)
        return;

    view_file (filename, 0, use_internal_view);
    g_free (filename);
}

/* --------------------------------------------------------------------------------------------- */
/** Run plain internal viewer on the currently selected file */
void
view_raw_cmd (void)
{
    do_view_cmd (TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
view_filtered_cmd (void)
{
    char *command;
    const char *initial_command;

    if (cmdline->buffer[0] == '\0')
        initial_command = selection (current_panel)->fname;
    else
        initial_command = cmdline->buffer;

    command =
        input_dialog (_("Filtered view"),
                      _("Filter command and arguments:"),
                      MC_HISTORY_FM_FILTERED_VIEW, initial_command);

    if (command != NULL)
    {
        mcview_viewer (command, "", 0);
        g_free (command);
        dialog_switch_process_pending ();
    }
}

/* --------------------------------------------------------------------------------------------- */

void
do_edit_at_line (const char *what, gboolean internal, int start_line)
{
    static const char *editor = NULL;

#ifdef USE_INTERNAL_EDIT
    if (internal)
        edit_file (what, start_line);
    else
#else
    (void) start_line;
#endif /* USE_INTERNAL_EDIT */
    {
        if (editor == NULL)
        {
            editor = getenv ("EDITOR");
            if (editor == NULL)
                editor = get_default_editor ();
        }
        execute_with_vfs_arg (editor, what);
    }

    if (mc_global.mc_run_mode == MC_RUN_FULL)
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);

#ifdef USE_INTERNAL_EDIT
    if (use_internal_edit)
        dialog_switch_process_pending ();
    else
#endif /* USE_INTERNAL_EDIT */
        repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

void
edit_cmd (void)
{
    if (regex_command (selection (current_panel)->fname, "Edit", NULL) == 0)
        do_edit (selection (current_panel)->fname);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef USE_INTERNAL_EDIT
void
edit_cmd_force_internal (void)
{
    if (regex_command (selection (current_panel)->fname, "Edit", NULL) == 0)
        do_edit_at_line (selection (current_panel)->fname, TRUE, 0);
}
#endif

/* --------------------------------------------------------------------------------------------- */

void
edit_cmd_new (void)
{
#if HAVE_CHARSET
    mc_global.source_codepage = default_source_codepage;
#endif
    do_edit (NULL);
}

/* --------------------------------------------------------------------------------------------- */
/** Invoked by F5.  Copy, default to the other panel.  */

void
copy_cmd (void)
{
    save_cwds_stat ();
    if (panel_operate (current_panel, OP_COPY, FALSE))
    {
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
        repaint_screen ();
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Invoked by F6.  Move/rename, default to the other panel, ignore marks.  */

void
rename_cmd (void)
{
    save_cwds_stat ();
    if (panel_operate (current_panel, OP_MOVE, FALSE))
    {
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
        repaint_screen ();
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Invoked by F15.  Copy, default to the same panel, ignore marks.  */

void
copy_cmd_local (void)
{
    save_cwds_stat ();
    if (panel_operate (current_panel, OP_COPY, TRUE))
    {
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
        repaint_screen ();
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Invoked by F16.  Move/rename, default to the same panel.  */

void
rename_cmd_local (void)
{
    save_cwds_stat ();
    if (panel_operate (current_panel, OP_MOVE, TRUE))
    {
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
        repaint_screen ();
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mkdir_cmd (void)
{
    char *dir, *absdir;
    const char *name = "";

    /* If 'on' then automatically fills name with current selected item name */
    if (auto_fill_mkdir_name && strcmp (selection (current_panel)->fname, "..") != 0)
        name = selection (current_panel)->fname;

    dir =
        input_expand_dialog (_("Create a new Directory"),
                             _("Enter directory name:"), MC_HISTORY_FM_MKDIR, name);

    if (!dir)
        return;

    if (*dir)
    {
        if (dir[0] == '/' || dir[0] == '~')
            absdir = g_strdup (dir);
        else
            absdir = concat_dir_and_file (current_panel->cwd, dir);

        save_cwds_stat ();
        if (my_mkdir (absdir, 0777) == 0)
        {
            update_panels (UP_OPTIMIZE, dir);
            repaint_screen ();
            select_item (current_panel);
        }
        else
        {
            message (D_ERROR, MSG_ERROR, "%s", unix_error_string (errno));
        }
        g_free (absdir);
    }
    g_free (dir);
}

/* --------------------------------------------------------------------------------------------- */

void
delete_cmd (void)
{
    save_cwds_stat ();

    if (panel_operate (current_panel, OP_DELETE, FALSE))
    {
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
        repaint_screen ();
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Invoked by F18.  Remove selected file, regardless of marked files.  */

void
delete_cmd_local (void)
{
    save_cwds_stat ();

    if (panel_operate (current_panel, OP_DELETE, TRUE))
    {
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
        repaint_screen ();
    }
}

/* --------------------------------------------------------------------------------------------- */

void
find_cmd (void)
{
    find_file ();
}

/* --------------------------------------------------------------------------------------------- */
/** Invoked from the left/right menus */

void
filter_cmd (void)
{
    WPanel *p;

    if (!SELECTED_IS_PANEL)
        return;

    p = MENU_PANEL;
    set_panel_filter (p);
}

/* --------------------------------------------------------------------------------------------- */

void
reread_cmd (void)
{
    panel_update_flags_t flag = UP_ONLY_CURRENT;

    if (get_current_type () == view_listing && get_other_type () == view_listing
        && strcmp (current_panel->cwd, other_panel->cwd) == 0)
        flag = UP_OPTIMIZE;

    update_panels (UP_RELOAD | flag, UP_KEEPSEL);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

void
select_invert_cmd (void)
{
    int i;
    file_entry *file;

    for (i = 0; i < current_panel->count; i++)
    {
        file = &current_panel->dir.list[i];
        if (!panels_options.reverse_files_only || !S_ISDIR (file->st.st_mode))
            do_file_mark (current_panel, i, !file->f.marked);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
select_cmd (void)
{
    select_unselect_cmd (_("Select"), ":select_cmd: Select ", TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
unselect_cmd (void)
{
    select_unselect_cmd (_("Unselect"), ":unselect_cmd: Unselect ", FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
ext_cmd (void)
{
    char *buffer;
    char *extdir;
    int dir;

    dir = 0;
    if (geteuid () == 0)
    {
        dir = query_dialog (_("Extension file edit"),
                            _("Which extension file you want to edit?"), D_NORMAL, 2,
                            _("&User"), _("&System Wide"));
    }
    extdir = concat_dir_and_file (mc_global.sysconfig_dir, MC_LIB_EXT);

    if (dir == 0)
    {
        buffer = g_build_filename (mc_config_get_data_path (), MC_FILEBIND_FILE, NULL);
        check_for_default (extdir, buffer);
        do_edit (buffer);
        g_free (buffer);
    }
    else if (dir == 1)
    {
        if (!exist_file (extdir))
        {
            g_free (extdir);
            extdir = concat_dir_and_file (mc_global.share_data_dir, MC_LIB_EXT);
        }
        do_edit (extdir);
    }
    g_free (extdir);
    flush_extension_file ();
}

/* --------------------------------------------------------------------------------------------- */
/** edit file menu for mc */

void
edit_mc_menu_cmd (void)
{
    char *buffer;
    char *menufile;
    int dir = 0;

    dir = query_dialog (_("Menu edit"),
                        _("Which menu file do you want to edit?"),
                        D_NORMAL, geteuid ()? 2 : 3, _("&Local"), _("&User"), _("&System Wide"));

    menufile = concat_dir_and_file (mc_global.sysconfig_dir, MC_GLOBAL_MENU);

    if (!exist_file (menufile))
    {
        g_free (menufile);
        menufile = concat_dir_and_file (mc_global.share_data_dir, MC_GLOBAL_MENU);
    }

    switch (dir)
    {
    case 0:
        buffer = g_strdup (MC_LOCAL_MENU);
        check_for_default (menufile, buffer);
        chmod (buffer, 0600);
        break;

    case 1:
        buffer = g_build_filename (mc_config_get_data_path (), MC_USERMENU_FILE, NULL);
        check_for_default (menufile, buffer);
        break;

    case 2:
        buffer = concat_dir_and_file (mc_global.sysconfig_dir, MC_GLOBAL_MENU);
        if (!exist_file (buffer))
        {
            g_free (buffer);
            buffer = concat_dir_and_file (mc_global.share_data_dir, MC_GLOBAL_MENU);
        }
        break;

    default:
        g_free (menufile);
        return;
    }

    do_edit (buffer);

    g_free (buffer);
    g_free (menufile);
}

/* --------------------------------------------------------------------------------------------- */

void
edit_fhl_cmd (void)
{
    char *buffer = NULL;
    char *fhlfile = NULL;

    int dir;

    dir = 0;
    if (geteuid () == 0)
    {
        dir = query_dialog (_("Highlighting groups file edit"),
                            _("Which highlighting file you want to edit?"), D_NORMAL, 2,
                            _("&User"), _("&System Wide"));
    }
    fhlfile = concat_dir_and_file (mc_global.sysconfig_dir, MC_FHL_INI_FILE);

    if (dir == 0)
    {
        buffer = g_build_filename (mc_config_get_path (), MC_FHL_INI_FILE, NULL);
        check_for_default (fhlfile, buffer);
        do_edit (buffer);
        g_free (buffer);
    }
    else if (dir == 1)
    {
        if (!exist_file (fhlfile))
        {
            g_free (fhlfile);
            fhlfile = concat_dir_and_file (mc_global.sysconfig_dir, MC_FHL_INI_FILE);
        }
        do_edit (fhlfile);
    }
    g_free (fhlfile);

    /* refresh highlighting rules */
    mc_fhl_free (&mc_filehighlight);
    mc_filehighlight = mc_fhl_new (TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
hotlist_cmd (void)
{
    char *target;

    target = hotlist_show (LIST_HOTLIST);
    if (!target)
        return;

    if (get_current_type () == view_tree)
        tree_chdir (the_tree, target);
    else
    {
        char *cmd = g_strconcat ("cd ", target, (char *) NULL);
        do_cd_command (cmd);
        g_free (cmd);
    }
    g_free (target);
}

#ifdef ENABLE_VFS
void
vfs_list (void)
{
    char *target;

    target = hotlist_show (LIST_VFSLIST);
    if (!target)
        return;

    if (!do_cd (target, cd_exact))
        message (D_ERROR, MSG_ERROR, _("Cannot change directory"));
    g_free (target);
}
#endif /* ENABLE_VFS */

/* --------------------------------------------------------------------------------------------- */

void
compare_dirs_cmd (void)
{
    int choice;
    enum CompareMode thorough_flag;

    choice =
        query_dialog (_("Compare directories"),
                      _("Select compare method:"), D_NORMAL, 4,
                      _("&Quick"), _("&Size only"), _("&Thorough"), _("&Cancel"));

    if (choice < 0 || choice > 2)
        return;

    thorough_flag = choice;

    if (get_current_type () == view_listing && get_other_type () == view_listing)
    {
        compare_dir (current_panel, other_panel, thorough_flag);
        compare_dir (other_panel, current_panel, thorough_flag);
    }
    else
    {
        message (D_ERROR, MSG_ERROR,
                 _("Both panels should be in the listing mode\nto use this command"));
    }
}

/* --------------------------------------------------------------------------------------------- */

#ifdef USE_DIFF_VIEW
void
diff_view_cmd (void)
{
    dview_diff_cmd ();

    if (mc_global.mc_run_mode == MC_RUN_FULL)
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);

    dialog_switch_process_pending ();
}
#endif

/* --------------------------------------------------------------------------------------------- */

void
swap_cmd (void)
{
    swap_panels ();
    tty_touch_screen ();
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

void
view_other_cmd (void)
{
    static int message_flag = TRUE;

    if (!mc_global.tty.xterm_flag && mc_global.tty.console_flag == '\0'
        && !mc_global.tty.use_subshell && !output_starts_shell)
    {
        if (message_flag)
            message (D_ERROR, MSG_ERROR,
                     _("Not an xterm or Linux console;\nthe panels cannot be toggled."));
        message_flag = FALSE;
    }
    else
    {
        toggle_panels ();
    }
}

/* --------------------------------------------------------------------------------------------- */

void
link_cmd (link_type_t link_type)
{
    char *filename = selection (current_panel)->fname;

    if (filename != NULL)
        do_link (link_type, filename);
}

/* --------------------------------------------------------------------------------------------- */

void
edit_symlink_cmd (void)
{
    if (S_ISLNK (selection (current_panel)->st.st_mode))
    {
        char buffer[MC_MAXPATHLEN];
        char *p = NULL;
        int i;
        char *dest, *q;

        p = selection (current_panel)->fname;

        q = g_strdup_printf (_("Symlink `%s\' points to:"), str_trunc (p, 32));

        i = readlink (p, buffer, MC_MAXPATHLEN - 1);
        if (i > 0)
        {
            buffer[i] = 0;
            dest = input_expand_dialog (_("Edit symlink"), q, MC_HISTORY_FM_EDIT_LINK, buffer);
            if (dest)
            {
                if (*dest && strcmp (buffer, dest))
                {
                    save_cwds_stat ();
                    if (-1 == mc_unlink (p))
                    {
                        message (D_ERROR, MSG_ERROR, _("edit symlink, unable to remove %s: %s"),
                                 p, unix_error_string (errno));
                    }
                    else
                    {
                        if (-1 == mc_symlink (dest, p))
                            message (D_ERROR, MSG_ERROR, _("edit symlink: %s"),
                                     unix_error_string (errno));
                    }
                    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
                    repaint_screen ();
                }
                g_free (dest);
            }
        }
        g_free (q);
    }
    else
    {
        message (D_ERROR, MSG_ERROR, _("`%s' is not a symbolic link"),
                 selection (current_panel)->fname);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
help_cmd (void)
{
    ev_help_t event_data = { NULL, NULL };

    if (current_panel->searching)
        event_data.node = "[Quick search]";
    else
        event_data.node = "[main]";

    mc_event_raise (MCEVENT_GROUP_CORE, "help", &event_data);
}

/* --------------------------------------------------------------------------------------------- */

void
user_file_menu_cmd (void)
{
    (void) user_menu_cmd (NULL, NULL, -1);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Return a random hint.  If force is not 0, ignore the timeout.
 */

char *
get_random_hint (int force)
{
    char *data, *result = NULL, *eol;
    int len;
    int start;
    static int last_sec;
    static struct timeval tv;
    GIConv conv;
    GString *buffer;

    /* Do not change hints more often than one minute */
    gettimeofday (&tv, NULL);
    if (!force && !(tv.tv_sec > last_sec + 60))
        return g_strdup ("");
    last_sec = tv.tv_sec;

    data = load_mc_home_file (mc_global.share_data_dir, MC_HINT, NULL);
    if (data == NULL)
        return NULL;

    /* get a random entry */
    srand (tv.tv_sec);
    len = strlen (data);
    start = rand () % len;

    for (; start != 0; start--)
        if (data[start] == '\n')
        {
            start++;
            break;
        }

    eol = strchr (data + start, '\n');
    if (eol != NULL)
        *eol = '\0';

    /* hint files are stored in utf-8 */
    /* try convert hint file from utf-8 to terminal encoding */
    conv = str_crt_conv_from ("UTF-8");
    if (conv != INVALID_CONV)
    {
        buffer = g_string_new ("");
        if (str_convert (conv, &data[start], buffer) != ESTR_FAILURE)
            result = g_strdup (buffer->str);
        g_string_free (buffer, TRUE);
        str_close_conv (conv);
    }

    g_free (data);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS_NET
#ifdef ENABLE_VFS_FTP
void
ftplink_cmd (void)
{
    nice_cd (_("FTP to machine"), _(machine_str),
             "[FTP File System]", ":ftplink_cmd: FTP to machine ", "/#ftp:", 1);
}
#endif /* ENABLE_VFS_FTP */

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS_FISH
void
fishlink_cmd (void)
{
    nice_cd (_("Shell link to machine"), _(machine_str),
             "[FIle transfer over SHell filesystem]", ":fishlink_cmd: Shell link to machine ",
             "/#sh:", 1);
}
#endif /* ENABLE_VFS_FISH */

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS_SMB
void
smblink_cmd (void)
{
    nice_cd (_("SMB link to machine"), _(machine_str),
             "[SMB File System]", ":smblink_cmd: SMB link to machine ", "/#smb:", 0);
}
#endif /* ENABLE_VFS_SMB */
#endif /* ENABLE_VFS_NET */

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS_UNDELFS
void
undelete_cmd (void)
{
    nice_cd (_("Undelete files on an ext2 file system"),
             _("Enter device (without /dev/) to undelete\nfiles on: (F1 for details)"),
             "[Undelete File System]", ":undelete_cmd: Undel on ext2 fs ", "/#undel:", 0);
}
#endif /* ENABLE_VFS_UNDELFS */

/* --------------------------------------------------------------------------------------------- */

void
quick_cd_cmd (void)
{
    char *p = cd_dialog ();

    if (p && *p)
    {
        char *q = g_strconcat ("cd ", p, (char *) NULL);

        do_cd_command (q);
        g_free (q);
    }
    g_free (p);
}

/* --------------------------------------------------------------------------------------------- */
/*!
   \brief calculate dirs sizes

   calculate dirs sizes and resort panel:
   dirs_selected = show size for selected dirs,
   otherwise = show size for dir under cursor:
   dir under cursor ".." = show size for all dirs,
   otherwise = show size for dir under cursor
 */

void
smart_dirsize_cmd (void)
{
    WPanel *panel = current_panel;
    file_entry *entry;

    entry = &(panel->dir.list[panel->selected]);
    if ((S_ISDIR (entry->st.st_mode) && (strcmp (entry->fname, "..") == 0)) || panel->dirs_marked)
        dirsizes_cmd ();
    else
        single_dirsize_cmd ();
}

/* --------------------------------------------------------------------------------------------- */

void
single_dirsize_cmd (void)
{
    WPanel *panel = current_panel;
    file_entry *entry;

    entry = &(panel->dir.list[panel->selected]);
    if (S_ISDIR (entry->st.st_mode) && strcmp (entry->fname, "..") != 0)
    {
        size_t marked = 0;
        uintmax_t total = 0;
        ComputeDirSizeUI *ui;

        ui = compute_dir_size_create_ui ();

        if (compute_dir_size (entry->fname, ui, compute_dir_size_update_ui,
                              &marked, &total, TRUE) == FILE_CONT)
        {
            entry->st.st_size = (off_t) total;
            entry->f.dir_size_computed = 1;
        }

        compute_dir_size_destroy_ui (ui);
    }

    if (panels_options.mark_moves_down)
        send_message ((Widget *) panel, WIDGET_COMMAND, CK_Down);

    recalculate_panel_summary (panel);

    if (current_panel->sort_info.sort_field->sort_routine == (sortfn *) sort_size)
        panel_re_sort (panel);

    panel->dirty = 1;
}

/* --------------------------------------------------------------------------------------------- */

void
dirsizes_cmd (void)
{
    WPanel *panel = current_panel;
    int i;
    ComputeDirSizeUI *ui;

    ui = compute_dir_size_create_ui ();

    for (i = 0; i < panel->count; i++)
        if (S_ISDIR (panel->dir.list[i].st.st_mode)
            && ((panel->dirs_marked && panel->dir.list[i].f.marked)
                || !panel->dirs_marked) && strcmp (panel->dir.list[i].fname, "..") != 0)
        {
            size_t marked = 0;
            uintmax_t total = 0;

            if (compute_dir_size (panel->dir.list[i].fname,
                                  ui, compute_dir_size_update_ui, &marked, &total,
                                  TRUE) != FILE_CONT)
                break;

            panel->dir.list[i].st.st_size = (off_t) total;
            panel->dir.list[i].f.dir_size_computed = 1;
        }

    compute_dir_size_destroy_ui (ui);

    recalculate_panel_summary (panel);

    if (current_panel->sort_info.sort_field->sort_routine == (sortfn *) sort_size)
        panel_re_sort (panel);

    panel->dirty = 1;
}

/* --------------------------------------------------------------------------------------------- */

void
save_setup_cmd (void)
{
    char *d1;
    const char *d2;

    d1 = g_build_filename (mc_config_get_path (), MC_CONFIG_FILE, (char *) NULL);
    d2 = strip_home_and_password (d1);
    g_free (d1);

    if (save_setup (TRUE, TRUE))
        message (D_NORMAL, _("Setup"), _("Setup saved to %s"), d2);
    else
        message (D_ERROR, _("Setup"), _("Unable to save setup to %s"), d2);
}

/* --------------------------------------------------------------------------------------------- */

void
info_cmd_no_menu (void)
{
    if (get_display_type (0) == view_info)
        set_display_type (0, view_listing);
    else if (get_display_type (1) == view_info)
        set_display_type (1, view_listing);
    else
        set_display_type (current_panel == left_panel ? 1 : 0, view_info);
}

/* --------------------------------------------------------------------------------------------- */

void
quick_cmd_no_menu (void)
{
    if (get_display_type (0) == view_quick)
        set_display_type (0, view_listing);
    else if (get_display_type (1) == view_quick)
        set_display_type (1, view_listing);
    else
        set_display_type (current_panel == left_panel ? 1 : 0, view_quick);
}

/* --------------------------------------------------------------------------------------------- */

void
listing_cmd (void)
{
    switch_to_listing (MENU_PANEL_IDX);
}

/* --------------------------------------------------------------------------------------------- */

void
change_listing_cmd (void)
{
    int list_type;
    int use_msformat;
    char *user, *status;
    WPanel *p = NULL;

    if (get_display_type (MENU_PANEL_IDX) == view_listing)
        p = MENU_PANEL_IDX == 0 ? left_panel : right_panel;

    list_type = display_box (p, &user, &status, &use_msformat, MENU_PANEL_IDX);

    if (list_type != -1)
    {
        switch_to_listing (MENU_PANEL_IDX);
        p = MENU_PANEL_IDX == 0 ? left_panel : right_panel;
        configure_panel_listing (p, list_type, use_msformat, user, status);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
panel_tree_cmd (void)
{
    set_display_type (MENU_PANEL_IDX, view_tree);
}

/* --------------------------------------------------------------------------------------------- */

void
info_cmd (void)
{
    set_display_type (MENU_PANEL_IDX, view_info);
}

/* --------------------------------------------------------------------------------------------- */

void
quick_view_cmd (void)
{
    if ((WPanel *) get_panel_widget (MENU_PANEL_IDX) == current_panel)
        change_panel ();
    set_display_type (MENU_PANEL_IDX, view_quick);
}

/* --------------------------------------------------------------------------------------------- */

void
toggle_listing_cmd (void)
{
    int current = get_current_index ();
    WPanel *p = (WPanel *) get_panel_widget (current);

    set_basic_panel_listing_to (current, (p->list_type + 1) % LIST_TYPES);
}

/* --------------------------------------------------------------------------------------------- */

void
encoding_cmd (void)
{
    if (SELECTED_IS_PANEL)
        panel_change_encoding (MENU_PANEL);
}

/* --------------------------------------------------------------------------------------------- */
