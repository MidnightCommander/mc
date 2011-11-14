/*
   Editor high level editing commands

   Copyright (C) 1996, 1997, 1998, 2001, 2002, 2003, 2004, 2005, 2006,
   2007, 2011
   The Free Software Foundation, Inc.

   Written by:
   Paul Sheer, 1996, 1997

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

/** \file
 *  \brief Source: editor high level editing commands
 *  \author Paul Sheer
 *  \date 1996, 1997
 */

/* #define PIPE_BLOCKS_SO_READ_BYTE_BY_BYTE */

#include <config.h>

#include <assert.h>
#include <ctype.h>

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* XCTRL */
#include "lib/mcconfig.h"
#include "lib/skin.h"
#include "lib/strutil.h"        /* utf string functions */
#include "lib/lock.h"
#include "lib/util.h"           /* tilde_expand() */
#include "lib/vfs/vfs.h"
#include "lib/widget.h"
#include "lib/charsets.h"
#include "lib/event.h"          /* mc_event_raise() */

#include "src/history.h"
#include "src/setup.h"          /* option_tab_spacing */
#include "src/selcodepage.h"
#include "src/keybind-defaults.h"
#include "src/util.h"           /* check_for_default() */
#include "src/filemanager/layout.h"     /* mc_refresh()  */


#include "edit-impl.h"
#include "editwidget.h"
#include "editcmd_dialogs.h"
#include "etags.h"

/*** global variables ****************************************************************************/

/* search and replace: */
int search_create_bookmark = FALSE;

/* queries on a save */
int edit_confirm_save = 1;

/*** file scope macro definitions ****************************************************************/

#define space_width 1

#define TEMP_BUF_LEN 1024

#define INPUT_INDEX 9

/* thanks to  Liviu Daia <daia@stoilow.imar.ro>  for getting this
   (and the above) routines to work properly - paul */

#define is_digit(x) ((x) >= '0' && (x) <= '9')

#define MAIL_DLG_HEIGHT 12

#define MAX_WORD_COMPLETIONS 100        /* in listbox */

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/*  If 0 (quick save) then  a) create/truncate <filename> file,
   b) save to <filename>;
   if 1 (safe save) then   a) save to <tempnam>,
   b) rename <tempnam> to <filename>;
   if 2 (do backups) then  a) save to <tempnam>,
   b) rename <filename> to <filename.backup_ext>,
   c) rename <tempnam> to <filename>. */

/* returns 0 on error, -1 on abort */

