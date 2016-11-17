/*
   Panel managing.

   Copyright (C) 1994-2016
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2013

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

/** \file info.c
 *  \brief Source: panel managing
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <inttypes.h>           /* PRIuMAX */

#include "lib/global.h"
#include "lib/unixcompat.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* is_idle() */
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/timefmt.h"        /* file_date() */
#include "lib/util.h"
#include "lib/widget.h"

#include "src/setup.h"          /* panels_options */

#include "midnight.h"           /* the_menubar */
#include "layout.h"
#include "mountlist.h"
#include "info.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#ifndef VERSION
#define VERSION "undefined"
#endif

/*** file scope type declarations ****************************************************************/

struct WInfo
{
    Widget widget;
    gboolean ready;
};

/*** file scope variables ************************************************************************/

static struct my_statfs myfs_stats;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
info_box (WInfo * info)
{
    Widget *w = WIDGET (info);

    const char *title = _("Information");
    const int len = str_term_width1 (title);

    tty_set_normal_attrs ();
    tty_setcolor (NORMAL_COLOR);
    widget_erase (w);
    tty_draw_box (w->y, w->x, w->lines, w->cols, FALSE);

    widget_move (w, 0, (w->cols - len - 2) / 2);
    tty_printf (" %s ", title);

    widget_move (w, 2, 0);
    tty_print_alt_char (ACS_LTEE, FALSE);
    widget_move (w, 2, w->cols - 1);
    tty_print_alt_char (ACS_RTEE, FALSE);
    tty_draw_hline (w->y + 2, w->x + 1, ACS_HLINE, w->cols - 2);
}

/* --------------------------------------------------------------------------------------------- */

