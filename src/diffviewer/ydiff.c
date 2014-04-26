/*
   Copyright (C) 2007-2015
   Free Software Foundation, Inc.

   Written by:
   Daniel Borca <dborca@yahoo.com>, 2007
   Slava Zanko <slavazanko@gmail.com>, 2010, 2013, 2014, 2015
   Andrew Borodin <aborodin@vmail.ru>, 2010, 2012, 2013, 2015
   Ilia Maslakov <il.smind@gmail.com>, 2010

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


#include <config.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/color.h"
#include "lib/tty/key.h"
#include "lib/skin.h"           /* EDITOR_NORMAL_COLOR */
#include "lib/widget.h"
#include "lib/strutil.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif
#include "lib/event.h"          /* mc_event_raise() */

#include "src/filemanager/cmd.h"        /* edit_file_at_line(), view_other_cmd() */
#include "src/filemanager/panel.h"

#include "src/keybind-defaults.h"
#include "src/setup.h"
#include "src/history.h"

#include "ydiff.h"
#include "internal.h"

#ifdef HAVE_CHARSET
#include "charset.h"
#endif

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/* view routines and callbacks ********************************************** */

static void
dview_compute_areas (WDiff * dview)
{
    dview->height = LINES - 2;
    dview->half1 = COLS / 2;
    dview->half2 = COLS - dview->half1;

    mc_diffviewer_compute_split (dview, 0);
}

/* --------------------------------------------------------------------------------------------- */