static int
edit_save_file (WEdit * edit, const char *filename)
{
    char *p;
    gchar *tmp;
    long filelen = 0;
    char *savename = 0;
    gchar *real_filename;
    int this_save_mode, fd = -1;

    if (!filename)
        return 0;
    if (!*filename)
        return 0;

    if (*filename != PATH_SEP && edit->dir)
    {
        real_filename = concat_dir_and_file (edit->dir, filename);
    }
    else
    {
        real_filename = g_strdup (filename);
    }

    this_save_mode = option_save_mode;
    if (this_save_mode != EDIT_QUICK_SAVE)
    {
        vfs_path_t *vpath = vfs_path_from_str (real_filename);
        if (!vfs_file_is_local (vpath) || (fd = mc_open (real_filename, O_RDONLY | O_BINARY)) == -1)
        {
            /*
             * The file does not exists yet, so no safe save or
             * backup are necessary.
             */
            this_save_mode = EDIT_QUICK_SAVE;
        }
        vfs_path_free (vpath);
        if (fd != -1)
            mc_close (fd);
    }

    if (this_save_mode == EDIT_QUICK_SAVE && !edit->skip_detach_prompt)
    {
        int rv;
        struct stat sb;

        rv = mc_stat (real_filename, &sb);
        if (rv == 0 && sb.st_nlink > 1)
        {
            rv = edit_query_dialog3 (_("Warning"),
                                     _("File has hard-links. Detach before saving?"),
                                     _("&Yes"), _("&No"), _("&Cancel"));
            switch (rv)
            {
            case 0:
                this_save_mode = EDIT_SAFE_SAVE;
                /* fallthrough */
            case 1:
                edit->skip_detach_prompt = 1;
                break;
            default:
                g_free (real_filename);
                return -1;
            }
        }

        /* Prevent overwriting changes from other editor sessions. */
        if (rv == 0 && edit->stat1.st_mtime != 0 && edit->stat1.st_mtime != sb.st_mtime)
        {

            /* The default action is "Cancel". */
            query_set_sel (1);

            rv = edit_query_dialog2 (_("Warning"),
                                     _("The file has been modified in the meantime. Save anyway?"),
                                     _("&Yes"), _("&Cancel"));
            if (rv != 0)
            {
                g_free (real_filename);
                return -1;
            }
        }
    }

    if (this_save_mode != EDIT_QUICK_SAVE)
    {
        char *savedir, *saveprefix;
        const char *slashpos;
        slashpos = strrchr (real_filename, PATH_SEP);
        if (slashpos)
        {
            savedir = g_strdup (real_filename);
            savedir[slashpos - real_filename + 1] = '\0';
        }
        else
            savedir = g_strdup (".");
        saveprefix = concat_dir_and_file (savedir, "cooledit");
        g_free (savedir);
        fd = mc_mkstemps (&savename, saveprefix, NULL);
        g_free (saveprefix);
        if (!savename)
        {
            g_free (real_filename);
            return 0;
        }
        /* FIXME:
         * Close for now because mc_mkstemps use pure open system call
         * to create temporary file and it needs to be reopened by
         * VFS-aware mc_open().
         */
        close (fd);
    }
    else
        savename = g_strdup (real_filename);
    {
        int ret;
        ret = mc_chown (savename, edit->stat1.st_uid, edit->stat1.st_gid);
        ret = mc_chmod (savename, edit->stat1.st_mode);
    }

    fd = mc_open (savename, O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, edit->stat1.st_mode);
    if (fd == -1)
        goto error_save;

    /* pipe save */
    p = edit_get_write_filter (savename, real_filename);
    if (p != NULL)
    {
        FILE *file;

        mc_close (fd);
        file = (FILE *) popen (p, "w");

        if (file)
        {
            filelen = edit_write_stream (edit, file);
#if 1
            pclose (file);
#else
            if (pclose (file) != 0)
            {
                tmp = g_strdup_printf (_("Error writing to pipe: %s"), p);
                edit_error_dialog (_("Error"), tmp);
                g_free (tmp);
                g_free (p);
                goto error_save;
            }
#endif
        }
        else
        {
            tmp = g_strdup_printf (_("Cannot open pipe for writing: %s"), p);
            edit_error_dialog (_("Error"), get_sys_error (tmp));
            g_free (p);
            g_free (tmp);
            goto error_save;
        }
        g_free (p);
    }
    else if (edit->lb == LB_ASIS)
    {                           /* do not change line breaks */
        long buf;
        buf = 0;
        filelen = edit->last_byte;
        while (buf <= (edit->curs1 >> S_EDIT_BUF_SIZE) - 1)
        {
            if (mc_write (fd, (char *) edit->buffers1[buf], EDIT_BUF_SIZE) != EDIT_BUF_SIZE)
            {
                mc_close (fd);
                goto error_save;
            }
            buf++;
        }
        if (mc_write
            (fd, (char *) edit->buffers1[buf],
             edit->curs1 & M_EDIT_BUF_SIZE) != (edit->curs1 & M_EDIT_BUF_SIZE))
        {
            filelen = -1;
        }
        else if (edit->curs2)
        {
            edit->curs2--;
            buf = (edit->curs2 >> S_EDIT_BUF_SIZE);
            if (mc_write
                (fd,
                 (char *) edit->buffers2[buf] + EDIT_BUF_SIZE -
                 (edit->curs2 & M_EDIT_BUF_SIZE) - 1,
                 1 + (edit->curs2 & M_EDIT_BUF_SIZE)) != 1 + (edit->curs2 & M_EDIT_BUF_SIZE))
            {
                filelen = -1;
            }
            else
            {
                while (--buf >= 0)
                {
                    if (mc_write (fd, (char *) edit->buffers2[buf], EDIT_BUF_SIZE) != EDIT_BUF_SIZE)
                    {
                        filelen = -1;
                        break;
                    }
                }
            }
            edit->curs2++;
        }
        if (mc_close (fd))
            goto error_save;

        /* Update the file information, especially the mtime. */
        if (mc_stat (savename, &edit->stat1) == -1)
            goto error_save;
    }
    else
    {                           /* change line breaks */
        FILE *file;

        mc_close (fd);

        file = (FILE *) fopen (savename, "w");

        if (file)
        {
            filelen = edit_write_stream (edit, file);
            fclose (file);
        }
        else
        {
            char *msg;

            msg = g_strdup_printf (_("Cannot open file for writing: %s"), savename);
            edit_error_dialog (_("Error"), msg);
            g_free (msg);
            goto error_save;
        }
    }

    if (filelen != edit->last_byte)
        goto error_save;

    if (this_save_mode == EDIT_DO_BACKUP)
    {
        assert (option_backup_ext != NULL);
        tmp = g_strconcat (real_filename, option_backup_ext, (char *) NULL);
        if (mc_rename (real_filename, tmp) == -1)
        {
            g_free (tmp);
            goto error_save;
        }
    }

    if (this_save_mode != EDIT_QUICK_SAVE)
        if (mc_rename (savename, real_filename) == -1)
            goto error_save;
    g_free (savename);
    g_free (real_filename);
    return 1;
  error_save:
    /*  FIXME: Is this safe ?
     *  if (this_save_mode != EDIT_QUICK_SAVE)
     *      mc_unlink (savename);
     */
    g_free (real_filename);
    g_free (savename);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
edit_check_newline (WEdit * edit)
{
    return !(option_check_nl_at_eof && edit->last_byte > 0
             && edit_get_byte (edit, edit->last_byte - 1) != '\n'
             && edit_query_dialog2 (_("Warning"),
                                    _("The file you are saving is not finished with a newline"),
                                    _("C&ontinue"), _("&Cancel")));
}

/* --------------------------------------------------------------------------------------------- */

static char *
edit_get_save_file_as (WEdit * edit)
{
#define DLG_WIDTH 64
#define DLG_HEIGHT 14

    static LineBreaks cur_lb = LB_ASIS;

    char *filename = edit->filename;

    const char *lb_names[LB_NAMES] = {
        N_("&Do not change"),
        N_("&Unix format (LF)"),
        N_("&Windows/DOS format (CR LF)"),
        N_("&Macintosh format (CR)")
    };

    QuickWidget quick_widgets[] = {
        QUICK_BUTTON (6, 10, DLG_HEIGHT - 3, DLG_HEIGHT, N_("&Cancel"), B_CANCEL, NULL),
        QUICK_BUTTON (2, 10, DLG_HEIGHT - 3, DLG_HEIGHT, N_("&OK"), B_ENTER, NULL),
        QUICK_RADIO (5, DLG_WIDTH, DLG_HEIGHT - 8, DLG_HEIGHT, LB_NAMES, lb_names, (int *) &cur_lb),
        QUICK_LABEL (3, DLG_WIDTH, DLG_HEIGHT - 9, DLG_HEIGHT, N_("Change line breaks to:")),
        QUICK_INPUT (3, DLG_WIDTH, DLG_HEIGHT - 11, DLG_HEIGHT, filename, DLG_WIDTH - 6, 0,
                     "save-as", &filename),
        QUICK_LABEL (3, DLG_WIDTH, DLG_HEIGHT - 12, DLG_HEIGHT, N_("Enter file name:")),
        QUICK_END
    };

    QuickDialog Quick_options = {
        DLG_WIDTH, DLG_HEIGHT, -1, -1,
        N_("Save As"), "[Save File As]",
        quick_widgets, NULL, FALSE
    };

    if (quick_dialog (&Quick_options) != B_CANCEL)
    {
        char *fname;

        edit->lb = cur_lb;
        fname = tilde_expand (filename);
        g_free (filename);
        return fname;
    }

    return NULL;

#undef DLG_WIDTH
#undef DLG_HEIGHT
}

/** returns 1 on success */

static int
edit_save_cmd (WEdit * edit)
{
    int res, save_lock = 0;

    if (!edit->locked && !edit->delete_file)
        save_lock = edit_lock_file (edit);
    res = edit_save_file (edit, edit->filename);

    /* Maintain modify (not save) lock on failure */
    if ((res > 0 && edit->locked) || save_lock)
        edit->locked = edit_unlock_file (edit);

    /* On failure try 'save as', it does locking on its own */
    if (!res)
        return edit_save_as_cmd (edit);
    edit->force |= REDRAW_COMPLETELY;
    if (res > 0)
    {
        edit->delete_file = 0;
        edit->modified = 0;
    }

    return 1;
}

/* --------------------------------------------------------------------------------------------- */

/** returns FALSE on error */
static gboolean
edit_load_file_from_filename (WEdit * edit, char *exp)
{
    int prev_locked = edit->locked;
    char *prev_filename = g_strdup (edit->filename);

    if (!edit_reload (edit, exp))
    {
        g_free (prev_filename);
        return FALSE;
    }

    if (prev_locked != 0)
    {
        char *fullpath;

        fullpath = mc_build_filename (edit->dir, prev_filename, (char *) NULL);
        unlock_file (fullpath);
        g_free (fullpath);
    }
    g_free (prev_filename);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
edit_load_syntax_file (WEdit * edit)
{
    char *extdir;
    int dir = 0;

    if (geteuid () == 0)
    {
        dir = query_dialog (_("Syntax file edit"),
                            _("Which syntax file you want to edit?"), D_NORMAL, 2,
                            _("&User"), _("&System Wide"));
    }

    extdir = g_build_filename (mc_global.sysconfig_dir, "syntax", "Syntax", (char *) NULL);
    if (!exist_file (extdir))
    {
        g_free (extdir);
        extdir = g_build_filename (mc_global.share_data_dir, "syntax", "Syntax", (char *) NULL);
    }

    if (dir == 0)
    {
        char *buffer;

        buffer = concat_dir_and_file (mc_config_get_data_path (), EDIT_SYNTAX_FILE);
        check_for_default (extdir, buffer);
        edit_load_file_from_filename (edit, buffer);
        g_free (buffer);
    }
    else if (dir == 1)
        edit_load_file_from_filename (edit, extdir);

    g_free (extdir);
}

/* --------------------------------------------------------------------------------------------- */

static void
edit_load_menu_file (WEdit * edit)
{
    char *buffer;
    char *menufile;
    int dir = 0;

    dir = query_dialog (_("Menu edit"),
                        _("Which menu file do you want to edit?"), D_NORMAL,
                        geteuid () != 0 ? 2 : 3, _("&Local"), _("&User"), _("&System Wide"));

    menufile = concat_dir_and_file (mc_global.sysconfig_dir, EDIT_GLOBAL_MENU);

    if (!exist_file (menufile))
    {
        g_free (menufile);
        menufile = concat_dir_and_file (mc_global.share_data_dir, EDIT_GLOBAL_MENU);
    }

    switch (dir)
    {
    case 0:
        buffer = g_strdup (EDIT_LOCAL_MENU);
        check_for_default (menufile, buffer);
        chmod (buffer, 0600);
        break;

    case 1:
        buffer = concat_dir_and_file (mc_config_get_data_path (), EDIT_HOME_MENU);
        check_for_default (menufile, buffer);
        break;

    case 2:
        buffer = concat_dir_and_file (mc_global.sysconfig_dir, EDIT_GLOBAL_MENU);
        if (!exist_file (buffer))
        {
            g_free (buffer);
            buffer = concat_dir_and_file (mc_global.share_data_dir, EDIT_GLOBAL_MENU);
        }
        break;

    default:
        g_free (menufile);
        return;
    }

    edit_load_file_from_filename (edit, buffer);

    g_free (buffer);
    g_free (menufile);
}

/* --------------------------------------------------------------------------------------------- */

static void
edit_delete_column_of_text (WEdit * edit)
{
    long p, q, r, m1, m2;
    long b, c, d, n;

    eval_marks (edit, &m1, &m2);
    n = edit_move_forward (edit, m1, 0, m2) + 1;
    c = edit_move_forward3 (edit, edit_bol (edit, m1), 0, m1);
    d = edit_move_forward3 (edit, edit_bol (edit, m2), 0, m2);
    b = max (min (c, d), min (edit->column1, edit->column2));
    c = max (c, max (edit->column1, edit->column2));

    while (n--)
    {
        r = edit_bol (edit, edit->curs1);
        p = edit_move_forward3 (edit, r, b, 0);
        q = edit_move_forward3 (edit, r, c, 0);
        if (p < m1)
            p = m1;
        if (q > m2)
            q = m2;
        edit_cursor_move (edit, p - edit->curs1);
        while (q > p)
        {
            /* delete line between margins */
            if (edit_get_byte (edit, edit->curs1) != '\n')
                edit_delete (edit, 1);
            q--;
        }
        if (n)
            /* move to next line except on the last delete */
            edit_cursor_move (edit, edit_move_forward (edit, edit->curs1, 1, 0) - edit->curs1);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** if success return 0 */

static int
edit_block_delete (WEdit * edit)
{
    long count;
    long start_mark, end_mark;
    int curs_pos, line_width;
    long curs_line, c1, c2;

    if (eval_marks (edit, &start_mark, &end_mark))
        return 0;
    if (edit->column_highlight && edit->mark2 < 0)
        edit_mark_cmd (edit, 0);
    if ((end_mark - start_mark) > option_max_undo / 2)
    {
        /* Warning message with a query to continue or cancel the operation */
        if (edit_query_dialog2
            (_("Warning"),
             _
             ("Block is large, you may not be able to undo this action"),
             _("C&ontinue"), _("&Cancel")))
        {
            return 1;
        }
    }
    c1 = min (edit->column1, edit->column2);
    c2 = max (edit->column1, edit->column2);
    edit->column1 = c1;
    edit->column2 = c2;

    edit_push_markers (edit);

    curs_line = edit->curs_line;

    /* calculate line width and cursor position before cut */
    line_width = edit_move_forward3 (edit, edit_bol (edit, edit->curs1), 0,
                                     edit_eol (edit, edit->curs1));
    curs_pos = edit->curs_col + edit->over_col;

    /* move cursor to start of selection */
    edit_cursor_move (edit, start_mark - edit->curs1);
    edit_scroll_screen_over_cursor (edit);
    count = start_mark;
    if (start_mark < end_mark)
    {
        if (edit->column_highlight)
        {
            if (edit->mark2 < 0)
                edit_mark_cmd (edit, 0);
            edit_delete_column_of_text (edit);
            /* move cursor to the saved position */
            edit_move_to_line (edit, curs_line);
            /* calculate line width after cut */
            line_width = edit_move_forward3 (edit, edit_bol (edit, edit->curs1), 0,
                                             edit_eol (edit, edit->curs1));
            if (option_cursor_beyond_eol && curs_pos > line_width)
                edit->over_col = curs_pos - line_width;
        }
        else
        {
            while (count < end_mark)
            {
                edit_delete (edit, 1);
                count++;
            }
        }
    }
    edit_set_markers (edit, 0, 0, 0, 0);
    edit->force |= REDRAW_PAGE;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
editcmd_find (WEdit * edit, gsize * len)
{
    off_t search_start = edit->search_start;
    off_t search_end;
    long start_mark = 0;
    long end_mark = edit->last_byte;
    int mark_res = 0;

    if (edit_search_options.only_in_selection)
    {
        mark_res = eval_marks (edit, &start_mark, &end_mark);
        if (mark_res != 0)
        {
            edit->search->error = MC_SEARCH_E_NOTFOUND;
            edit->search->error_str = g_strdup (_("Search string not found"));
            return FALSE;
        }
        if (edit_search_options.backwards)
        {
            if (search_start > end_mark || search_start <= start_mark)
            {
                search_start = end_mark;
            }
        }
        else
        {
            if (search_start < start_mark || search_start >= end_mark)
            {
                search_start = start_mark;
            }
        }
    }
    else
    {
        if (edit_search_options.backwards)
            end_mark = max (1, edit->curs1) - 1;
    }
    if (edit_search_options.backwards)
    {
        search_end = end_mark;
        while ((int) search_start >= start_mark)
        {
            if (search_end > (off_t) (search_start + edit->search->original_len) &&
                mc_search_is_fixed_search_str (edit->search))
            {
                search_end = search_start + edit->search->original_len;
            }
            if (mc_search_run (edit->search, (void *) edit, search_start, search_end, len)
                && edit->search->normal_offset == search_start)
            {
                return TRUE;
            }
            search_start--;
        }
        edit->search->error_str = g_strdup (_("Search string not found"));
    }
    else
    {
        return mc_search_run (edit->search, (void *) edit, search_start, end_mark, len);
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static char *
edit_replace_cmd__conv_to_display (char *str)
{
#ifdef HAVE_CHARSET
    GString *tmp;

    tmp = str_convert_to_display (str);
    if (tmp != NULL)
    {
        if (tmp->len != 0)
            return g_string_free (tmp, FALSE);
        g_string_free (tmp, TRUE);
    }
#endif
    return g_strdup (str);
}

/* --------------------------------------------------------------------------------------------- */

static char *
edit_replace_cmd__conv_to_input (char *str)
{
#ifdef HAVE_CHARSET
    GString *tmp;

    tmp = str_convert_to_input (str);
    if (tmp != NULL)
    {
        if (tmp->len != 0)
            return g_string_free (tmp, FALSE);
        g_string_free (tmp, TRUE);
    }
#endif
    return g_strdup (str);
}

/* --------------------------------------------------------------------------------------------- */

static void
edit_do_search (WEdit * edit)
{
    gsize len = 0;

    if (edit->search == NULL)
        edit->search_start = edit->curs1;

    edit_push_undo_action (edit, KEY_PRESS + edit->start_display);

    if (search_create_bookmark)
    {
        int found = 0, books = 0;
        long l = 0, l_last = -1;
        long q = 0;

        search_create_bookmark = FALSE;
        book_mark_flush (edit, -1);

        while (TRUE)
        {
            if (!mc_search_run (edit->search, (void *) edit, q, edit->last_byte, &len))
                break;
            if (found == 0)
                edit->search_start = edit->search->normal_offset;
            found++;
            l += edit_count_lines (edit, q, edit->search->normal_offset);
            if (l != l_last)
            {
                book_mark_insert (edit, l, BOOK_MARK_FOUND_COLOR);
                books++;
            }
            l_last = l;
            q = edit->search->normal_offset + 1;
        }

        if (found == 0)
            edit_error_dialog (_("Search"), _("Search string not found"));
        else
            edit_cursor_move (edit, edit->search_start - edit->curs1);
    }
    else
    {
        if (edit->found_len != 0 && edit->search_start == edit->found_start + 1
            && edit_search_options.backwards)
            edit->search_start--;

        if (edit->found_len != 0 && edit->search_start == edit->found_start - 1
            && !edit_search_options.backwards)
            edit->search_start++;

        if (editcmd_find (edit, &len))
        {
            edit->found_start = edit->search_start = edit->search->normal_offset;
            edit->found_len = len;
            edit->over_col = 0;
            edit_cursor_move (edit, edit->search_start - edit->curs1);
            edit_scroll_screen_over_cursor (edit);
            if (edit_search_options.backwards)
                edit->search_start--;
            else
                edit->search_start++;
        }
        else
        {
            edit->search_start = edit->curs1;
            if (edit->search->error_str != NULL)
                edit_error_dialog (_("Search"), edit->search->error_str);
        }
    }

    edit->force |= REDRAW_COMPLETELY;
    edit_scroll_screen_over_cursor (edit);
}

/* --------------------------------------------------------------------------------------------- */

static void
edit_search (WEdit * edit)
{
    if (editcmd_dialog_search_show (edit))
        edit_do_search (edit);
}

/* --------------------------------------------------------------------------------------------- */
/** Return a null terminated length of text. Result must be g_free'd */

static unsigned char *
edit_get_block (WEdit * edit, long start, long finish, int *l)
{
    unsigned char *s, *r;
    r = s = g_malloc0 (finish - start + 1);
    if (edit->column_highlight)
    {
        *l = 0;
        /* copy from buffer, excluding chars that are out of the column 'margins' */
        while (start < finish)
        {
            int c;
            long x;
            x = edit_move_forward3 (edit, edit_bol (edit, start), 0, start);
            c = edit_get_byte (edit, start);
            if ((x >= edit->column1 && x < edit->column2)
                || (x >= edit->column2 && x < edit->column1) || c == '\n')
            {
                *s++ = c;
                (*l)++;
            }
            start++;
        }
    }
    else
    {
        *l = finish - start;
        while (start < finish)
            *s++ = edit_get_byte (edit, start++);
    }
    *s = 0;
    return r;
}

/* --------------------------------------------------------------------------------------------- */
/** copies a block to clipboard file */

static int
edit_save_block_to_clip_file (WEdit * edit, long start, long finish)
{
    int ret;
    gchar *tmp;
    tmp = concat_dir_and_file (mc_config_get_cache_path (), EDIT_CLIP_FILE);
    ret = edit_save_block (edit, tmp, start, finish);
    g_free (tmp);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static void
pipe_mail (WEdit * edit, char *to, char *subject, char *cc)
{
    FILE *p = 0;
    char *s;

    to = name_quote (to, 0);
    subject = name_quote (subject, 0);
    cc = name_quote (cc, 0);
    s = g_strconcat ("mail -s ", subject, *cc ? " -c " : "", cc, " ", to, (char *) NULL);
    g_free (to);
    g_free (subject);
    g_free (cc);

    if (s)
    {
        p = popen (s, "w");
        g_free (s);
    }

    if (p)
    {
        long i;
        for (i = 0; i < edit->last_byte; i++)
            fputc (edit_get_byte (edit, i), p);
        pclose (p);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
is_break_char (char c)
{
    return (isspace (c) || strchr ("{}[]()<>=|/\\!?~-+`'\",.;:#$%^&*", c));
}

/* --------------------------------------------------------------------------------------------- */
/** find first character of current word */

static int
edit_find_word_start (WEdit * edit, long *word_start, gsize * word_len)
{
    int c, last;
    gsize i;

    /* return if at begin of file */
    if (edit->curs1 <= 0)
        return 0;

    c = (unsigned char) edit_get_byte (edit, edit->curs1 - 1);
    /* return if not at end or in word */
    if (is_break_char (c))
        return 0;

    /* search start of word to be completed */
    for (i = 2;; i++)
    {
        /* return if at begin of file */
        if ((gsize) edit->curs1 < i)
            return 0;

        last = c;
        c = (unsigned char) edit_get_byte (edit, edit->curs1 - i);

        if (is_break_char (c))
        {
            /* return if word starts with digit */
            if (isdigit (last))
                return 0;

            *word_start = edit->curs1 - (i - 1);        /* start found */
            *word_len = i - 1;
            break;
        }
    }
    /* success */
    return 1;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get current word under cursor
 *
 * @param edit editor object
 * @param srch mc_search object
 * @param word_start start word position
 *
 * @return newly allocated string or NULL if no any words under cursor
 */

static char *
edit_collect_completions_get_current_word (WEdit * edit, mc_search_t * srch, long word_start)
{
    gsize len = 0, i;
    GString *temp;

    if (!mc_search_run (srch, (void *) edit, word_start, edit->last_byte, &len))
        return NULL;

    temp = g_string_sized_new (len);

    for (i = 0; i < len; i++)
    {
        int chr;

        chr = edit_get_byte (edit, word_start + i);
        if (!isspace (chr))
            g_string_append_c (temp, chr);
    }

    return g_string_free (temp, temp->len == 0);
}

/* --------------------------------------------------------------------------------------------- */
/** collect the possible completions */

static gsize
edit_collect_completions (WEdit * edit, long word_start, gsize word_len,
                          char *match_expr, struct selection *compl, gsize * num)
{
    gsize len = 0;
    gsize max_len = 0;
    gsize i;
    int skip;
    GString *temp;
    mc_search_t *srch;
    long last_byte, start = -1;
    char *current_word;

    srch = mc_search_new (match_expr, -1);
    if (srch == NULL)
        return 0;

    if (mc_config_get_bool
        (mc_main_config, CONFIG_APP_SECTION, "editor_wordcompletion_collect_entire_file", 0))
    {
        last_byte = edit->last_byte;
    }
    else
    {
        last_byte = word_start;
    }

    srch->search_type = MC_SEARCH_T_REGEX;
    srch->is_case_sensitive = TRUE;
    srch->search_fn = edit_search_cmd_callback;

    current_word = edit_collect_completions_get_current_word (edit, srch, word_start);

    temp = g_string_new ("");

    /* collect max MAX_WORD_COMPLETIONS completions */
    while (mc_search_run (srch, (void *) edit, start + 1, last_byte, &len))
    {
        g_string_set_size (temp, 0);
        start = srch->normal_offset;

        /* add matched completion if not yet added */
        for (i = 0; i < len; i++)
        {
            skip = edit_get_byte (edit, start + i);
            if (isspace (skip))
                continue;

            /* skip current word */
            if (start + (long) i == word_start)
                break;

            g_string_append_c (temp, skip);
        }

        if (temp->len == 0)
            continue;

        if (current_word != NULL && strcmp (current_word, temp->str) == 0)
            continue;

        skip = 0;

        for (i = 0; i < *num; i++)
        {
            if (strncmp
                ((char *) &compl[i].text[word_len],
                 (char *) &temp->str[word_len], max (len, compl[i].len) - word_len) == 0)
            {
                struct selection this = compl[i];
                for (++i; i < *num; i++)
                {
                    compl[i - 1] = compl[i];
                }
                compl[*num - 1] = this;
                skip = 1;
                break;          /* skip it, already added */
            }
        }
        if (skip != 0)
            continue;

        if (*num == MAX_WORD_COMPLETIONS)
        {
            g_free (compl[0].text);
            for (i = 1; i < *num; i++)
            {
                compl[i - 1] = compl[i];
            }
            (*num)--;
        }
#ifdef HAVE_CHARSET
        {
            GString *recoded;
            recoded = str_convert_to_display (temp->str);

            if (recoded && recoded->len)
                g_string_assign (temp, recoded->str);

            g_string_free (recoded, TRUE);
        }
#endif
        compl[*num].text = g_strdup (temp->str);
        compl[*num].len = temp->len;
        (*num)++;
        start += len;

        /* note the maximal length needed for the completion dialog */
        if (len > max_len)
            max_len = len;
    }

    mc_search_free (srch);
    g_string_free (temp, TRUE);
    g_free (current_word);

    return max_len;
}

/* --------------------------------------------------------------------------------------------- */

static void
edit_insert_column_of_text (WEdit * edit, unsigned char *data, int size, int width,
                            long *start_pos, long *end_pos, int *col1, int *col2)
{
    long cursor;
    int i, col;

    cursor = edit->curs1;
    col = edit_get_col (edit);

    for (i = 0; i < size; i++)
    {
        if (data[i] != '\n')
            edit_insert (edit, data[i]);
        else
        {                       /* fill in and move to next line */
            int l;
            long p;

            if (edit_get_byte (edit, edit->curs1) != '\n')
            {
                l = width - (edit_get_col (edit) - col);
                while (l > 0)
                {
                    edit_insert (edit, ' ');
                    l -= space_width;
                }
            }
            for (p = edit->curs1;; p++)
            {
                if (p == edit->last_byte)
                {
                    edit_cursor_move (edit, edit->last_byte - edit->curs1);
                    edit_insert_ahead (edit, '\n');
                    p++;
                    break;
                }
                if (edit_get_byte (edit, p) == '\n')
                {
                    p++;
                    break;
                }
            }
            edit_cursor_move (edit, edit_move_forward3 (edit, p, col, 0) - edit->curs1);
            l = col - edit_get_col (edit);
            while (l >= space_width)
            {
                edit_insert (edit, ' ');
                l -= space_width;
            }
        }
    }

    *col1 = col;
    *col2 = col + width;
    *start_pos = cursor;
    *end_pos = edit->curs1;
    edit_cursor_move (edit, cursor - edit->curs1);
}

/* --------------------------------------------------------------------------------------------- */

static int
edit_macro_comparator (gconstpointer * macro1, gconstpointer * macro2)
{
    const macros_t *m1 = (const macros_t *) macro1;
    const macros_t *m2 = (const macros_t *) macro2;

    return m1->hotkey - m2->hotkey;
}

/* --------------------------------------------------------------------------------------------- */

static void
edit_macro_sort_by_hotkey (void)
{
    if (macros_list != NULL && macros_list->len != 0)
        g_array_sort (macros_list, (GCompareFunc) edit_macro_comparator);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
edit_get_macro (WEdit * edit, int hotkey, const macros_t ** macros, guint * indx)
{
    const macros_t *array_start = &g_array_index (macros_list, struct macros_t, 0);
    macros_t *result;
    macros_t search_macro;

    (void) edit;

    search_macro.hotkey = hotkey;
    result = bsearch (&search_macro, macros_list->data, macros_list->len,
                      sizeof (macros_t), (GCompareFunc) edit_macro_comparator);

    if (result != NULL && result->macro != NULL)
    {
        *indx = (result - array_start);
        *macros = result;
        return TRUE;
    }
    *indx = 0;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/** returns FALSE on error */

static gboolean
edit_delete_macro (WEdit * edit, int hotkey)
{
    mc_config_t *macros_config = NULL;
    const char *section_name = "editor";
    gchar *macros_fname;
    guint indx;
    char *keyname;
    const macros_t *macros = NULL;

    /* clear array of actions for current hotkey */
    while (edit_get_macro (edit, hotkey, &macros, &indx))
    {
        if (macros->macro != NULL)
            g_array_free (macros->macro, TRUE);
        macros = NULL;
        g_array_remove_index (macros_list, indx);
        edit_macro_sort_by_hotkey ();
    }

    macros_fname = g_build_filename (mc_config_get_data_path (), MC_MACRO_FILE, (char *) NULL);
    macros_config = mc_config_init (macros_fname);
    g_free (macros_fname);

    if (macros_config == NULL)
        return FALSE;

    keyname = lookup_key_by_code (hotkey);
    while (mc_config_del_key (macros_config, section_name, keyname))
        ;
    g_free (keyname);
    mc_config_save_file (macros_config, NULL);
    mc_config_deinit (macros_config);
    return TRUE;
}


/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
edit_help_cmd (WEdit * edit)
{
    ev_help_t event_data = { NULL, "[Internal File Editor]" };
    mc_event_raise (MCEVENT_GROUP_CORE, "help", &event_data);

    edit->force |= REDRAW_COMPLETELY;
}

/* --------------------------------------------------------------------------------------------- */

void
edit_refresh_cmd (WEdit * edit)
{
#ifdef HAVE_SLANG
    int color;

    edit_get_syntax_color (edit, -1, &color);
    tty_touch_screen ();
    mc_refresh ();
#else
    (void) edit;

    clr_scr ();
    repaint_screen ();
#endif /* !HAVE_SLANG */
    tty_keypad (TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
menu_save_mode_cmd (void)
{
    /* diaog sizes */
    const int DLG_X = 38;
    const int DLG_Y = 13;

    char *str_result;

    const char *str[] = {
        N_("&Quick save"),
        N_("&Safe save"),
        N_("&Do backups with following extension:")
    };

    QuickWidget widgets[] = {
        /* 0 */
        QUICK_BUTTON (18, DLG_X, DLG_Y - 3, DLG_Y, N_("&Cancel"), B_CANCEL, NULL),
        /* 1 */
        QUICK_BUTTON (6, DLG_X, DLG_Y - 3, DLG_Y, N_("&OK"), B_ENTER, NULL),
        /* 2 */
        QUICK_CHECKBOX (4, DLG_X, 8, DLG_Y, N_("Check &POSIX new line"), &option_check_nl_at_eof),
        /* 3 */
        QUICK_INPUT (8, DLG_X, 6, DLG_Y, option_backup_ext, 9, 0, "edit-backup-ext", &str_result),
        /* 4 */
        QUICK_RADIO (4, DLG_X, 3, DLG_Y, 3, str, &option_save_mode),
        QUICK_END
    };

    QuickDialog dialog = {
        DLG_X, DLG_Y, -1, -1, N_("Edit Save Mode"),
        "[Edit Save Mode]", widgets, NULL, FALSE
    };

    size_t i;
    size_t maxlen = 0;
    size_t w0, w1, b_len, w3;

    assert (option_backup_ext != NULL);

    /* OK/Cancel buttons */
    w0 = str_term_width1 (_(widgets[0].u.button.text)) + 3;
    w1 = str_term_width1 (_(widgets[1].u.button.text)) + 5;     /* default button */
    b_len = w0 + w1 + 3;

    maxlen = max (b_len, (size_t) str_term_width1 (_(dialog.title)) + 2);

    w3 = 0;
    for (i = 0; i < 3; i++)
    {
#ifdef ENABLE_NLS
        str[i] = _(str[i]);
#endif
        w3 = max (w3, (size_t) str_term_width1 (str[i]));
    }

    maxlen = max (maxlen, w3 + 4);

    dialog.xlen = min ((size_t) COLS, maxlen + 8);

    widgets[3].u.input.len = w3;
    widgets[1].relative_x = (dialog.xlen - b_len) / 2;
    widgets[0].relative_x = widgets[1].relative_x + w0 + 2;

    for (i = 0; i < sizeof (widgets) / sizeof (widgets[0]); i++)
        widgets[i].x_divisions = dialog.xlen;

    if (quick_dialog (&dialog) != B_CANCEL)
    {
        g_free (option_backup_ext);
        option_backup_ext = str_result;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
edit_set_filename (WEdit * edit, const char *name)
{
    g_free (edit->filename);

    if (name == NULL)
        name = "";

    edit->filename = tilde_expand (name);
    if (edit->dir == NULL && !g_path_is_absolute (name))
        edit->dir = vfs_get_current_dir ();
}

/* --------------------------------------------------------------------------------------------- */
/* Here we want to warn the users of overwriting an existing file,
   but only if they have made a change to the filename */
/* returns 1 on success */
int
edit_save_as_cmd (WEdit * edit)
{
    /* This heads the 'Save As' dialog box */
    char *exp;
    int save_lock = 0;
    int different_filename = 0;

    if (!edit_check_newline (edit))
        return 0;

    exp = edit_get_save_file_as (edit);
    edit_push_undo_action (edit, KEY_PRESS + edit->start_display);

    if (exp)
    {
        if (!*exp)
        {
            g_free (exp);
            edit->force |= REDRAW_COMPLETELY;
            return 0;
        }
        else
        {
            int rv;
            if (strcmp (edit->filename, exp))
            {
                int file;
                different_filename = 1;
                file = mc_open (exp, O_RDONLY | O_BINARY);
                if (file != -1)
                {
                    /* the file exists */
                    mc_close (file);
                    /* Overwrite the current file or cancel the operation */
                    if (edit_query_dialog2
                        (_("Warning"),
                         _("A file already exists with this name"), _("&Overwrite"), _("&Cancel")))
                    {
                        edit->force |= REDRAW_COMPLETELY;
                        g_free (exp);
                        return 0;
                    }
                }
                else
                {
                    edit->stat1.st_mode |= S_IWUSR;
                }
                save_lock = lock_file (exp);
            }
            else
            {
                /* filenames equal, check if already locked */
                if (!edit->locked && !edit->delete_file)
                    save_lock = lock_file (exp);
            }

            if (different_filename)
            {
                /*
                 * Allow user to write into saved (under another name) file
                 * even if original file had r/o user permissions.
                 */
                edit->stat1.st_mode |= S_IWRITE;
            }

            rv = edit_save_file (edit, exp);
            switch (rv)
            {
            case 1:
                /* Succesful, so unlock both files */
                if (different_filename)
                {
                    if (save_lock)
                        unlock_file (exp);
                    if (edit->locked)
                        edit->locked = edit_unlock_file (edit);
                }
                else
                {
                    if (edit->locked || save_lock)
                        edit->locked = edit_unlock_file (edit);
                }

                edit_set_filename (edit, exp);
                if (edit->lb != LB_ASIS)
                    edit_reload (edit, exp);
                g_free (exp);
                edit->modified = 0;
                edit->delete_file = 0;
                if (different_filename)
                    edit_load_syntax (edit, NULL, edit->syntax_type);
                edit->force |= REDRAW_COMPLETELY;
                return 1;
            default:
                edit_error_dialog (_("Save as"), get_sys_error (_("Cannot save file")));
                /* fallthrough */
            case -1:
                /* Failed, so maintain modify (not save) lock */
                if (save_lock)
                    unlock_file (exp);
                g_free (exp);
                edit->force |= REDRAW_COMPLETELY;
                return 0;
            }
        }
    }
    edit->force |= REDRAW_COMPLETELY;
    return 0;
}

/* {{{ Macro stuff starts here */
/* --------------------------------------------------------------------------------------------- */

void
edit_delete_macro_cmd (WEdit * edit)
{
    int hotkey;

    hotkey = editcmd_dialog_raw_key_query (_("Delete macro"), _("Press macro hotkey:"), 1);

    if (hotkey != 0 && !edit_delete_macro (edit, hotkey))
        message (D_ERROR, _("Delete macro"), _("Macro not deleted"));
}

/* --------------------------------------------------------------------------------------------- */

/** returns FALSE on error */
gboolean
edit_execute_macro (WEdit * edit, int hotkey)
{
    gboolean res = FALSE;

    if (hotkey != 0)
    {
        const macros_t *macros;
        guint indx;

        if (edit_get_macro (edit, hotkey, &macros, &indx) &&
            macros->macro != NULL && macros->macro->len != 0)
        {
            guint i;

            edit->force |= REDRAW_PAGE;

            for (i = 0; i < macros->macro->len; i++)
            {
                const macro_action_t *m_act;

                m_act = &g_array_index (macros->macro, struct macro_action_t, i);
                edit_execute_cmd (edit, m_act->action, m_act->ch);
                res = TRUE;
            }
        }
    }
    edit_update_screen (edit);
    return res;
}

/* --------------------------------------------------------------------------------------------- */

/** returns FALSE on error */
gboolean
edit_store_macro_cmd (WEdit * edit)
{
    int i;
    int hotkey;
    GString *marcros_string;
    mc_config_t *macros_config = NULL;
    const char *section_name = "editor";
    gchar *macros_fname;
    GArray *macros;             /* current macro */
    int tmp_act;
    gboolean have_macro = FALSE;
    char *keyname = NULL;

    hotkey = editcmd_dialog_raw_key_query (_("Save macro"), _("Press the macro's new hotkey:"), 1);
    if (hotkey == ESC_CHAR)
        return FALSE;

    tmp_act = keybind_lookup_keymap_command (editor_map, hotkey);

    /* return FALSE if try assign macro into restricted hotkeys */
    if (tmp_act == CK_MacroStartRecord
        || tmp_act == CK_MacroStopRecord || tmp_act == CK_MacroStartStopRecord)
        return FALSE;

    edit_delete_macro (edit, hotkey);

    macros_fname = g_build_filename (mc_config_get_data_path (), MC_MACRO_FILE, (char *) NULL);
    macros_config = mc_config_init (macros_fname);
    g_free (macros_fname);

    if (macros_config == NULL)
        return FALSE;

    edit_push_undo_action (edit, KEY_PRESS + edit->start_display);

    marcros_string = g_string_sized_new (250);
    macros = g_array_new (TRUE, FALSE, sizeof (macro_action_t));

    keyname = lookup_key_by_code (hotkey);

    for (i = 0; i < macro_index; i++)
    {
        macro_action_t m_act;
        const char *action_name;

        action_name = keybind_lookup_actionname (record_macro_buf[i].action);

        if (action_name == NULL)
            break;

        m_act.action = record_macro_buf[i].action;
        m_act.ch = record_macro_buf[i].ch;
        g_array_append_val (macros, m_act);
        have_macro = TRUE;
        g_string_append_printf (marcros_string, "%s:%i;", action_name,
                                (int) record_macro_buf[i].ch);
    }
    if (have_macro)
    {
        macros_t macro;
        macro.hotkey = hotkey;
        macro.macro = macros;
        g_array_append_val (macros_list, macro);
        mc_config_set_string (macros_config, section_name, keyname, marcros_string->str);
    }
    else
        mc_config_del_key (macros_config, section_name, keyname);

    g_free (keyname);
    edit_macro_sort_by_hotkey ();

    g_string_free (marcros_string, TRUE);
    mc_config_save_file (macros_config, NULL);
    mc_config_deinit (macros_config);
    return TRUE;
}

 /* --------------------------------------------------------------------------------------------- */

gboolean
edit_repeat_macro_cmd (WEdit * edit)
{
    int i, j;
    char *f;
    long count_repeat;
    char *error = NULL;

    f = input_dialog (_("Repeat last commands"), _("Repeat times:"), MC_HISTORY_EDIT_REPEAT, NULL);
    if (f == NULL || *f == '\0')
    {
        g_free (f);
        return FALSE;
    }

    count_repeat = strtol (f, &error, 0);

    if (*error != '\0')
    {
        g_free (f);
        return FALSE;
    }

    g_free (f);

    edit_push_undo_action (edit, KEY_PRESS + edit->start_display);
    edit->force |= REDRAW_PAGE;

    for (j = 0; j < count_repeat; j++)
        for (i = 0; i < macro_index; i++)
            edit_execute_cmd (edit, record_macro_buf[i].action, record_macro_buf[i].ch);
    edit_update_screen (edit);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/** return FALSE on error */

gboolean
edit_load_macro_cmd (WEdit * edit)
{
    mc_config_t *macros_config = NULL;
    gchar **profile_keys, **keys;
    gchar **values, **curr_values;
    gsize len, values_len;
    const char *section_name = "editor";
    gchar *macros_fname;
    int hotkey;

    (void) edit;

    macros_fname = g_build_filename (mc_config_get_data_path (), MC_MACRO_FILE, (char *) NULL);
    macros_config = mc_config_init (macros_fname);
    g_free (macros_fname);

    if (macros_config == NULL)
        return FALSE;

    profile_keys = keys = mc_config_get_keys (macros_config, section_name, &len);
    while (*profile_keys != NULL)
    {
        gboolean have_macro;
        GArray *macros;
        macros_t macro;

        macros = g_array_new (TRUE, FALSE, sizeof (macro_action_t));

        curr_values = values = mc_config_get_string_list (macros_config, section_name,
                                                          *profile_keys, &values_len);
        hotkey = lookup_key (*profile_keys, NULL);
        have_macro = FALSE;

        while (*curr_values != NULL && *curr_values[0] != '\0')
        {
            char **macro_pair = NULL;

            macro_pair = g_strsplit (*curr_values, ":", 2);

            if (macro_pair != NULL)
            {
                macro_action_t m_act;
                if (macro_pair[0] == NULL || macro_pair[0][0] == '\0')
                    m_act.action = 0;
                else
                {
                    m_act.action = keybind_lookup_action (macro_pair[0]);
                    g_free (macro_pair[0]);
                    macro_pair[0] = NULL;
                }
                if (macro_pair[1] == NULL || macro_pair[1][0] == '\0')
                    m_act.ch = -1;
                else
                {
                    m_act.ch = strtol (macro_pair[1], NULL, 0);
                    g_free (macro_pair[1]);
                    macro_pair[1] = NULL;
                }
                if (m_act.action != 0)
                {
                    /* a shell command */
                    if ((m_act.action / CK_PipeBlock (0)) == 1)
                    {
                        m_act.action = CK_PipeBlock (0) + (m_act.ch > 0 ? m_act.ch : 0);
                        m_act.ch = -1;
                    }
                    g_array_append_val (macros, m_act);
                    have_macro = TRUE;
                }
                g_strfreev (macro_pair);
                macro_pair = NULL;
            }
            curr_values++;
        }
        if (have_macro)
        {
            macro.hotkey = hotkey;
            macro.macro = macros;
            g_array_append_val (macros_list, macro);
        }
        profile_keys++;
        g_strfreev (values);
    }
    g_strfreev (keys);
    mc_config_deinit (macros_config);
    edit_macro_sort_by_hotkey ();
    return TRUE;
}

/* }}} Macro stuff end here */

/* --------------------------------------------------------------------------------------------- */
/** returns 1 on success */

int
edit_save_confirm_cmd (WEdit * edit)
{
    gchar *f = NULL;

    if (!edit_check_newline (edit))
        return 0;

    if (edit_confirm_save)
    {
        f = g_strdup_printf (_("Confirm save file: \"%s\""), edit->filename);
        if (edit_query_dialog2 (_("Save file"), f, _("&Save"), _("&Cancel")))
        {
            g_free (f);
            return 0;
        }
        g_free (f);
    }
    return edit_save_cmd (edit);
}

/* --------------------------------------------------------------------------------------------- */

/** returns TRUE on success */
gboolean
edit_new_cmd (WEdit * edit)
{
    if (edit->modified && edit_query_dialog2 (_("Warning"),
                                              _
                                              ("Current text was modified without a file save.\nContinue discards these changes"),
                                              _("C&ontinue"), _("&Cancel")))
    {
        edit->force |= REDRAW_COMPLETELY;
        return TRUE;
    }

    edit->force |= REDRAW_COMPLETELY;

    return edit_renew (edit);   /* if this gives an error, something has really screwed up */
}

/* --------------------------------------------------------------------------------------------- */

gboolean
edit_load_cmd (WEdit * edit, edit_current_file_t what)
{
    char *exp;

    if (edit->modified && (edit_query_dialog2 (_("Warning"),
                                               _("Current text was modified without a file save.\n"
                                                 "Continue discards these changes"), _("C&ontinue"),
                                               _("&Cancel")) == 1))
    {
        edit->force |= REDRAW_COMPLETELY;
        return TRUE;
    }

    switch (what)
    {
    case EDIT_FILE_COMMON:
        exp = input_expand_dialog (_("Load"), _("Enter file name:"),
                                   MC_HISTORY_EDIT_LOAD, edit->filename);

        if (exp != NULL)
        {
            if (*exp != '\0')
                edit_load_file_from_filename (edit, exp);
            g_free (exp);
        }
        break;

    case EDIT_FILE_SYNTAX:
        edit_load_syntax_file (edit);
        break;

    case EDIT_FILE_MENU:
        edit_load_menu_file (edit);
        break;

    default:
        break;
    }

    edit->force |= REDRAW_COMPLETELY;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
   if mark2 is -1 then marking is from mark1 to the cursor.
   Otherwise its between the markers. This handles this.
   Returns 1 if no text is marked.
 */

int
eval_marks (WEdit * edit, long *start_mark, long *end_mark)
{
    if (edit->mark1 != edit->mark2)
    {
        long start_bol, start_eol;
        long end_bol, end_eol;
        long col1, col2;
        long diff1, diff2;
        long end_mark_curs;

        if (edit->end_mark_curs < 0)
            end_mark_curs = edit->curs1;
        else
            end_mark_curs = edit->end_mark_curs;

        if (edit->mark2 >= 0)
        {
            *start_mark = min (edit->mark1, edit->mark2);
            *end_mark = max (edit->mark1, edit->mark2);
        }
        else
        {
            *start_mark = min (edit->mark1, end_mark_curs);
            *end_mark = max (edit->mark1, end_mark_curs);
            edit->column2 = edit->curs_col + edit->over_col;
        }

        if (edit->column_highlight
            && (((edit->mark1 > end_mark_curs) && (edit->column1 < edit->column2))
                || ((edit->mark1 < end_mark_curs) && (edit->column1 > edit->column2))))
        {
            start_bol = edit_bol (edit, *start_mark);
            start_eol = edit_eol (edit, start_bol - 1) + 1;
            end_bol = edit_bol (edit, *end_mark);
            end_eol = edit_eol (edit, *end_mark);
            col1 = min (edit->column1, edit->column2);
            col2 = max (edit->column1, edit->column2);

            diff1 =
                edit_move_forward3 (edit, start_bol, col2, 0) - edit_move_forward3 (edit, start_bol,
                                                                                    col1, 0);
            diff2 =
                edit_move_forward3 (edit, end_bol, col2, 0) - edit_move_forward3 (edit, end_bol,
                                                                                  col1, 0);

            *start_mark -= diff1;
            *end_mark += diff2;
            *start_mark = max (*start_mark, start_eol);
            *end_mark = min (*end_mark, end_eol);
        }
        return 0;
    }
    else
    {
        *start_mark = *end_mark = 0;
        edit->column2 = edit->column1 = 0;
        return 1;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
edit_insert_over (WEdit * edit)
{
    int i;

    for (i = 0; i < edit->over_col; i++)
    {
        edit_insert (edit, ' ');
    }
    edit->over_col = 0;
}

/* --------------------------------------------------------------------------------------------- */

int
edit_insert_column_of_text_from_file (WEdit * edit, int file,
                                      long *start_pos, long *end_pos, int *col1, int *col2)
{
    long cursor;
    int col;
    int blocklen = -1, width = 0;
    unsigned char *data;

    cursor = edit->curs1;
    col = edit_get_col (edit);
    data = g_malloc0 (TEMP_BUF_LEN);

    while ((blocklen = mc_read (file, (char *) data, TEMP_BUF_LEN)) > 0)
    {
        int i;
        for (width = 0; width < blocklen; width++)
        {
            if (data[width] == '\n')
                break;
        }
        for (i = 0; i < blocklen; i++)
        {
            if (data[i] == '\n')
            {                   /* fill in and move to next line */
                int l;
                long p;
                if (edit_get_byte (edit, edit->curs1) != '\n')
                {
                    l = width - (edit_get_col (edit) - col);
                    while (l > 0)
                    {
                        edit_insert (edit, ' ');
                        l -= space_width;
                    }
                }
                for (p = edit->curs1;; p++)
                {
                    if (p == edit->last_byte)
                    {
                        edit_cursor_move (edit, edit->last_byte - edit->curs1);
                        edit_insert_ahead (edit, '\n');
                        p++;
                        break;
                    }
                    if (edit_get_byte (edit, p) == '\n')
                    {
                        p++;
                        break;
                    }
                }
                edit_cursor_move (edit, edit_move_forward3 (edit, p, col, 0) - edit->curs1);
                l = col - edit_get_col (edit);
                while (l >= space_width)
                {
                    edit_insert (edit, ' ');
                    l -= space_width;
                }
                continue;
            }
            edit_insert (edit, data[i]);
        }
    }
    *col1 = col;
    *col2 = col + width;
    *start_pos = cursor;
    *end_pos = edit->curs1;
    edit_cursor_move (edit, cursor - edit->curs1);
    g_free (data);

    return blocklen;
}

/* --------------------------------------------------------------------------------------------- */

void
edit_block_copy_cmd (WEdit * edit)
{
    long start_mark, end_mark, current = edit->curs1;
    long col_delta = 0;
    long mark1, mark2;
    int c1, c2;
    int size;
    unsigned char *copy_buf;

    edit_update_curs_col (edit);
    if (eval_marks (edit, &start_mark, &end_mark))
        return;

    copy_buf = edit_get_block (edit, start_mark, end_mark, &size);

    /* all that gets pushed are deletes hence little space is used on the stack */

    edit_push_markers (edit);

    if (edit->column_highlight)
    {
        col_delta = abs (edit->column2 - edit->column1);
        edit_insert_column_of_text (edit, copy_buf, size, col_delta, &mark1, &mark2, &c1, &c2);
    }
    else
    {
        while (size--)
            edit_insert_ahead (edit, copy_buf[size]);
    }

    g_free (copy_buf);
    edit_scroll_screen_over_cursor (edit);

    if (edit->column_highlight)
        edit_set_markers (edit, edit->curs1, mark2, c1, c2);
    else if (start_mark < current && end_mark > current)
        edit_set_markers (edit, start_mark, end_mark + end_mark - start_mark, 0, 0);

    edit->force |= REDRAW_PAGE;
}


/* --------------------------------------------------------------------------------------------- */

void
edit_block_move_cmd (WEdit * edit)
{
    long current;
    unsigned char *copy_buf = NULL;
    long start_mark, end_mark;
    long line;

    if (eval_marks (edit, &start_mark, &end_mark))
        return;

    line = edit->curs_line;
    if (edit->mark2 < 0)
        edit_mark_cmd (edit, 0);
    edit_push_markers (edit);

    if (edit->column_highlight)
    {
        long mark1, mark2;
        int size;
        int b_width = 0;
        int c1, c2;
        int x, x2;

        c1 = min (edit->column1, edit->column2);
        c2 = max (edit->column1, edit->column2);
        b_width = (c2 - c1);

        edit_update_curs_col (edit);

        x = edit->curs_col;
        x2 = x + edit->over_col;

        /* do nothing when cursor inside first line of selected area */
        if ((edit_eol (edit, edit->curs1) == edit_eol (edit, start_mark)) && (x2 > c1 && x2 <= c2))
            return;

        if (edit->curs1 > start_mark && edit->curs1 < edit_eol (edit, end_mark))
        {
            if (x > c2)
                x -= b_width;
            else if (x > c1 && x <= c2)
                x = c1;
        }
        /* save current selection into buffer */
        copy_buf = edit_get_block (edit, start_mark, end_mark, &size);

        /* remove current selection */
        edit_block_delete_cmd (edit);

        edit->over_col = max (0, edit->over_col - b_width);
        /* calculate the cursor pos after delete block */
        current = edit_move_forward3 (edit, edit_bol (edit, edit->curs1), x, 0);
        edit_cursor_move (edit, current - edit->curs1);
        edit_scroll_screen_over_cursor (edit);

        /* add TWS if need before block insertion */
        if (option_cursor_beyond_eol && edit->over_col > 0)
            edit_insert_over (edit);

        edit_insert_column_of_text (edit, copy_buf, size, b_width, &mark1, &mark2, &c1, &c2);
        edit_set_markers (edit, mark1, mark2, c1, c2);
    }
    else
    {
        long count;

        current = edit->curs1;
        copy_buf = g_malloc0 (end_mark - start_mark);
        edit_cursor_move (edit, start_mark - edit->curs1);
        edit_scroll_screen_over_cursor (edit);
        count = start_mark;
        while (count < end_mark)
        {
            copy_buf[end_mark - count - 1] = edit_delete (edit, 1);
            count++;
        }
        edit_scroll_screen_over_cursor (edit);
        edit_cursor_move (edit,
                          current - edit->curs1 -
                          (((current - edit->curs1) > 0) ? end_mark - start_mark : 0));
        edit_scroll_screen_over_cursor (edit);
        while (count-- > start_mark)
            edit_insert_ahead (edit, copy_buf[end_mark - count - 1]);
        edit_set_markers (edit, edit->curs1, edit->curs1 + end_mark - start_mark, 0, 0);
    }

    edit_scroll_screen_over_cursor (edit);
    g_free (copy_buf);
    edit->force |= REDRAW_PAGE;
}

/* --------------------------------------------------------------------------------------------- */
/** returns 1 if canceelled by user */

int
edit_block_delete_cmd (WEdit * edit)
{
    long start_mark, end_mark;
    if (eval_marks (edit, &start_mark, &end_mark))
    {
        edit_delete_line (edit);
        return 0;
    }
    return edit_block_delete (edit);
}

/* --------------------------------------------------------------------------------------------- */
/** call with edit = 0 before shutdown to close memory leaks */

void
edit_replace_cmd (WEdit * edit, int again)
{
    /* 1 = search string, 2 = replace with */
    static char *saved1 = NULL; /* saved default[123] */
    static char *saved2 = NULL;
    char *input1 = NULL;        /* user input from the dialog */
    char *input2 = NULL;
    char *disp1 = NULL;
    char *disp2 = NULL;
    long times_replaced = 0;
    gboolean once_found = FALSE;

    if (!edit)
    {
        g_free (saved1), saved1 = NULL;
        g_free (saved2), saved2 = NULL;
        return;
    }

    edit->force |= REDRAW_COMPLETELY;

    if (again && !saved1 && !saved2)
        again = 0;

    if (again)
    {
        input1 = g_strdup (saved1 ? saved1 : "");
        input2 = g_strdup (saved2 ? saved2 : "");
    }
    else
    {
        char *tmp_inp1, *tmp_inp2;
        disp1 = edit_replace_cmd__conv_to_display (saved1 ? saved1 : (char *) "");
        disp2 = edit_replace_cmd__conv_to_display (saved2 ? saved2 : (char *) "");

        edit_push_undo_action (edit, KEY_PRESS + edit->start_display);

        editcmd_dialog_replace_show (edit, disp1, disp2, &input1, &input2);

        g_free (disp1);
        g_free (disp2);

        if (input1 == NULL || *input1 == '\0')
        {
            edit->force = REDRAW_COMPLETELY;
            goto cleanup;
        }

        tmp_inp1 = input1;
        tmp_inp2 = input2;
        input1 = edit_replace_cmd__conv_to_input (input1);
        input2 = edit_replace_cmd__conv_to_input (input2);
        g_free (tmp_inp1);
        g_free (tmp_inp2);

        g_free (saved1), saved1 = g_strdup (input1);
        g_free (saved2), saved2 = g_strdup (input2);

        mc_search_free (edit->search);
        edit->search = NULL;
    }

    if (!edit->search)
    {
        edit->search = mc_search_new (input1, -1);
        if (edit->search == NULL)
        {
            edit->search_start = edit->curs1;
            goto cleanup;
        }
        edit->search->search_type = edit_search_options.type;
        edit->search->is_all_charsets = edit_search_options.all_codepages;
        edit->search->is_case_sensitive = edit_search_options.case_sens;
        edit->search->whole_words = edit_search_options.whole_words;
        edit->search->search_fn = edit_search_cmd_callback;
    }

    if (edit->found_len && edit->search_start == edit->found_start + 1
        && edit_search_options.backwards)
        edit->search_start--;

    if (edit->found_len && edit->search_start == edit->found_start - 1
        && !edit_search_options.backwards)
        edit->search_start++;

    do
    {
        gsize len = 0;

        if (!editcmd_find (edit, &len))
        {
            if (!(edit->search->error == MC_SEARCH_E_OK ||
                  (once_found && edit->search->error == MC_SEARCH_E_NOTFOUND)))
            {
                edit_error_dialog (_("Search"), edit->search->error_str);
            }
            break;
        }
        once_found = TRUE;

        edit->search_start = edit->search->normal_offset;
        /*returns negative on not found or error in pattern */

        if ((edit->search_start >= 0) && (edit->search_start < edit->last_byte))
        {
            gsize i;
            GString *tmp_str, *repl_str;

            edit->found_start = edit->search_start;
            i = edit->found_len = len;

            edit_cursor_move (edit, edit->search_start - edit->curs1);
            edit_scroll_screen_over_cursor (edit);

            if (edit->replace_mode == 0)
            {
                int l;
                int prompt;

                l = edit->curs_row - edit->widget.lines / 3;
                if (l > 0)
                    edit_scroll_downward (edit, l);
                if (l < 0)
                    edit_scroll_upward (edit, -l);

                edit_scroll_screen_over_cursor (edit);
                edit->force |= REDRAW_PAGE;
                edit_render_keypress (edit);

                /*so that undo stops at each query */
                edit_push_key_press (edit);
                /* and prompt 2/3 down */
                disp1 = edit_replace_cmd__conv_to_display (saved1);
                disp2 = edit_replace_cmd__conv_to_display (saved2);
                prompt = editcmd_dialog_replace_prompt_show (edit, disp1, disp2, -1, -1);
                g_free (disp1);
                g_free (disp2);

                if (prompt == B_REPLACE_ALL)
                    edit->replace_mode = 1;
                else if (prompt == B_SKIP_REPLACE)
                {
                    if (edit_search_options.backwards)
                        edit->search_start--;
                    else
                        edit->search_start++;
                    continue;   /* loop */
                }
                else if (prompt == B_CANCEL)
                {
                    edit->replace_mode = -1;
                    break;      /* loop */
                }
            }

            /* don't process string each time */
            tmp_str = g_string_new (input2);
            repl_str = mc_search_prepare_replace_str (edit->search, tmp_str);
            g_string_free (tmp_str, TRUE);

            if (edit->search->error != MC_SEARCH_E_OK)
            {
                edit_error_dialog (_("Replace"), edit->search->error_str);
                g_string_free (repl_str, TRUE);
                break;
            }

            /* delete then insert new */
            for (i = 0; i < len; i++)
                edit_delete (edit, 1);

            for (i = 0; i < repl_str->len; i++)
                edit_insert (edit, repl_str->str[i]);

            edit->found_len = repl_str->len;
            g_string_free (repl_str, TRUE);
            times_replaced++;

            /* so that we don't find the same string again */
            if (edit_search_options.backwards)
                edit->search_start--;
            else
            {
                edit->search_start += edit->found_len;

                if (edit->search_start >= edit->last_byte)
                    break;
            }

            edit_scroll_screen_over_cursor (edit);
        }
        else
        {
            /* try and find from right here for next search */
            edit->search_start = edit->curs1;
            edit_update_curs_col (edit);

            edit->force |= REDRAW_PAGE;
            edit_render_keypress (edit);

            if (times_replaced == 0)
                query_dialog (_("Replace"), _("Search string not found"), D_NORMAL, 1, _("&OK"));
            break;
        }
    }
    while (edit->replace_mode >= 0);

    edit_scroll_screen_over_cursor (edit);
    edit->force |= REDRAW_COMPLETELY;
    edit_render_keypress (edit);

    if ((edit->replace_mode == 1) && (times_replaced != 0))
        message (D_NORMAL, _("Replace"), _("%ld replacements made"), times_replaced);

  cleanup:
    g_free (input1);
    g_free (input2);
}

/* --------------------------------------------------------------------------------------------- */

int
edit_search_cmd_callback (const void *user_data, gsize char_offset)
{
    return edit_get_byte ((WEdit *) user_data, (long) char_offset);
}

/* --------------------------------------------------------------------------------------------- */

void
edit_search_cmd (WEdit * edit, gboolean again)
{
    if (edit == NULL)
        return;

    if (!again)
        edit_search (edit);
    else if (edit->last_search_string != NULL)
        edit_do_search (edit);
    else
    {
        /* find last search string in history */
        GList *history;

        history = history_get (MC_HISTORY_SHARED_SEARCH);
        if (history != NULL && history->data != NULL)
        {
            edit->last_search_string = (char *) history->data;
            history->data = NULL;
            history = g_list_first (history);
            g_list_foreach (history, (GFunc) g_free, NULL);
            g_list_free (history);

            edit->search = mc_search_new (edit->last_search_string, -1);
            if (edit->search == NULL)
            {
                /* if not... then ask for an expression */
                g_free (edit->last_search_string);
                edit->last_search_string = NULL;
                edit_search (edit);
            }
            else
            {
                edit->search->search_type = edit_search_options.type;
                edit->search->is_all_charsets = edit_search_options.all_codepages;
                edit->search->is_case_sensitive = edit_search_options.case_sens;
                edit->search->whole_words = edit_search_options.whole_words;
                edit->search->search_fn = edit_search_cmd_callback;
                edit_do_search (edit);
            }
        }
        else
        {
            /* if not... then ask for an expression */
            g_free (edit->last_search_string);
            edit->last_search_string = NULL;
            edit_search (edit);
        }
    }
}


/* --------------------------------------------------------------------------------------------- */
/**
 * Check if it's OK to close the editor.  If there are unsaved changes,
 * ask user.  Return 1 if it's OK to exit, 0 to continue editing.
 */

gboolean
edit_ok_to_exit (WEdit * edit)
{
    int act;

    if (!edit->modified)
        return TRUE;

    if (!mc_global.widget.midnight_shutdown)
    {
        if (!edit_check_newline (edit))
            return FALSE;

        query_set_sel (2);
        act = edit_query_dialog3 (_("Quit"), _("File was modified. Save with exit?"),
                                  _("&Yes"), _("&No"), _("&Cancel quit"));
    }
    else
    {
        act =
            edit_query_dialog2 (_("Quit"),
                                _("Midnight Commander is being shut down.\nSave modified file?"),
                                _("&Yes"), _("&No"));

        /* Esc is No */
        if (act == -1)
            act = 1;
    }

    switch (act)
    {
    case 0:                    /* Yes */
        edit_push_markers (edit);
        edit_set_markers (edit, 0, 0, 0, 0);
        if (!edit_save_cmd (edit) || mc_global.widget.midnight_shutdown)
            return mc_global.widget.midnight_shutdown;
        break;
    case 1:                    /* No */
        break;
    case 2:                    /* Cancel quit */
    case -1:                   /* Esc */
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/** save block, returns 1 on success */

int
edit_save_block (WEdit * edit, const char *filename, long start, long finish)
{
    int len, file;

    file = mc_open (filename, O_CREAT | O_WRONLY | O_TRUNC,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | O_BINARY);
    if (file == -1)
        return 0;

    if (edit->column_highlight)
    {
        int r;
        r = mc_write (file, VERTICAL_MAGIC, sizeof (VERTICAL_MAGIC));
        if (r > 0)
        {
            unsigned char *block, *p;
            p = block = edit_get_block (edit, start, finish, &len);
            while (len)
            {
                r = mc_write (file, p, len);
                if (r < 0)
                    break;
                p += r;
                len -= r;
            }
            g_free (block);
        }
    }
    else
    {
        unsigned char *buf;
        int i = start, end;
        len = finish - start;
        buf = g_malloc0 (TEMP_BUF_LEN);
        while (start != finish)
        {
            end = min (finish, start + TEMP_BUF_LEN);
            for (; i < end; i++)
                buf[i - start] = edit_get_byte (edit, i);
            len -= mc_write (file, (char *) buf, end - start);
            start = end;
        }
        g_free (buf);
    }
    mc_close (file);
    if (len)
        return 0;
    return 1;
}

/* --------------------------------------------------------------------------------------------- */

void
edit_paste_from_history (WEdit * edit)
{
    (void) edit;
    edit_error_dialog (_("Error"), _("This function is not implemented"));
}

/* --------------------------------------------------------------------------------------------- */

int
edit_copy_to_X_buf_cmd (WEdit * edit)
{
    long start_mark, end_mark;
    if (eval_marks (edit, &start_mark, &end_mark))
        return 0;
    if (!edit_save_block_to_clip_file (edit, start_mark, end_mark))
    {
        edit_error_dialog (_("Copy to clipboard"), get_sys_error (_("Unable to save to file")));
        return 1;
    }
    /* try use external clipboard utility */
    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_file_to_ext_clip", NULL);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

int
edit_cut_to_X_buf_cmd (WEdit * edit)
{
    long start_mark, end_mark;
    if (eval_marks (edit, &start_mark, &end_mark))
        return 0;
    if (!edit_save_block_to_clip_file (edit, start_mark, end_mark))
    {
        edit_error_dialog (_("Cut to clipboard"), _("Unable to save to file"));
        return 1;
    }
    /* try use external clipboard utility */
    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_file_to_ext_clip", NULL);

    edit_block_delete_cmd (edit);
    edit_mark_cmd (edit, 1);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

void
edit_paste_from_X_buf_cmd (WEdit * edit)
{
    gchar *tmp;
    /* try use external clipboard utility */
    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_file_from_ext_clip", NULL);
    tmp = concat_dir_and_file (mc_config_get_cache_path (), EDIT_CLIP_FILE);
    edit_insert_file (edit, tmp);
    g_free (tmp);
}


/* --------------------------------------------------------------------------------------------- */
/**
 * Ask user for the line and go to that line.
 * Negative numbers mean line from the end (i.e. -1 is the last line).
 */

void
edit_goto_cmd (WEdit * edit)
{
    char *f;
    static long line = 0;       /* line as typed, saved as default */
    long l;
    char *error;
    char s[32];

    g_snprintf (s, sizeof (s), "%ld", line);
    f = input_dialog (_("Goto line"), _("Enter line:"), MC_HISTORY_EDIT_GOTO_LINE, line ? s : "");
    if (!f)
        return;

    if (!*f)
    {
        g_free (f);
        return;
    }

    l = strtol (f, &error, 0);
    if (*error)
    {
        g_free (f);
        return;
    }

    line = l;
    if (l < 0)
        l = edit->total_lines + l + 2;
    edit_move_display (edit, l - edit->widget.lines / 2 - 1);
    edit_move_to_line (edit, l - 1);
    edit->force |= REDRAW_COMPLETELY;
    g_free (f);
}


/* --------------------------------------------------------------------------------------------- */
/** Return 1 on success */

int
edit_save_block_cmd (WEdit * edit)
{
    long start_mark, end_mark;
    char *exp, *tmp;

    if (eval_marks (edit, &start_mark, &end_mark))
        return 1;

    tmp = concat_dir_and_file (mc_config_get_cache_path (), EDIT_CLIP_FILE);
    exp =
        input_expand_dialog (_("Save block"), _("Enter file name:"),
                             MC_HISTORY_EDIT_SAVE_BLOCK, tmp);
    g_free (tmp);
    edit_push_undo_action (edit, KEY_PRESS + edit->start_display);
    if (exp)
    {
        if (!*exp)
        {
            g_free (exp);
            return 0;
        }
        else
        {
            if (edit_save_block (edit, exp, start_mark, end_mark))
            {
                g_free (exp);
                edit->force |= REDRAW_COMPLETELY;
                return 1;
            }
            else
            {
                g_free (exp);
                edit_error_dialog (_("Save block"), get_sys_error (_("Cannot save file")));
            }
        }
    }
    edit->force |= REDRAW_COMPLETELY;
    return 0;
}


/* --------------------------------------------------------------------------------------------- */

/** returns TRUE on success */
gboolean
edit_insert_file_cmd (WEdit * edit)
{
    char *tmp;
    char *exp;
    gboolean ret = FALSE;

    tmp = g_build_filename (mc_config_get_cache_path (), EDIT_CLIP_FILE, (char *) NULL);
    exp = input_expand_dialog (_("Insert file"), _("Enter file name:"),
                               MC_HISTORY_EDIT_INSERT_FILE, tmp);
    g_free (tmp);

    edit_push_undo_action (edit, KEY_PRESS + edit->start_display);

    if (exp != NULL)
    {
        if (*exp == '\0')
        {
            g_free (exp);
            return FALSE;
        }

        ret = edit_insert_file (edit, exp) != 0;
        g_free (exp);
        if (!ret)
            edit_error_dialog (_("Insert file"), get_sys_error (_("Cannot insert file")));
    }

    edit->force |= REDRAW_COMPLETELY;
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/** sorts a block, returns -1 on system fail, 1 on cancel and 0 on success */

int
edit_sort_cmd (WEdit * edit)
{
    static char *old = 0;
    char *exp, *tmp;
    long start_mark, end_mark;
    int e;

    if (eval_marks (edit, &start_mark, &end_mark))
    {
        edit_error_dialog (_("Sort block"), _("You must first highlight a block of text"));
        return 0;
    }

    tmp = concat_dir_and_file (mc_config_get_cache_path (), EDIT_BLOCK_FILE);
    edit_save_block (edit, tmp, start_mark, end_mark);
    g_free (tmp);

    exp = input_dialog (_("Run sort"),
                        _("Enter sort options (see manpage) separated by whitespace:"),
                        MC_HISTORY_EDIT_SORT, (old != NULL) ? old : "");

    if (!exp)
        return 1;
    g_free (old);
    old = exp;
    tmp =
        g_strconcat (" sort ", exp, " ", mc_config_get_cache_path (), PATH_SEP_STR EDIT_BLOCK_FILE,
                     " > ", mc_config_get_cache_path (), PATH_SEP_STR EDIT_TEMP_FILE,
                     (char *) NULL);
    e = system (tmp);
    g_free (tmp);
    if (e)
    {
        if (e == -1 || e == 127)
        {
            edit_error_dialog (_("Sort"), get_sys_error (_("Cannot execute sort command")));
        }
        else
        {
            char q[8];
            sprintf (q, "%d ", e);
            tmp = g_strdup_printf (_("Sort returned non-zero: %s"), q);
            edit_error_dialog (_("Sort"), tmp);
            g_free (tmp);
        }
        return -1;
    }

    edit->force |= REDRAW_COMPLETELY;

    if (edit_block_delete_cmd (edit))
        return 1;
    tmp = concat_dir_and_file (mc_config_get_cache_path (), EDIT_TEMP_FILE);
    edit_insert_file (edit, tmp);
    g_free (tmp);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Ask user for a command, execute it and paste its output back to the
 * editor.
 */

int
edit_ext_cmd (WEdit * edit)
{
    char *exp, *tmp;
    int e;

    exp =
        input_dialog (_("Paste output of external command"),
                      _("Enter shell command(s):"), MC_HISTORY_EDIT_PASTE_EXTCMD, NULL);

    if (!exp)
        return 1;

    tmp =
        g_strconcat (exp, " > ", mc_config_get_cache_path (), PATH_SEP_STR EDIT_TEMP_FILE,
                     (char *) NULL);
    e = system (tmp);
    g_free (tmp);
    g_free (exp);

    if (e)
    {
        edit_error_dialog (_("External command"), get_sys_error (_("Cannot execute command")));
        return -1;
    }

    edit->force |= REDRAW_COMPLETELY;
    tmp = concat_dir_and_file (mc_config_get_cache_path (), EDIT_TEMP_FILE);
    edit_insert_file (edit, tmp);
    g_free (tmp);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/** if block is 1, a block must be highlighted and the shell command
   processes it. If block is 0 the shell command is a straight system
   command, that just produces some output which is to be inserted */

void
edit_block_process_cmd (WEdit * edit, int macro_number)
{
    char *fname;
    char *macros_fname = NULL;

    fname = g_strdup_printf ("%s.%i.sh", MC_EXTMACRO_FILE, macro_number);
    macros_fname = g_build_filename (mc_config_get_data_path (), fname, (char *) NULL);
    user_menu (edit, macros_fname, 0);
    g_free (fname);
    g_free (macros_fname);
    edit->force |= REDRAW_COMPLETELY;
}

/* --------------------------------------------------------------------------------------------- */

void
edit_mail_dialog (WEdit * edit)
{
    char *tmail_to;
    char *tmail_subject;
    char *tmail_cc;

    static char *mail_cc_last = 0;
    static char *mail_subject_last = 0;
    static char *mail_to_last = 0;

    QuickWidget quick_widgets[] = {
        /* 0 */ QUICK_BUTTON (6, 10, 9, MAIL_DLG_HEIGHT, N_("&Cancel"), B_CANCEL, NULL),
        /* 1 */ QUICK_BUTTON (2, 10, 9, MAIL_DLG_HEIGHT, N_("&OK"), B_ENTER, NULL),
        /* 2 */ QUICK_INPUT (3, 50, 8, MAIL_DLG_HEIGHT, "", 44, 0, "mail-dlg-input", &tmail_cc),
        /* 3 */ QUICK_LABEL (3, 50, 7, MAIL_DLG_HEIGHT, N_("Copies to")),
        /* 4 */ QUICK_INPUT (3, 50, 6, MAIL_DLG_HEIGHT, "", 44, 0, "mail-dlg-input-2",
                             &tmail_subject),
        /* 5 */ QUICK_LABEL (3, 50, 5, MAIL_DLG_HEIGHT, N_("Subject")),
        /* 6 */ QUICK_INPUT (3, 50, 4, MAIL_DLG_HEIGHT, "", 44, 0, "mail-dlg-input-3", &tmail_to),
        /* 7 */ QUICK_LABEL (3, 50, 3, MAIL_DLG_HEIGHT, N_("To")),
        /* 8 */ QUICK_LABEL (3, 50, 2, MAIL_DLG_HEIGHT, N_("mail -s <subject> -c <cc> <to>")),
        QUICK_END
    };

    QuickDialog Quick_input = {
        50, MAIL_DLG_HEIGHT, -1, -1, N_("Mail"),
        "[Input Line Keys]", quick_widgets, NULL, FALSE
    };

    quick_widgets[2].u.input.text = mail_cc_last ? mail_cc_last : "";
    quick_widgets[4].u.input.text = mail_subject_last ? mail_subject_last : "";
    quick_widgets[6].u.input.text = mail_to_last ? mail_to_last : "";

    if (quick_dialog (&Quick_input) != B_CANCEL)
    {
        g_free (mail_cc_last);
        g_free (mail_subject_last);
        g_free (mail_to_last);
        mail_cc_last = tmail_cc;
        mail_subject_last = tmail_subject;
        mail_to_last = tmail_to;
        pipe_mail (edit, mail_to_last, mail_subject_last, mail_cc_last);
    }
}


/*******************/
/* Word Completion */
/*******************/

/* --------------------------------------------------------------------------------------------- */
/**
 * Complete current word using regular expression search
 * backwards beginning at the current cursor position.
 */

void
edit_complete_word_cmd (WEdit * edit)
{
    gsize i, max_len, word_len = 0, num_compl = 0;
    long word_start = 0;
    unsigned char *bufpos;
    char *match_expr;
    struct selection compl[MAX_WORD_COMPLETIONS];       /* completions */

    /* search start of word to be completed */
    if (!edit_find_word_start (edit, &word_start, &word_len))
        return;

    /* prepare match expression */
    bufpos = &edit->buffers1[word_start >> S_EDIT_BUF_SIZE][word_start & M_EDIT_BUF_SIZE];

    /* match_expr = g_strdup_printf ("\\b%.*s[a-zA-Z_0-9]+", word_len, bufpos); */
    match_expr =
        g_strdup_printf
        ("(^|\\s+|\\b)%.*s[^\\s\\.=\\+\\[\\]\\(\\)\\,\\;\\:\\\"\\'\\-\\?\\/\\|\\\\\\{\\}\\*\\&\\^\\%%\\$#@\\!]+",
         (int) word_len, bufpos);

    /* collect the possible completions              */
    /* start search from begin to end of file */
    max_len =
        edit_collect_completions (edit, word_start, word_len, match_expr,
                                  (struct selection *) &compl, &num_compl);

    if (num_compl > 0)
    {
        /* insert completed word if there is only one match */
        if (num_compl == 1)
        {
            for (i = word_len; i < compl[0].len; i++)
                edit_insert (edit, *(compl[0].text + i));
        }
        /* more than one possible completion => ask the user */
        else
        {
            /* !!! usually only a beep is expected and when <ALT-TAB> is !!! */
            /* !!! pressed again the selection dialog pops up, but that  !!! */
            /* !!! seems to require a further internal state             !!! */
            /*tty_beep (); */

            /* let the user select the preferred completion */
            editcmd_dialog_completion_show (edit, max_len, word_len,
                                            (struct selection *) &compl, num_compl);
        }
    }

    g_free (match_expr);
    /* release memory before return */
    for (i = 0; i < num_compl; i++)
        g_free (compl[i].text);
}

/* --------------------------------------------------------------------------------------------- */

void
edit_select_codepage_cmd (WEdit * edit)
{
#ifdef HAVE_CHARSET
    if (do_select_codepage ())
        edit_set_codeset (edit);

    edit->force = REDRAW_COMPLETELY;
    edit_refresh_cmd (edit);
#else
    (void) edit;
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
edit_insert_literal_cmd (WEdit * edit)
{
    int char_for_insertion = editcmd_dialog_raw_key_query (_("Insert literal"),
                                                           _("Press any key:"), 0);
    edit_execute_key_command (edit, -1, ascii_alpha_to_cntrl (char_for_insertion));
}

/* --------------------------------------------------------------------------------------------- */

void
edit_begin_end_macro_cmd (WEdit * edit)
{
    /* edit is a pointer to the widget */
    if (edit != NULL)
    {
        unsigned long command = macro_index < 0 ? CK_MacroStartRecord : CK_MacroStopRecord;
        edit_execute_key_command (edit, command, -1);
    }
}

 /* --------------------------------------------------------------------------------------------- */

void
edit_begin_end_repeat_cmd (WEdit * edit)
{
    /* edit is a pointer to the widget */
    if (edit != NULL)
    {
        unsigned long command = macro_index < 0 ? CK_RepeatStartRecord : CK_RepeatStopRecord;
        edit_execute_key_command (edit, command, -1);
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
edit_load_forward_cmd (WEdit * edit)
{
    if (edit->modified
        && edit_query_dialog2 (_("Warning"),
                               _("Current text was modified without a file save\n"
                                 "Continue discards these changes"), _("C&ontinue"), _("&Cancel")))
    {
        edit->force |= REDRAW_COMPLETELY;
        return TRUE;
    }

    if (edit_stack_iterator + 1 >= MAX_HISTORY_MOVETO)
        return FALSE;

    if (edit_history_moveto[edit_stack_iterator + 1].line < 1)
        return FALSE;

    edit_stack_iterator++;
    if (edit_history_moveto[edit_stack_iterator].filename != NULL)
        return edit_reload_line (edit, edit_history_moveto[edit_stack_iterator].filename,
                                 edit_history_moveto[edit_stack_iterator].line);

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
edit_load_back_cmd (WEdit * edit)
{
    if (edit->modified
        && edit_query_dialog2 (_("Warning"),
                               _("Current text was modified without a file save\n"
                                 "Continue discards these changes"), _("C&ontinue"), _("&Cancel")))
    {
        edit->force |= REDRAW_COMPLETELY;
        return TRUE;
    }

    if (edit_stack_iterator < 0)
        return FALSE;

    edit_stack_iterator--;
    if (edit_history_moveto[edit_stack_iterator].filename)
        return edit_reload_line (edit, edit_history_moveto[edit_stack_iterator].filename,
                                 edit_history_moveto[edit_stack_iterator].line);

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
edit_get_match_keyword_cmd (WEdit * edit)
{
    gsize word_len = 0, max_len = 0;
    int num_def = 0;
    int i;
    long word_start = 0;
    unsigned char *bufpos;
    char *match_expr;
    char *path = NULL;
    char *ptr = NULL;
    char *tagfile = NULL;

    etags_hash_t def_hash[MAX_DEFINITIONS];

    for (i = 0; i < MAX_DEFINITIONS; i++)
    {
        def_hash[i].filename = NULL;
    }

    /* search start of word to be completed */
    if (!edit_find_word_start (edit, &word_start, &word_len))
        return;

    /* prepare match expression */
    bufpos = &edit->buffers1[word_start >> S_EDIT_BUF_SIZE][word_start & M_EDIT_BUF_SIZE];
    match_expr = g_strdup_printf ("%.*s", (int) word_len, bufpos);

    ptr = g_get_current_dir ();
    path = g_strconcat (ptr, G_DIR_SEPARATOR_S, (char *) NULL);
    g_free (ptr);

    /* Recursive search file 'TAGS' in parent dirs */
    do
    {
        ptr = g_path_get_dirname (path);
        g_free (path);
        path = ptr;
        g_free (tagfile);
        tagfile = mc_build_filename (path, TAGS_NAME, (char *) NULL);
        if (exist_file (tagfile))
            break;
    }
    while (strcmp (path, G_DIR_SEPARATOR_S) != 0);

    if (tagfile)
    {
        num_def =
            etags_set_definition_hash (tagfile, path, match_expr, (etags_hash_t *) & def_hash);
        g_free (tagfile);
    }
    g_free (path);

    max_len = MAX_WIDTH_DEF_DIALOG;
    word_len = 0;
    if (num_def > 0)
    {
        editcmd_dialog_select_definition_show (edit, match_expr, max_len, word_len,
                                               (etags_hash_t *) & def_hash, num_def);
    }
    g_free (match_expr);
}

/* --------------------------------------------------------------------------------------------- */
