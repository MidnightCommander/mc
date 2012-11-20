/*
   Internal file viewer for the Midnight Commander
   Interface functions

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009, 2011
   The Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995, 1998
   Janne Kukonlehto, 1994, 1995
   Jakub Jelinek, 1995
   Joseph M. Hinkle, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Roland Illig <roland.illig@gmx.de>, 2004, 2005
   Slava Zanko <slavazanko@google.com>, 2009
   Andrew Borodin <aborodin@vmail.ru>, 2009
   Ilia Maslakov <il.smind@gmail.com>, 2009

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
#include <errno.h>
#include <fcntl.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/mouse.h"
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"           /* load_file_position() */
#include "lib/widget.h"

#include "src/filemanager/layout.h"     /* menubar_visible */
#include "src/filemanager/midnight.h"   /* the_menubar */

#include "internal.h"
#include "mcviewer.h"

/*** global variables ****************************************************************************/

int mcview_default_hex_mode = 0;
int mcview_default_nroff_flag = 0;
int mcview_global_wrap_mode = 1;
int mcview_default_magic_flag = 1;

int mcview_altered_hex_mode = 0;
int mcview_altered_magic_flag = 0;
int mcview_altered_nroff_flag = 0;

int mcview_remember_file_position = FALSE;

/* Maxlimit for skipping updates */
int mcview_max_dirt_limit = 10;

/* Scrolling is done in pages or line increments */
int mcview_mouse_move_pages = 1;

/* end of file will be showen from mcview_show_eof */
char *mcview_show_eof = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/