static int
dview_init (WDiff * dview, const char *args, const char *file1, const char *file2,
            const char *label1, const char *label2, DSRC dsrc)
{
    int ndiff;
    FBUF *f[DIFF_COUNT];

    f[DIFF_LEFT] = NULL;
    f[DIFF_RIGHT] = NULL;

    if (dsrc == DATA_SRC_TMP)
    {
        f[DIFF_LEFT] = mc_diffviewer_file_temp ();
        if (f[DIFF_LEFT] == NULL)
            return -1;

        f[DIFF_RIGHT] = mc_diffviewer_file_temp ();
        if (f[DIFF_RIGHT] == NULL)
        {
            mc_diffviewer_file_close (f[DIFF_LEFT]);
            return -1;
        }
    }
    else if (dsrc == DATA_SRC_ORG)
    {
        f[DIFF_LEFT] = mc_diffviewer_file_open (file1, O_RDONLY);
        if (f[DIFF_LEFT] == NULL)
            return -1;

        f[DIFF_RIGHT] = mc_diffviewer_file_open (file2, O_RDONLY);
        if (f[DIFF_RIGHT] == NULL)
        {
            mc_diffviewer_file_close (f[DIFF_LEFT]);
            return -1;
        }
    }

    dview->args = args;
    dview->file[DIFF_LEFT] = file1;
    dview->file[DIFF_RIGHT] = file2;
    dview->label[DIFF_LEFT] = g_strdup (label1);
    dview->label[DIFF_RIGHT] = g_strdup (label2);
    dview->f[DIFF_LEFT] = f[0];
    dview->f[DIFF_RIGHT] = f[1];
    dview->merged[DIFF_LEFT] = FALSE;
    dview->merged[DIFF_RIGHT] = FALSE;
    dview->hdiff = NULL;
    dview->dsrc = dsrc;
#ifdef HAVE_CHARSET
    dview->converter = str_cnv_from_term;
    mc_diffviewer_set_codeset (dview);
#endif
    dview->a[DIFF_LEFT] = g_array_new (FALSE, FALSE, sizeof (DIFFLN));
    dview->a[DIFF_RIGHT] = g_array_new (FALSE, FALSE, sizeof (DIFFLN));

    ndiff = mc_diffviewer_redo_diff (dview);
    if (ndiff < 0)
    {
        /* goto MSG_DESTROY stage: mc_diffviewer_deinit() */
        mc_diffviewer_file_close (f[DIFF_LEFT]);
        mc_diffviewer_file_close (f[DIFF_RIGHT]);
        return -1;
    }

    dview->ndiff = ndiff;

    dview->view_quit = 0;

    dview->bias = 0;
    dview->new_frame = 1;
    dview->skip_rows = 0;
    dview->skip_cols = 0;
    dview->display_symbols = 0;
    dview->display_numbers = 0;
    dview->show_cr = 1;
    dview->tab_size = 8;
    dview->ord = DIFF_LEFT;
    dview->full = 0;

    dview->search.handle = NULL;
    dview->search.last_string = NULL;
    dview->search.last_found_line = -1;
    dview->search.last_accessed_num_line = -1;

    dview->opt.quality = 0;
    dview->opt.strip_trailing_cr = 0;
    dview->opt.ignore_tab_expansion = 0;
    dview->opt.ignore_space_change = 0;
    dview->opt.ignore_all_space = 0;
    dview->opt.ignore_case = 0;

    dview_compute_areas (dview);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_labels (WDiff * dview)
{
    Widget *d;
    WDialog *h;
    WButtonBar *b;

    d = WIDGET (dview);
    h = d->owner;
    b = find_buttonbar (h);

    buttonbar_set_label (b, 1, Q_ ("ButtonBar|Help"), diff_map, d);
    buttonbar_set_label (b, 2, Q_ ("ButtonBar|Save"), diff_map, d);
    buttonbar_set_label (b, 4, Q_ ("ButtonBar|Edit"), diff_map, d);
    buttonbar_set_label (b, 5, Q_ ("ButtonBar|Merge"), diff_map, d);
    buttonbar_set_label (b, 7, Q_ ("ButtonBar|Search"), diff_map, d);
    buttonbar_set_label (b, 9, Q_ ("ButtonBar|Options"), diff_map, d);
    buttonbar_set_label (b, 10, Q_ ("ButtonBar|Quit"), diff_map, d);
}

/* --------------------------------------------------------------------------------------------- */

static int
dview_event (Gpm_Event * event, void *data)
{
    WDiff *dview = (WDiff *) data;

    if (!mouse_global_in_widget (event, data))
        return MOU_UNHANDLED;

    /* We are not interested in release events */
    if ((event->type & (GPM_DOWN | GPM_DRAG)) == 0)
        return MOU_NORMAL;

    /* Wheel events */
    if ((event->buttons & GPM_B_UP) != 0 && (event->type & GPM_DOWN) != 0)
    {
        dview->skip_rows -= 2;
        dview->search.last_accessed_num_line = dview->skip_rows;
        mc_diffviewer_update (dview);
    }
    else if ((event->buttons & GPM_B_DOWN) != 0 && (event->type & GPM_DOWN) != 0)
    {
        dview->skip_rows += 2;
        dview->search.last_accessed_num_line = dview->skip_rows;
        mc_diffviewer_update (dview);
    }

    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Check if it's OK to close the diff viewer.  If there are unsaved changes,
 * ask user.
 */

static gboolean
dview_ok_to_exit (WDiff * dview)
{
    gboolean res = TRUE;
    int act;

    if (!dview->merged[DIFF_LEFT] && !dview->merged[DIFF_RIGHT])
        return res;

    act = query_dialog (_("Quit"), !mc_global.midnight_shutdown ?
                        _("File(s) was modified. Save with exit?") :
                        _("Midnight Commander is being shut down.\nSave modified file(s)?"),
                        D_NORMAL, 2, _("&Yes"), _("&No"));

    /* Esc is No */
    if (mc_global.midnight_shutdown || (act == -1))
        act = 1;

    switch (act)
    {
    case -1:                   /* Esc */
        res = FALSE;
        break;
    case 0:                    /* Yes */
        mc_event_raise (MCEVENT_GROUP_DIFFVIEWER, "save_changes", dview, NULL, NULL);
        res = TRUE;
        break;
    case 1:                    /* No */
        if (mc_util_restore_from_backup_if_possible (dview->file[DIFF_LEFT], "~~~"))
            res = mc_util_unlink_backup_if_possible (dview->file[DIFF_LEFT], "~~~");
        if (mc_util_restore_from_backup_if_possible (dview->file[DIFF_RIGHT], "~~~"))
            res = mc_util_unlink_backup_if_possible (dview->file[DIFF_RIGHT], "~~~");
        /* fall through */
    default:
        res = TRUE;
        break;
    }
    return res;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dview_execute_cmd (WDiff * dview, unsigned long command)
{
    cb_ret_t res = MSG_NOT_HANDLED;
    const char *event_name = NULL;
    const char *event_group_name = MCEVENT_GROUP_DIFFVIEWER;

    switch (command)
    {
    case CK_ShowSymbols:
        event_name = "show_symbols";
        break;
    case CK_ShowNumbers:
        event_name = "show_numbers";
        break;
    case CK_SplitFull:
        event_name = "split_full";
        break;
    case CK_SplitEqual:
        event_name = "split_equal";
        break;
    case CK_SplitMore:
        event_name = "split_more";
        break;
    case CK_SplitLess:
        event_name = "split_less";
        break;
    case CK_Tab2:
        event_name = "tab_size_2";
        break;
    case CK_Tab3:
        event_name = "tab_size_3";
        break;
    case CK_Tab4:
        event_name = "tab_size_4";
        break;
    case CK_Tab8:
        event_name = "tab_size_8";
        break;
    case CK_Swap:
        event_name = "swap";
        break;
    case CK_Redo:
        event_name = "redo";
        break;
    case CK_HunkNext:
        event_name = "hunk_next";
        break;
    case CK_HunkPrev:
        event_name = "hunk_prev";
        break;
    case CK_Goto:
        event_name = "goto_line";
        break;
    case CK_Edit:
        event_name = "edit_current";
        break;
    case CK_Merge:
        event_name = "merge_from_left_to_right";
        break;
    case CK_MergeOther:
        event_name = "merge_from_right_to_left";
        break;
    case CK_EditOther:
        event_name = "edit_other";
        break;
    case CK_Search:
        event_name = "search";
        break;
    case CK_SearchContinue:
        event_name = "continue_search";
        break;
    case CK_Top:
        event_name = "goto_top";
        break;
    case CK_Bottom:
        event_name = "goto_bottom";
        break;
    case CK_Up:
        event_name = "goto_up";
        break;
    case CK_Down:
        event_name = "goto_down";
        break;
    case CK_PageDown:
        event_name = "goto_page_down";
        break;
    case CK_PageUp:
        event_name = "goto_page_up";
        break;
    case CK_Left:
        event_name = "goto_left";
        break;
    case CK_Right:
        event_name = "goto_right";
        break;
    case CK_LeftQuick:
        event_name = "goto_left_quick";
        break;
    case CK_RightQuick:
        event_name = "goto_right_quick";
        break;
    case CK_Home:
        event_name = "goto_start_of_line";
        break;
    case CK_Shell:
        event_group_name = MCEVENT_GROUP_FILEMANAGER;
        event_name = "view_other";
        res = MSG_HANDLED;
        break;
    case CK_Quit:
        event_name = "quit";
        break;
    case CK_Save:
        event_name = "save_changes";
        break;
    case CK_Options:
        event_name = "options_show_dialog";
        break;
#ifdef HAVE_CHARSET
    case CK_SelectCodepage:
        event_name = "select_encoding_show_dialog";
        break;
#endif
    case CK_Cancel:
        /* don't close diffviewer due to SIGINT */
        break;
    default:;
    }

    if (event_name != NULL && mc_event_raise (event_group_name, event_name, dview, NULL, NULL))
        res = MSG_HANDLED;

    return res;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dview_handle_key (WDiff * dview, int key)
{
    unsigned long command;

#ifdef HAVE_CHARSET
    key = convert_from_input_c (key);
#endif

    command = keybind_lookup_keymap_command (diff_map, key);
    if ((command != CK_IgnoreKey) && (dview_execute_cmd (dview, command) == MSG_HANDLED))
        return MSG_HANDLED;

    /* Key not used */
    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dview_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDiff *dview = (WDiff *) w;
    WDialog *h = w->owner;
    cb_ret_t i;

    switch (msg)
    {
    case MSG_INIT:
        dview_labels (dview);
        mc_event_raise (MCEVENT_GROUP_DIFFVIEWER, "options_load", dview, NULL, NULL);
        mc_diffviewer_update (dview);
        return MSG_HANDLED;

    case MSG_DRAW:
        dview->new_frame = 1;
        mc_diffviewer_update (dview);
        return MSG_HANDLED;

    case MSG_KEY:
        i = dview_handle_key (dview, parm);
        if (dview->view_quit)
            dlg_stop (h);
        else
            mc_diffviewer_update (dview);
        return i;

    case MSG_ACTION:
        i = dview_execute_cmd (dview, parm);
        if (dview->view_quit)
            dlg_stop (h);
        else
            mc_diffviewer_update (dview);
        return i;

    case MSG_DESTROY:
        mc_event_raise (MCEVENT_GROUP_DIFFVIEWER, "options_save", dview, NULL, NULL);
        mc_diffviewer_deinit (dview);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
dview_adjust_size (WDialog * h)
{
    WDiff *dview;
    WButtonBar *bar;

    /* Look up the viewer and the buttonbar, we assume only two widgets here */
    dview = (WDiff *) find_widget_type (h, dview_callback);
    bar = find_buttonbar (h);
    widget_set_size (WIDGET (dview), 0, 0, LINES - 1, COLS);
    widget_set_size (WIDGET (bar), LINES - 1, 0, 1, COLS);

    dview_compute_areas (dview);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dview_dialog_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDiff *dview = (WDiff *) data;
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_RESIZE:
        dview_adjust_size (h);
        return MSG_HANDLED;

    case MSG_ACTION:
        /* shortcut */
        if (sender == NULL)
            return dview_execute_cmd (NULL, parm);
        /* message from buttonbar */
        if (sender == WIDGET (find_buttonbar (h)))
        {
            if (data != NULL)
                return send_message (data, NULL, MSG_ACTION, parm, NULL);

            dview = (WDiff *) find_widget_type (h, dview_callback);
            return dview_execute_cmd (dview, parm);
        }
        return MSG_NOT_HANDLED;

    case MSG_VALIDATE:
        dview = (WDiff *) find_widget_type (h, dview_callback);
        h->state = DLG_ACTIVE;  /* don't stop the dialog before final decision */
        if (dview_ok_to_exit (dview))
            h->state = DLG_CLOSED;
        return MSG_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static char *
dview_get_title (const WDialog * h, size_t len)
{
    const WDiff *dview;
    const char *modified = " (*) ";
    const char *notmodified = "     ";
    size_t len1;
    GString *title;

    dview = (const WDiff *) find_widget_type (h, dview_callback);
    len1 = (len - str_term_width1 (_("Diff:")) - strlen (modified) - 3) / 2;

    title = g_string_sized_new (len);
    g_string_append (title, _("Diff:"));
    g_string_append (title, dview->merged[DIFF_LEFT] ? modified : notmodified);
    g_string_append (title, str_term_trim (dview->label[DIFF_LEFT], len1));
    g_string_append (title, " | ");
    g_string_append (title, dview->merged[DIFF_RIGHT] ? modified : notmodified);
    g_string_append (title, str_term_trim (dview->label[DIFF_RIGHT], len1));

    return g_string_free (title, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static int
diff_view (const char *file1, const char *file2, const char *label1, const char *label2)
{
    int error;
    WDiff *dview;
    Widget *w;
    WDialog *dview_dlg;

    /* Create dialog and widgets, put them on the dialog */
    dview_dlg =
        dlg_create (FALSE, 0, 0, LINES, COLS, NULL, dview_dialog_callback, NULL,
                    "[Diff Viewer]", NULL, DLG_WANT_TAB);

    dview = g_new0 (WDiff, 1);
    w = WIDGET (dview);
    widget_init (w, 0, 0, LINES - 1, COLS, dview_callback, dview_event);
    widget_want_cursor (w, FALSE);

    add_widget (dview_dlg, dview);
    add_widget (dview_dlg, buttonbar_new (TRUE));

    dview_dlg->get_title = dview_get_title;

    error = dview_init (dview, "-a", file1, file2, label1, label2, DATA_SRC_MEM);       /* XXX binary diff? */

    /* Please note that if you add another widget,
     * you have to modify dview_adjust_size to
     * be aware of it
     */
    if (error == 0)
        dlg_run (dview_dlg);

    if ((error != 0) || (dview_dlg->state == DLG_CLOSED))
        dlg_destroy (dview_dlg);

    return error == 0 ? 1 : 0;
}

/* --------------------------------------------------------------------------------------------- */

static vfs_path_t *
mc_diffviewer_get_fname_from_wpanel (WPanel * panel)
{
    if (S_ISDIR (selection (panel)->st.st_mode))
    {
        message (D_ERROR, MSG_ERROR, _("\"%s\" is a directory"),
                 path_trunc (selection (panel)->fname, 30));
        return NULL;
    }
    return vfs_path_append_new (panel->cwd_vpath, selection (panel)->fname, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static vfs_path_t *
mc_diffviewer_get_fname_from_string (const char *fname)
{
    struct stat st;
    vfs_path_t *file;

    file = vfs_path_from_str (fname);
    if (mc_stat (file, &st) == 0)
    {
        if (S_ISDIR (st.st_mode))
        {
            message (D_ERROR, MSG_ERROR, _("\"%s\" is a directory"), path_trunc (fname, 30));
            vfs_path_free (file);
            return NULL;
        }
    }
    else
    {
        message (D_ERROR, MSG_ERROR, _("Cannot stat \"%s\"\n%s"),
                 path_trunc (fname, 30), unix_error_string (errno));
        vfs_path_free (file);
        return NULL;
    }
    return file;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_diffviewer_get_fnames_of_diffs (ev_diffviewer_run_t * info, vfs_path_t ** file0,
                                   vfs_path_t ** file1)
{
    switch (info->run_mode)
    {
    case MC_RUN_FULL:
        /* run from panels */
        *file0 = mc_diffviewer_get_fname_from_wpanel (info->data.panel.first);
        if (*file0 == NULL)
            return FALSE;

        *file1 = mc_diffviewer_get_fname_from_wpanel (info->data.panel.second);
        if (*file1 == NULL)
            return FALSE;

        break;

    case MC_RUN_DIFFVIEWER:
        /* run from command line */
        *file0 = mc_diffviewer_get_fname_from_string (info->data.file.first);
        if (*file0 == NULL)
            return FALSE;

        *file1 = mc_diffviewer_get_fname_from_string (info->data.file.second);
        if (*file1 == NULL)
            return FALSE;

        break;

    default:
        /* this should not happaned */
        message (D_ERROR, MSG_ERROR, _("Diff viewer: invalid mode"));
        return FALSE;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_diffviewer_init (GError ** error)
{
    mc_diffviewer_init_events (error);
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

#define GET_FILE_AND_STAMP(n) \
do \
{ \
    use_copy##n = 0; \
    real_file##n = file##n; \
    if (!vfs_file_is_local (file##n)) \
    { \
        real_file##n = mc_getlocalcopy (file##n); \
        if (real_file##n != NULL) \
        { \
            use_copy##n = 1; \
            if (mc_stat (real_file##n, &st##n) != 0) \
                use_copy##n = -1; \
        } \
    } \
} \
while (0)

#define UNGET_FILE(n) \
do \
{ \
    if (use_copy##n) \
    { \
        int changed = 0; \
        if (use_copy##n > 0) \
        { \
            time_t mtime; \
            mtime = st##n.st_mtime; \
            if (mc_stat (real_file##n, &st##n) == 0) \
                changed = (mtime != st##n.st_mtime); \
        } \
        mc_ungetlocalcopy (file##n, real_file##n, changed); \
        vfs_path_free (real_file##n); \
    } \
} \
while (0)


gboolean
mc_diffviewer_cmd_run (event_info_t * event_info, gpointer data, GError ** error)
{
    ev_diffviewer_run_t *info = (ev_diffviewer_run_t *) data;
    int rv = 0;
    vfs_path_t *file0 = NULL;
    vfs_path_t *file1 = NULL;

    (void) event_info;
    (void) error;

    if (mc_diffviewer_get_fnames_of_diffs (info, &file0, &file1))
    {
        if (rv == 0)
        {
            rv = -1;
            if (file0 != NULL && file1 != NULL)
            {
                int use_copy0;
                int use_copy1;
                struct stat st0;
                struct stat st1;
                vfs_path_t *real_file0;
                vfs_path_t *real_file1;

                GET_FILE_AND_STAMP (0);
                GET_FILE_AND_STAMP (1);

                if (real_file0 != NULL && real_file1 != NULL)
                    rv = diff_view (vfs_path_as_str (real_file0), vfs_path_as_str (real_file1),
                                    vfs_path_as_str (file0), vfs_path_as_str (file1));

                UNGET_FILE (1);
                UNGET_FILE (0);
            }
        }

        if (rv == 0)
            message (D_ERROR, MSG_ERROR, _("Two files are needed to compare"));
    }
    vfs_path_free (file1);
    vfs_path_free (file0);

    info->ret_value = (rv != 0);
    return TRUE;
}

#undef GET_FILE_AND_STAMP
#undef UNGET_FILE

/* --------------------------------------------------------------------------------------------- */