static void
info_show_info (WInfo * info)
{
    Widget *w = WIDGET (info);
    char *fname;
    size_t fname_len = 0;
    static int i18n_adjust = 0;
    static const char *file_label;
    GString *buff;
    struct stat st;
    char rp_cwd[PATH_MAX];
    const char *p_rp_cwd;

    if (!is_idle ())
        return;

    info_box (info);

    tty_setcolor (MARKED_COLOR);
    widget_move (w, 1, 3);
    tty_printf (_("Midnight Commander %s"), VERSION);

    if (!info->ready)
        return;

    if (get_current_type () != view_listing)
        return;

    /* don't rely on vpath CWD when cd_symlinks enabled */
    p_rp_cwd = mc_realpath (vfs_path_as_str (current_panel->cwd_vpath), rp_cwd);
    if (p_rp_cwd == NULL)
        p_rp_cwd = vfs_path_as_str (current_panel->cwd_vpath);

    my_statfs (&myfs_stats, p_rp_cwd);

    st = current_panel->dir.list[current_panel->selected].st;

    /* Print only lines which fit */

    if (i18n_adjust == 0)
    {
        /* This printf pattern string is used as a reference for size */
        file_label = _("File: %s");
        i18n_adjust = str_term_width1 (file_label) + 2;
    }

    tty_setcolor (NORMAL_COLOR);

    buff = g_string_new ("");

    switch (w->lines - 2)
    {
        /* Note: all cases are fall-throughs */

    default:

    case 17: {
        /* TODO:
         *   error handling: malloc, popen, fgets*2
         *   handle long lines exceeding panel width
         */
        FILE *command_stream;
        char *command;
        char *command_pos;
        /* command_substr = "file -b -- '';file -bi -- ''" */
        const size_t command_substr_len = 28;
        /* size of command_outputs arbitrarily / staticly set */
        #define CMD_OUT_LEN 1024
        char command_output_1[CMD_OUT_LEN];
        char command_output_2[CMD_OUT_LEN];

        fname = current_panel->dir.list[current_panel->selected].fname;
        fname_len = strlen(fname);
        command = malloc((fname_len*2)+command_substr_len+1);
        if (command==NULL) goto malloc_error_case_17;
        command_pos = stpcpy( command, "file -b -- '");
        command_pos = stpcpy( command_pos, fname);
        command_pos = stpcpy( command_pos, "';file -bi -- '");
        command_pos = stpcpy( command_pos, fname);
        command_pos = stpcpy( command_pos, "'");
        command_stream = popen(command,"r");
        fgets(command_output_1, CMD_OUT_LEN, command_stream);
        command_output_1[strlen(command_output_1)-1]='\0';
        fgets(command_output_2, CMD_OUT_LEN, command_stream);
        command_output_2[strlen(command_output_2)-1]='\0';
        widget_move (w, 17, 3);
        str_printf (buff, _("Mime Type:  %s (%s)"), command_output_1, command_output_2);
        tty_print_string (buff->str);
        g_string_set_size (buff, 0);
        pclose(command_stream);
        }
        malloc_error_case_17: /* Yes, I also hate gotos, but this is a fall-through switch */
    case 16:
        widget_move (w, 16, 3);
        if ((myfs_stats.nfree == 0 && myfs_stats.nodes == 0) ||
            (myfs_stats.nfree == (uintmax_t) (-1) && myfs_stats.nodes == (uintmax_t) (-1)))
            tty_print_string (_("No node information"));
        else if (myfs_stats.nfree == (uintmax_t) (-1))
            tty_printf ("%s -/%" PRIuMAX, _("Free nodes:"), myfs_stats.nodes);
        else if (myfs_stats.nodes == (uintmax_t) (-1))
            tty_printf ("%s %" PRIuMAX "/-", _("Free nodes:"), myfs_stats.nfree);
        else
            tty_printf ("%s %" PRIuMAX "/%" PRIuMAX " (%d%%)",
                        _("Free nodes:"),
                        myfs_stats.nfree, myfs_stats.nodes,
                        myfs_stats.nodes == 0 ? 0 :
                        (int) (100 * (long double) myfs_stats.nfree / myfs_stats.nodes));

    case 15:
        widget_move (w, 15, 3);
        if (myfs_stats.avail == 0 && myfs_stats.total == 0)
            tty_print_string (_("No space information"));
        else
        {
            char buffer1[6], buffer2[6];

            size_trunc_len (buffer1, 5, myfs_stats.avail, 1, panels_options.kilobyte_si);
            size_trunc_len (buffer2, 5, myfs_stats.total, 1, panels_options.kilobyte_si);
            tty_printf (_("Free space: %s/%s (%d%%)"), buffer1, buffer2,
                        myfs_stats.total == 0 ? 0 :
                        (int) (100 * (long double) myfs_stats.avail / myfs_stats.total));
        }

    case 14:
        widget_move (w, 14, 3);
        tty_printf (_("Type:       %s"),
                    myfs_stats.typename ? myfs_stats.typename : _("non-local vfs"));
        if (myfs_stats.type != 0xffff && myfs_stats.type != -1)
            tty_printf (" (%Xh)", myfs_stats.type);

    case 13:
        widget_move (w, 13, 3);
        str_printf (buff, _("Device:     %s"),
                    str_trunc (myfs_stats.device, w->cols - i18n_adjust));
        tty_print_string (buff->str);
        g_string_set_size (buff, 0);
    case 12:
        widget_move (w, 12, 3);
        str_printf (buff, _("Filesystem: %s"),
                    str_trunc (myfs_stats.mpoint, w->cols - i18n_adjust));
        tty_print_string (buff->str);
        g_string_set_size (buff, 0);
    case 11:
        widget_move (w, 11, 3);
        str_printf (buff, _("Accessed:   %s"), file_date (st.st_atime));
        tty_print_string (buff->str);
        g_string_set_size (buff, 0);
    case 10:
        widget_move (w, 10, 3);
        str_printf (buff, _("Modified:   %s"), file_date (st.st_mtime));
        tty_print_string (buff->str);
        g_string_set_size (buff, 0);
    case 9:
        widget_move (w, 9, 3);
        /* The field st_ctime is changed by writing or by setting inode
           information (i.e., owner, group, link count, mode, etc.).  */
        /* TRANSLATORS: Time of last status change as in stat(2) man. */
        str_printf (buff, _("Changed:    %s"), file_date (st.st_ctime));
        tty_print_string (buff->str);
        g_string_set_size (buff, 0);

    case 8:
        widget_move (w, 8, 3);
#ifdef HAVE_STRUCT_STAT_ST_RDEV
        if (S_ISCHR (st.st_mode) || S_ISBLK (st.st_mode))
            tty_printf (_("Dev. type: major %lu, minor %lu"),
                        (unsigned long) major (st.st_rdev), (unsigned long) minor (st.st_rdev));
        else
#endif
        {
            char buffer[10];
            size_trunc_len (buffer, 9, st.st_size, 0, panels_options.kilobyte_si);
            tty_printf (_("Size:       %s"), buffer);
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
            tty_printf (ngettext (" (%ld block)", " (%ld blocks)",
                                  (unsigned long) st.st_blocks), (unsigned long) st.st_blocks);
#endif
        }

    case 7:
        widget_move (w, 7, 3);
        tty_printf (_("Owner:      %s/%s"), get_owner (st.st_uid), get_group (st.st_gid));

    case 6:
        widget_move (w, 6, 3);
        tty_printf (_("Links:      %d"), (int) st.st_nlink);

    case 5:
        widget_move (w, 5, 3);
        tty_printf (_("Mode:       %s (%04o)"),
                    string_perm (st.st_mode), (unsigned) st.st_mode & 07777);

    case 4:
        widget_move (w, 4, 3);
        tty_printf (_("Location:   %Xh:%Xh"), (int) st.st_dev, (int) st.st_ino);

    case 3:
        widget_move (w, 3, 2);
        /* fname assignment possibly redundant now; see case 17, above */
        fname = current_panel->dir.list[current_panel->selected].fname;
        str_printf (buff, file_label, str_trunc (fname, w->cols - i18n_adjust));
        tty_print_string (buff->str);

    case 2:
    case 1:
    case 0:
        ;
    }                           /* switch */
    g_string_free (buff, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
info_hook (void *data)
{
    WInfo *info = (WInfo *) data;
    Widget *other_widget;

    other_widget = get_panel_widget (get_current_index ());
    if (!other_widget)
        return;
    if (widget_overlapped (WIDGET (info), other_widget))
        return;

    info->ready = TRUE;
    info_show_info (info);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
info_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WInfo *info = (WInfo *) w;

    switch (msg)
    {
    case MSG_INIT:
        init_my_statfs ();
        add_hook (&select_file_hook, info_hook, info);
        info->ready = FALSE;
        return MSG_HANDLED;

    case MSG_DRAW:
        info_hook (info);
        return MSG_HANDLED;

    case MSG_DESTROY:
        delete_hook (&select_file_hook, info_hook);
        free_my_statfs ();
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WInfo *
info_new (int y, int x, int lines, int cols)
{
    WInfo *info;
    Widget *w;

    info = g_new (struct WInfo, 1);
    w = WIDGET (info);
    widget_init (w, y, x, lines, cols, info_callback, NULL);

    return info;
}

/* --------------------------------------------------------------------------------------------- */