/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Both views */
static gboolean
do_mcview_event (mcview_t * view, Gpm_Event * event, int *result)
{
    screen_dimen y, x;
    Gpm_Event local;
    Widget *w = WIDGET (view);

    /* rest of the upper frame - call menu */
    if (mcview_is_in_panel (view) && (event->type & GPM_DOWN) != 0 &&
        event->y == WIDGET (w->owner)->y + 1)
    {
        *result = MOU_UNHANDLED;
        return FALSE;           /* don't draw viewer over menu */
    }

    *result = MOU_NORMAL;

    local = mouse_get_local (event, w);

    /* We are not interested in the release events */
    if ((local.type & (GPM_DOWN | GPM_DRAG)) == 0)
        return FALSE;

    /* Wheel events */
    if ((local.buttons & GPM_B_UP) != 0 && (local.type & GPM_DOWN) != 0)
    {
        mcview_move_up (view, 2);
        return TRUE;
    }
    if ((local.buttons & GPM_B_DOWN) != 0 && (local.type & GPM_DOWN) != 0)
    {
        mcview_move_down (view, 2);
        return TRUE;
    }

    x = local.x;
    y = local.y;

    /* Scrolling left and right */
    if (!view->text_wrap_mode)
    {
        if (x < view->data_area.width * 1 / 4)
        {
            mcview_move_left (view, 1);
            goto processed;
        }

        if (x < view->data_area.width * 3 / 4)
        {
            /* ignore the click */
        }
        else
        {
            mcview_move_right (view, 1);
            goto processed;
        }
    }

    /* Scrolling up and down */
    if (y < view->data_area.top + view->data_area.height * 1 / 3)
    {
        if (mcview_mouse_move_pages)
            mcview_move_up (view, view->data_area.height / 2);
        else
            mcview_move_up (view, 1);
        goto processed;
    }
    else if (y < view->data_area.top + view->data_area.height * 2 / 3)
    {
        /* ignore the click */
    }
    else
    {
        if (mcview_mouse_move_pages)
            mcview_move_down (view, view->data_area.height / 2);
        else
            mcview_move_down (view, 1);
        goto processed;
    }

    return FALSE;

  processed:
    *result = MOU_REPEAT;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/** Real view only */
static int
mcview_event (Gpm_Event * event, void *data)
{
    mcview_t *view = (mcview_t *) data;
    int result;

    if (!mouse_global_in_widget (event, WIDGET (data)))
        return MOU_UNHANDLED;

    if (do_mcview_event (view, event, &result))
        mcview_update (view);
    return result;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

mcview_t *
mcview_new (int y, int x, int lines, int cols, gboolean is_panel)
{
    mcview_t *view;

    view = g_new0 (mcview_t, 1);
    init_widget (WIDGET (view), y, x, lines, cols, mcview_callback, mcview_event);

    view->hex_mode = FALSE;
    view->hexedit_mode = FALSE;
    view->locked = FALSE;
    view->hexview_in_text = FALSE;
    view->text_nroff_mode = FALSE;
    view->text_wrap_mode = FALSE;
    view->magic_mode = FALSE;

    view->dpy_frame_size = is_panel ? 1 : 0;
    view->converter = str_cnv_from_term;

    mcview_init (view);

    if (mcview_default_hex_mode)
        mcview_toggle_hex_mode (view);
    if (mcview_default_nroff_flag)
        mcview_toggle_nroff_mode (view);
    if (mcview_global_wrap_mode)
        mcview_toggle_wrap_mode (view);
    if (mcview_default_magic_flag)
        mcview_toggle_magic_mode (view);

    return view;
}

/* --------------------------------------------------------------------------------------------- */
/** Real view only */

gboolean
mcview_viewer (const char *command, const vfs_path_t * file_vpath, int start_line)
{
    gboolean succeeded;
    mcview_t *lc_mcview;
    WDialog *view_dlg;

    /* Create dialog and widgets, put them on the dialog */
    view_dlg = create_dlg (FALSE, 0, 0, LINES, COLS, NULL, mcview_dialog_callback, NULL,
                           "[Internal File Viewer]", NULL, DLG_WANT_TAB);

    lc_mcview = mcview_new (0, 0, LINES - 1, COLS, FALSE);
    add_widget (view_dlg, lc_mcview);

    add_widget (view_dlg, buttonbar_new (TRUE));

    view_dlg->get_title = mcview_get_title;

    {
        char *file;

        file = vfs_path_to_str (file_vpath);
        succeeded = mcview_load (lc_mcview, command, file, start_line);
        g_free (file);
    }

    if (succeeded)
        run_dlg (view_dlg);
    else
        view_dlg->state = DLG_CLOSED;

    if (view_dlg->state == DLG_CLOSED)
        destroy_dlg (view_dlg);

    return succeeded;
}

/* {{{ Miscellaneous functions }}} */

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_load (mcview_t * view, const char *command, const char *file, int start_line)
{
    gboolean retval = FALSE;
    vfs_path_t *vpath = NULL;

#ifdef HAVE_ASSERT_H
    assert (view->bytes_per_line != 0);
#endif

    view->filename_vpath = vfs_path_from_str (file);

    /* get working dir */
    if (file != NULL && file[0] != '\0')
    {
        vfs_path_free (view->workdir_vpath);

        if (!g_path_is_absolute (file))
        {
            vfs_path_t *p;

            p = vfs_path_clone (vfs_get_raw_current_dir ());
            view->workdir_vpath = vfs_path_append_new (p, file, (char *) NULL);
            vfs_path_free (p);
        }
        else
        {
            /* try extract path form filename */
            const char *fname;
            char *dir;

            fname = x_basename (file);
            dir = g_strndup (file, (size_t) (fname - file));
            view->workdir_vpath = vfs_path_from_str (dir);
            g_free (dir);
        }
    }

    if (!mcview_is_in_panel (view))
        view->dpy_text_column = 0;

    mcview_set_codeset (view);

    if (command != NULL && (view->magic_mode || file == NULL || file[0] == '\0'))
        retval = mcview_load_command_output (view, command);
    else if (file != NULL && file[0] != '\0')
    {
        int fd = -1;
        char tmp[BUF_MEDIUM];
        struct stat st;

        /* Open the file */
        vpath = vfs_path_from_str (file);
        fd = mc_open (vpath, O_RDONLY | O_NONBLOCK);
        if (fd == -1)
        {
            g_snprintf (tmp, sizeof (tmp), _("Cannot open \"%s\"\n%s"),
                        file, unix_error_string (errno));
            mcview_show_error (view, tmp);
            vfs_path_free (view->filename_vpath);
            view->filename_vpath = NULL;
            vfs_path_free (view->workdir_vpath);
            view->workdir_vpath = NULL;
            goto finish;
        }

        /* Make sure we are working with a regular file */
        if (mc_fstat (fd, &st) == -1)
        {
            mc_close (fd);
            g_snprintf (tmp, sizeof (tmp), _("Cannot stat \"%s\"\n%s"),
                        file, unix_error_string (errno));
            mcview_show_error (view, tmp);
            vfs_path_free (view->filename_vpath);
            view->filename_vpath = NULL;
            vfs_path_free (view->workdir_vpath);
            view->workdir_vpath = NULL;
            goto finish;
        }

        if (!S_ISREG (st.st_mode))
        {
            mc_close (fd);
            mcview_show_error (view, _("Cannot view: not a regular file"));
            vfs_path_free (view->filename_vpath);
            view->filename_vpath = NULL;
            vfs_path_free (view->workdir_vpath);
            view->workdir_vpath = NULL;
            goto finish;
        }

        if (st.st_size == 0 || mc_lseek (fd, 0, SEEK_SET) == -1)
        {
            /* Must be one of those nice files that grow (/proc) */
            mcview_set_datasource_vfs_pipe (view, fd);
        }
        else
        {
            int type;

            type = get_compression_type (fd, file);

            if (view->magic_mode && (type != COMPRESSION_NONE))
            {
                char *tmp_filename;

                vfs_path_free (view->filename_vpath);
                tmp_filename = g_strconcat (file, decompress_extension (type), (char *) NULL);
                view->filename_vpath = vfs_path_from_str (tmp_filename);
                g_free (tmp_filename);
            }
            mcview_set_datasource_file (view, fd, &st);
        }
        retval = TRUE;
    }

  finish:
    view->command = g_strdup (command);
    view->dpy_start = 0;
    view->search_start = 0;
    view->search_end = 0;
    view->dpy_text_column = 0;

    mcview_compute_areas (view);
    mcview_update_bytes_per_line (view);

    if (mcview_remember_file_position && view->filename_vpath != NULL && start_line == 0)
    {
        long line, col;
        off_t new_offset, max_offset;

        load_file_position (view->filename_vpath, &line, &col, &new_offset, &view->saved_bookmarks);
        max_offset = mcview_get_filesize (view) - 1;
        if (max_offset < 0)
            new_offset = 0;
        else
            new_offset = min (new_offset, max_offset);
        if (!view->hex_mode)
            view->dpy_start = mcview_bol (view, new_offset, 0);
        else
        {
            view->dpy_start = new_offset - new_offset % view->bytes_per_line;
            view->hex_cursor = new_offset;
        }
    }
    else if (start_line > 0)
        mcview_moveto (view, start_line - 1, 0);

    view->hexedit_lownibble = FALSE;
    view->hexview_in_text = FALSE;
    view->change_list = NULL;
    vfs_path_free (vpath);
    return retval;
}

/* --------------------------------------------------------------------------------------------- */
