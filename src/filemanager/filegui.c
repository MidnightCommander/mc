/*
   File management GUI for the text mode edition

   The copy code was based in GNU's cp, and was written by:
   Torbjorn Granlund, David MacKenzie, and Jim Meyering.

   The move code was based in GNU's mv, and was written by:
   Mike Parker and David MacKenzie.

   Janne Kukonlehto added much error recovery to them for being used
   in an interactive program.

   Copyright (C) 1994-2024
   Free Software Foundation, Inc.

   Written by:
   Janne Kukonlehto, 1994, 1995
   Fred Leeflang, 1994, 1995
   Miguel de Icaza, 1994, 1995, 1996
   Jakub Jelinek, 1995, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Slava Zanko, 2009, 2010, 2011, 2012, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2009-2023

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

/*
 * Please note that all dialogs used here must be safe for background
 * operations.
 */

/** \file  filegui.c
 *  \brief Source: file management GUI for the text mode edition
 */

/* {{{ Include files */

#include <config.h>

#if ((defined STAT_STATVFS || defined STAT_STATVFS64)                                       \
     && (defined HAVE_STRUCT_STATVFS_F_BASETYPE || defined HAVE_STRUCT_STATVFS_F_FSTYPENAME \
         || (! defined HAVE_STRUCT_STATFS_F_FSTYPENAME)))
#define USE_STATVFS 1
#else
#define USE_STATVFS 0
#endif

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#if USE_STATVFS
#include <sys/statvfs.h>
#elif defined HAVE_SYS_VFS_H
#include <sys/vfs.h>
#elif defined HAVE_SYS_MOUNT_H && defined HAVE_SYS_PARAM_H
/* NOTE: freebsd5.0 needs sys/param.h and sys/mount.h for statfs.
   It does have statvfs.h, but shouldn't use it, since it doesn't
   HAVE_STRUCT_STATVFS_F_BASETYPE.  So find a clean way to fix it.  */
/* NetBSD 1.5.2 needs these, for the declaration of struct statfs. */
#include <sys/param.h>
#include <sys/mount.h>
#elif defined HAVE_OS_H         /* Haiku, also (obsolete) BeOS */
#include <fs_info.h>
#endif

#if USE_STATVFS
#if ! defined STAT_STATVFS && defined STAT_STATVFS64
#define STRUCT_STATVFS struct statvfs64
#define STATFS statvfs64
#else
#define STRUCT_STATVFS struct statvfs
#define STATFS statvfs

#if defined __linux__ && (defined __GLIBC__ || defined __UCLIBC__)
#include <sys/utsname.h>
#include <sys/statfs.h>
#define STAT_STATFS2_BSIZE 1
#endif
#endif

#else
#define STATFS statfs
#define STRUCT_STATVFS struct statfs
#ifdef HAVE_OS_H                /* Haiku, also (obsolete) BeOS */
/* BeOS has a statvfs function, but it does not return sensible values
   for f_files, f_ffree and f_favail, and lacks f_type, f_basetype and
   f_fstypename.  Use 'struct fs_info' instead.  */
static int
statfs (char const *filename, struct fs_info *buf)
{
    dev_t device;

    device = dev_for_path (filename);

    if (device < 0)
    {
        errno = (device == B_ENTRY_NOT_FOUND ? ENOENT
                 : device == B_BAD_VALUE ? EINVAL
                 : device == B_NAME_TOO_LONG ? ENAMETOOLONG
                 : device == B_NO_MEMORY ? ENOMEM : device == B_FILE_ERROR ? EIO : 0);
        return -1;
    }
    /* If successful, buf->dev will be == device.  */
    return fs_stat_dev (device, buf);
}

#define STRUCT_STATVFS struct fs_info
#else
#define STRUCT_STATVFS struct statfs
#endif
#endif

#ifdef HAVE_STRUCT_STATVFS_F_BASETYPE
#define STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME f_basetype
#else
#if defined HAVE_STRUCT_STATVFS_F_FSTYPENAME || defined HAVE_STRUCT_STATFS_F_FSTYPENAME
#define STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME f_fstypename
#elif defined HAVE_OS_H         /* Haiku, also (obsolete) BeOS */
#define STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME fsh_name
#endif
#endif

#include <unistd.h>

#include "lib/global.h"

#include "lib/tty/key.h"        /* tty_get_event */
#include "lib/mcconfig.h"
#include "lib/search.h"
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/timefmt.h"        /* file_date() */
#include "lib/util.h"
#include "lib/widget.h"

#include "src/setup.h"          /* verbose, safe_overwrite */

#include "filemanager.h"
#include "fileopctx.h"          /* FILE_CONT */

#include "filegui.h"

/* }}} */

/*** global variables ****************************************************************************/

gboolean classic_progressbar = TRUE;

/*** file scope macro definitions ****************************************************************/

#define truncFileStringSecure(dlg, s) path_trunc (s, WIDGET (dlg)->rect.cols - 10)

/*** file scope type declarations ****************************************************************/

/* *INDENT-OFF* */
typedef enum {
    MSDOS_SUPER_MAGIC     = 0x4d44,
    NTFS_SB_MAGIC         = 0x5346544e,
    FUSE_MAGIC            = 0x65735546,
    PROC_SUPER_MAGIC      = 0x9fa0,
    SMB_SUPER_MAGIC       = 0x517B,
    NCP_SUPER_MAGIC       = 0x564c,
    USBDEVICE_SUPER_MAGIC = 0x9fa2
} filegui_nonattrs_fs_t;
/* *INDENT-ON* */

/* Used for button result values */
typedef enum
{
    REPLACE_YES = B_USER,
    REPLACE_NO,
    REPLACE_APPEND,
    REPLACE_REGET,
    REPLACE_ALL,
    REPLACE_OLDER,
    REPLACE_NONE,
    REPLACE_SMALLER,
    REPLACE_SIZE,
    REPLACE_ABORT
} replace_action_t;

/* This structure describes the UI and internal data required by a file
 * operation context.
 */
typedef struct
{
    /* ETA and bps */
    gboolean showing_eta;
    gboolean showing_bps;

    /* Dialog and widgets for the operation progress window */
    WDialog *op_dlg;
    /* Source file: label and name */
    WLabel *src_file_label;
    WLabel *src_file;
    /* Target file: label and name */
    WLabel *tgt_file_label;
    WLabel *tgt_file;

    WGauge *progress_file_gauge;
    WLabel *progress_file_label;

    WGauge *progress_total_gauge;

    WLabel *total_files_processed_label;
    WLabel *time_label;
    WHLine *total_bytes_label;

    /* Query replace dialog */
    WDialog *replace_dlg;
    const char *src_filename;
    const char *tgt_filename;
    replace_action_t replace_result;
    gboolean dont_overwrite_with_zero;

    struct stat *src_stat, *dst_stat;
} file_op_context_ui_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static struct
{
    Widget *w;
    FileProgressStatus action;
    const char *text;
    button_flags_t flags;
    int len;
} progress_buttons[] = {
    /* *INDENT-OFF* */
    { NULL, FILE_SKIP, N_("&Skip"), NORMAL_BUTTON, -1 },
    { NULL, FILE_SUSPEND, N_("S&uspend"), NORMAL_BUTTON, -1 },
    { NULL, FILE_SUSPEND, N_("Con&tinue"), NORMAL_BUTTON, -1 },
    { NULL, FILE_ABORT, N_("&Abort"), NORMAL_BUTTON, -1 }
    /* *INDENT-ON* */
};

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Return true if statvfs works.  This is false for statvfs on systems
   with GNU libc on Linux kernels before 2.6.36, which stats all
   preceding entries in /proc/mounts; that makes df hang if even one
   of the corresponding file systems is hard-mounted but not available.  */

#if USE_STATVFS && ! (! defined STAT_STATVFS && defined STAT_STATVFS64)
static int
statvfs_works (void)
{
#if ! (defined __linux__ && (defined __GLIBC__ || defined __UCLIBC__))
    return 1;
#else
    static int statvfs_works_cache = -1;
    struct utsname name;

    if (statvfs_works_cache < 0)
        statvfs_works_cache = (uname (&name) == 0 && 0 <= str_verscmp (name.release, "2.6.36"));
    return statvfs_works_cache;
#endif
}
#endif

/* --------------------------------------------------------------------------------------------- */

static gboolean
filegui__check_attrs_on_fs (const char *fs_path)
{
    STRUCT_STATVFS stfs;

#if USE_STATVFS && defined(STAT_STATVFS)
    if (statvfs_works () && statvfs (fs_path, &stfs) != 0)
        return TRUE;
#else
    if (STATFS (fs_path, &stfs) != 0)
        return TRUE;
#endif

#if (USE_STATVFS && defined(HAVE_STRUCT_STATVFS_F_TYPE)) || \
        (!USE_STATVFS && defined(HAVE_STRUCT_STATFS_F_TYPE))
    switch ((filegui_nonattrs_fs_t) stfs.f_type)
    {
    case MSDOS_SUPER_MAGIC:
    case NTFS_SB_MAGIC:
    case PROC_SUPER_MAGIC:
    case SMB_SUPER_MAGIC:
    case NCP_SUPER_MAGIC:
    case USBDEVICE_SUPER_MAGIC:
        return FALSE;
    default:
        break;
    }
#elif defined(HAVE_STRUCT_STATVFS_F_FSTYPENAME) || defined(HAVE_STRUCT_STATFS_F_FSTYPENAME)
    if (strcmp (stfs.STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME, "msdos") == 0
        || strcmp (stfs.STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME, "msdosfs") == 0
        || strcmp (stfs.STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME, "ntfs") == 0
        || strcmp (stfs.STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME, "procfs") == 0
        || strcmp (stfs.STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME, "smbfs") == 0
        || strstr (stfs.STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME, "fusefs") != NULL)
        return FALSE;
#elif defined(HAVE_STRUCT_STATVFS_F_BASETYPE)
    if (strcmp (stfs.STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME, "pcfs") == 0
        || strcmp (stfs.STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME, "ntfs") == 0
        || strcmp (stfs.STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME, "proc") == 0
        || strcmp (stfs.STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME, "smbfs") == 0
        || strcmp (stfs.STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME, "fuse") == 0)
        return FALSE;
#endif

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
file_frmt_time (char *buffer, double eta_secs)
{
    int eta_hours, eta_mins, eta_s;

    eta_hours = (int) (eta_secs / (60 * 60));
    eta_mins = (int) ((eta_secs - (eta_hours * 60 * 60)) / 60);
    eta_s = (int) (eta_secs - (eta_hours * 60 * 60 + eta_mins * 60));
    g_snprintf (buffer, BUF_TINY, _("%d:%02d:%02d"), eta_hours, eta_mins, eta_s);
}

/* --------------------------------------------------------------------------------------------- */

static void
file_eta_prepare_for_show (char *buffer, double eta_secs, gboolean always_show)
{
    char _fmt_buff[BUF_TINY];

    if (eta_secs <= 0.5 && !always_show)
    {
        *buffer = '\0';
        return;
    }

    if (eta_secs <= 0.5)
        eta_secs = 1;
    file_frmt_time (_fmt_buff, eta_secs);
    g_snprintf (buffer, BUF_TINY, _("ETA %s"), _fmt_buff);
}

/* --------------------------------------------------------------------------------------------- */

static void
file_bps_prepare_for_show (char *buffer, long bps)
{
    if (bps > 1024 * 1024)
        g_snprintf (buffer, BUF_TINY, _("%.2f MB/s"), bps / (1024 * 1024.0));
    else if (bps > 1024)
        g_snprintf (buffer, BUF_TINY, _("%.2f KB/s"), bps / 1024.0);
    else if (bps > 1)
        g_snprintf (buffer, BUF_TINY, _("%ld B/s"), bps);
    else
        *buffer = '\0';
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
file_ui_op_dlg_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_ACTION:
        /* Do not close the dialog because the query dialog will be shown */
        if (parm == CK_Cancel)
        {
            DIALOG (w)->ret_value = FILE_ABORT; /* for check_progress_buttons() */
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The dialog layout:
 *
 * +---------------------- File exists -----------------------+
 * | New     : /path/to/original_file_name                    |   // 0, 1
 * |                    1234567             feb  4 2017 13:38 |   // 2, 3
 * | Existing: /path/to/target_file_name                      |   // 4, 5
 * |                 1234567890             feb  4 2017 13:37 |   // 6, 7
 * +----------------------------------------------------------+
 * |                   Overwrite this file?                   |   // 8
 * |            [ Yes ] [ No ] [ Append ] [ Reget ]           |   // 9, 10, 11, 12
 * +----------------------------------------------------------+
 * |                   Overwrite all files?                   |   // 13
 * |  [ ] Don't overwrite with zero length file               |   // 14
 * |  [ All ] [ Older ] [None] [ Smaller ] [ Size differs ]   |   // 15, 16, 17, 18, 19
 * +----------------------------------------------------------|
 * |                         [ Abort ]                        |   // 20
 * +----------------------------------------------------------+
 */

static replace_action_t
overwrite_query_dialog (file_op_context_t *ctx, enum OperationMode mode)
{
#define W(i) dlg_widgets[i].widget
#define WX(i) W(i)->rect.x
#define WY(i) W(i)->rect.y
#define WCOLS(i) W(i)->rect.cols

#define NEW_LABEL(i, text) \
    W(i) = WIDGET (label_new (dlg_widgets[i].y, dlg_widgets[i].x, text))

#define ADD_LABEL(i) \
    group_add_widget_autopos (g, W(i), dlg_widgets[i].pos_flags, \
                              g->current != NULL ? g->current->data : NULL)

#define NEW_BUTTON(i) \
    W(i) = WIDGET (button_new (dlg_widgets[i].y, dlg_widgets[i].x, \
                               dlg_widgets[i].value, NORMAL_BUTTON, dlg_widgets[i].text, NULL))

#define ADD_BUTTON(i) \
    group_add_widget_autopos (g, W(i), dlg_widgets[i].pos_flags, g->current->data)

    /* dialog sizes */
    const int dlg_height = 17;
    int dlg_width = 60;

    struct
    {
        Widget *widget;
        const char *text;
        int y;
        int x;
        widget_pos_flags_t pos_flags;
        int value;              /* 0 for labels and checkbox */
    } dlg_widgets[] = {
        /* *INDENT-OFF* */
        /*  0 - label */
        { NULL, N_("New     :"), 2, 3, WPOS_KEEP_DEFAULT, 0 },
        /*  1 - label - name */
        { NULL, NULL, 2, 14, WPOS_KEEP_DEFAULT, 0 },
        /*  2 - label - size */
        { NULL, NULL, 3, 3, WPOS_KEEP_DEFAULT, 0 },
        /*  3 - label - date & time */
        { NULL, NULL, 3, 43, WPOS_KEEP_TOP | WPOS_KEEP_RIGHT, 0 },
        /*  4 - label */
        { NULL, N_("Existing:"), 4, 3, WPOS_KEEP_DEFAULT, 0 },
        /*  5 - label - name */
        { NULL, NULL, 4, 14, WPOS_KEEP_DEFAULT, 0 },
        /*  6 - label - size */
        { NULL, NULL, 5, 3, WPOS_KEEP_DEFAULT, 0 },
        /*  7 - label - date & time */
        { NULL, NULL, 5, 43, WPOS_KEEP_TOP | WPOS_KEEP_RIGHT, 0 },
        /* --------------------------------------------------- */
        /*  8 - label */
        { NULL, N_("Overwrite this file?"), 7, 21, WPOS_KEEP_TOP | WPOS_CENTER_HORZ, 0 },
        /*  9 - button */
        { NULL, N_("&Yes"), 8, 14, WPOS_KEEP_DEFAULT, REPLACE_YES },
        /* 10 - button */
        { NULL, N_("&No"), 8, 22, WPOS_KEEP_DEFAULT, REPLACE_NO },
        /* 11 - button */
        { NULL, N_("A&ppend"), 8, 29, WPOS_KEEP_DEFAULT, REPLACE_APPEND },
        /* 12 - button */
        { NULL, N_("&Reget"), 8, 40, WPOS_KEEP_DEFAULT, REPLACE_REGET },
        /* --------------------------------------------------- */
        /* 13 - label */
        { NULL, N_("Overwrite all files?"), 10, 21, WPOS_KEEP_TOP | WPOS_CENTER_HORZ, 0 },
        /* 14 - checkbox */
        { NULL, N_("Don't overwrite with &zero length file"), 11, 3, WPOS_KEEP_DEFAULT, 0 },
        /* 15 - button */
        { NULL, N_("A&ll"), 12, 12, WPOS_KEEP_DEFAULT, REPLACE_ALL },
        /* 16 - button */
        { NULL, N_("&Older"), 12, 12, WPOS_KEEP_DEFAULT, REPLACE_OLDER },
        /* 17 - button */
        { NULL, N_("Non&e"), 12, 12, WPOS_KEEP_DEFAULT, REPLACE_NONE },
        /* 18 - button */
        { NULL, N_("S&maller"), 12, 25, WPOS_KEEP_DEFAULT, REPLACE_SMALLER },
        /* 19 - button */
        { NULL, N_("&Size differs"), 12, 40, WPOS_KEEP_DEFAULT, REPLACE_SIZE },
        /* --------------------------------------------------- */
        /* 20 - button */
        { NULL, N_("&Abort"), 14, 27, WPOS_KEEP_TOP | WPOS_CENTER_HORZ, REPLACE_ABORT }
        /* *INDENT-ON* */
    };

    const int gap = 1;

    file_op_context_ui_t *ui = ctx->ui;
    Widget *wd;
    WGroup *g;
    const char *title;

    vfs_path_t *p;
    char *s1;
    const char *cs1;
    char s2[BUF_SMALL];
    int w, bw1, bw2;
    unsigned short i;

    gboolean do_append = FALSE, do_reget = FALSE;
    unsigned long yes_id, no_id;
    int result;

    if (mode == Foreground)
        title = _("File exists");
    else
        title = _("Background process: File exists");

#ifdef ENABLE_NLS
    {
        const unsigned short num = G_N_ELEMENTS (dlg_widgets);

        for (i = 0; i < num; i++)
            if (dlg_widgets[i].text != NULL)
                dlg_widgets[i].text = _(dlg_widgets[i].text);
    }
#endif /* ENABLE_NLS */

    /* create widgets to get their real widths */
    /* new file */
    NEW_LABEL (0, dlg_widgets[0].text);
    /* new file name */
    p = vfs_path_from_str (ui->src_filename);
    s1 = vfs_path_to_str_flags (p, 0, VPF_STRIP_HOME | VPF_STRIP_PASSWORD);
    NEW_LABEL (1, s1);
    vfs_path_free (p, TRUE);
    g_free (s1);
    /* new file size */
    size_trunc_len (s2, sizeof (s2), ui->src_stat->st_size, 0, panels_options.kilobyte_si);
    NEW_LABEL (2, s2);
    /* new file modification date & time */
    cs1 = file_date (ui->src_stat->st_mtime);
    NEW_LABEL (3, cs1);

    /* existing file */
    NEW_LABEL (4, dlg_widgets[4].text);
    /* existing file name */
    p = vfs_path_from_str (ui->tgt_filename);
    s1 = vfs_path_to_str_flags (p, 0, VPF_STRIP_HOME | VPF_STRIP_PASSWORD);
    NEW_LABEL (5, s1);
    vfs_path_free (p, TRUE);
    g_free (s1);
    /* existing file size */
    size_trunc_len (s2, sizeof (s2), ui->dst_stat->st_size, 0, panels_options.kilobyte_si);
    NEW_LABEL (6, s2);
    /* existing file modification date & time */
    cs1 = file_date (ui->dst_stat->st_mtime);
    NEW_LABEL (7, cs1);

    /* will "Append" and "Reget" buttons be in the dialog? */
    do_append = !S_ISDIR (ui->dst_stat->st_mode);
    do_reget = do_append && ctx->operation == OP_COPY && ui->dst_stat->st_size != 0
        && ui->src_stat->st_size > ui->dst_stat->st_size;

    NEW_LABEL (8, dlg_widgets[8].text);
    NEW_BUTTON (9);
    NEW_BUTTON (10);
    if (do_append)
        NEW_BUTTON (11);
    if (do_reget)
        NEW_BUTTON (12);

    NEW_LABEL (13, dlg_widgets[13].text);
    dlg_widgets[14].widget =
        WIDGET (check_new (dlg_widgets[14].y, dlg_widgets[14].x, FALSE, dlg_widgets[14].text));
    for (i = 15; i <= 20; i++)
        NEW_BUTTON (i);

    /* place widgets */
    dlg_width -= 2 * (2 + gap); /* inside frame */

    /* perhaps longest line is buttons */
    bw1 = WCOLS (9) + gap + WCOLS (10);
    if (do_append)
        bw1 += gap + WCOLS (11);
    if (do_reget)
        bw1 += gap + WCOLS (12);
    dlg_width = MAX (dlg_width, bw1);

    bw2 = WCOLS (15);
    for (i = 16; i <= 19; i++)
        bw2 += gap + WCOLS (i);
    dlg_width = MAX (dlg_width, bw2);

    dlg_width = MAX (dlg_width, WCOLS (8));
    dlg_width = MAX (dlg_width, WCOLS (13));
    dlg_width = MAX (dlg_width, WCOLS (14));

    /* truncate file names */
    w = WCOLS (0) + gap + WCOLS (1);
    if (w > dlg_width)
    {
        WLabel *l = LABEL (W (1));

        w = dlg_width - gap - WCOLS (0);
        label_set_text (l, str_trunc (l->text, w));
    }

    w = WCOLS (4) + gap + WCOLS (5);
    if (w > dlg_width)
    {
        WLabel *l = LABEL (W (5));

        w = dlg_width - gap - WCOLS (4);
        label_set_text (l, str_trunc (l->text, w));
    }

    /* real dlalog width */
    dlg_width += 2 * (2 + gap);

    WX (1) = WX (0) + WCOLS (0) + gap;
    WX (5) = WX (4) + WCOLS (4) + gap;

    /* sizes: right alignment */
    WX (2) = dlg_width / 2 - WCOLS (2);
    WX (6) = dlg_width / 2 - WCOLS (6);

    w = dlg_width - (2 + gap);  /* right bound */

    /* date & time */
    WX (3) = w - WCOLS (3);
    WX (7) = w - WCOLS (7);

    /* buttons: center alignment */
    WX (9) = dlg_width / 2 - bw1 / 2;
    WX (10) = WX (9) + WCOLS (9) + gap;
    if (do_append)
        WX (11) = WX (10) + WCOLS (10) + gap;
    if (do_reget)
        WX (12) = WX (11) + WCOLS (11) + gap;

    WX (15) = dlg_width / 2 - bw2 / 2;
    for (i = 16; i <= 19; i++)
        WX (i) = WX (i - 1) + WCOLS (i - 1) + gap;

    /* TODO: write help (ticket #3970) */
    ui->replace_dlg =
        dlg_create (TRUE, 0, 0, dlg_height, dlg_width, WPOS_CENTER, FALSE, alarm_colors, NULL, NULL,
                    "[Replace]", title);
    wd = WIDGET (ui->replace_dlg);
    g = GROUP (ui->replace_dlg);

    /* file info */
    for (i = 0; i <= 7; i++)
        ADD_LABEL (i);
    group_add_widget (g, hline_new (WY (7) - wd->rect.y + 1, -1, -1));

    /* label & buttons */
    ADD_LABEL (8);              /* Overwrite this file? */
    yes_id = ADD_BUTTON (9);    /* Yes */
    no_id = ADD_BUTTON (10);    /* No */
    if (do_append)
        ADD_BUTTON (11);        /* Append */
    if (do_reget)
        ADD_BUTTON (12);        /* Reget */
    group_add_widget (g, hline_new (WY (10) - wd->rect.y + 1, -1, -1));

    /* label & buttons */
    ADD_LABEL (13);             /* Overwrite all files? */
    group_add_widget (g, dlg_widgets[14].widget);
    for (i = 15; i <= 19; i++)
        ADD_BUTTON (i);
    group_add_widget (g, hline_new (WY (19) - wd->rect.y + 1, -1, -1));

    ADD_BUTTON (20);            /* Abort */

    group_select_widget_by_id (g, safe_overwrite ? no_id : yes_id);

    result = dlg_run (ui->replace_dlg);

    if (result != B_CANCEL)
        ui->dont_overwrite_with_zero = CHECK (dlg_widgets[14].widget)->state;

    widget_destroy (wd);

    return (result == B_CANCEL) ? REPLACE_ABORT : (replace_action_t) result;

#undef ADD_BUTTON
#undef NEW_BUTTON
#undef ADD_LABEL
#undef NEW_LABEL
#undef WCOLS
#undef WX
#undef W
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
is_wildcarded (const char *p)
{
    gboolean escaped = FALSE;

    for (; *p != '\0'; p++)
    {
        if (*p == '\\')
        {
            if (p[1] >= '1' && p[1] <= '9' && !escaped)
                return TRUE;
            escaped = !escaped;
        }
        else
        {
            if ((*p == '*' || *p == '?') && !escaped)
                return TRUE;
            escaped = FALSE;
        }
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
place_progress_buttons (WDialog *h, gboolean suspended)
{
    const size_t i = suspended ? 2 : 1;
    Widget *w = WIDGET (h);
    int buttons_width;

    buttons_width = 2 + progress_buttons[0].len + progress_buttons[3].len;
    buttons_width += progress_buttons[i].len;
    button_set_text (BUTTON (progress_buttons[i].w), progress_buttons[i].text);

    progress_buttons[0].w->rect.x = w->rect.x + (w->rect.cols - buttons_width) / 2;
    progress_buttons[i].w->rect.x = progress_buttons[0].w->rect.x + progress_buttons[0].len + 1;
    progress_buttons[3].w->rect.x = progress_buttons[i].w->rect.x + progress_buttons[i].len + 1;
}

/* --------------------------------------------------------------------------------------------- */

static int
progress_button_callback (WButton *button, int action)
{
    (void) button;
    (void) action;

    /* don't close dialog in any case */
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

FileProgressStatus
check_progress_buttons (file_op_context_t *ctx)
{
    int c;
    Gpm_Event event;
    file_op_context_ui_t *ui;

    if (ctx == NULL || ctx->ui == NULL)
        return FILE_CONT;

    ui = ctx->ui;

  get_event:
    event.x = -1;               /* Don't show the GPM cursor */
    c = tty_get_event (&event, FALSE, ctx->suspended);
    if (c == EV_NONE)
        return FILE_CONT;

    /* Reinitialize to avoid old values after events other than selecting a button */
    ui->op_dlg->ret_value = FILE_CONT;

    dlg_process_event (ui->op_dlg, c, &event);
    switch (ui->op_dlg->ret_value)
    {
    case FILE_SKIP:
        if (ctx->suspended)
        {
            /* redraw dialog in case of Skip after Suspend */
            place_progress_buttons (ui->op_dlg, FALSE);
            widget_draw (WIDGET (ui->op_dlg));
        }
        ctx->suspended = FALSE;
        return FILE_SKIP;
    case B_CANCEL:
    case FILE_ABORT:
        ctx->suspended = FALSE;
        return FILE_ABORT;
    case FILE_SUSPEND:
        ctx->suspended = !ctx->suspended;
        place_progress_buttons (ui->op_dlg, ctx->suspended);
        widget_draw (WIDGET (ui->op_dlg));
        MC_FALLTHROUGH;
    default:
        if (ctx->suspended)
            goto get_event;
        return FILE_CONT;
    }
}

/* --------------------------------------------------------------------------------------------- */
/* {{{ File progress display routines */

void
file_op_context_create_ui (file_op_context_t *ctx, gboolean with_eta,
                           filegui_dialog_type_t dialog_type)
{
    file_op_context_ui_t *ui;
    Widget *w;
    WGroup *g;
    int buttons_width;
    int dlg_width = 58, dlg_height = 17;
    int y = 2, x = 3;
    WRect r;

    if (ctx == NULL || ctx->ui != NULL)
        return;

#ifdef ENABLE_NLS
    if (progress_buttons[0].len == -1)
    {
        size_t i;

        for (i = 0; i < G_N_ELEMENTS (progress_buttons); i++)
            progress_buttons[i].text = _(progress_buttons[i].text);
    }
#endif

    ctx->dialog_type = dialog_type;
    ctx->recursive_result = RECURSIVE_YES;
    ctx->ui = g_new0 (file_op_context_ui_t, 1);

    ui = ctx->ui;
    ui->replace_result = REPLACE_YES;

    ui->op_dlg = dlg_create (TRUE, 0, 0, dlg_height, dlg_width, WPOS_CENTER, FALSE, dialog_colors,
                             file_ui_op_dlg_callback, NULL, NULL, op_names[ctx->operation]);
    w = WIDGET (ui->op_dlg);
    g = GROUP (ui->op_dlg);

    if (dialog_type != FILEGUI_DIALOG_DELETE_ITEM)
    {
        ui->showing_eta = with_eta && ctx->progress_totals_computed;
        ui->showing_bps = with_eta;

        ui->src_file_label = label_new (y++, x, NULL);
        group_add_widget (g, ui->src_file_label);

        ui->src_file = label_new (y++, x, NULL);
        group_add_widget (g, ui->src_file);

        ui->tgt_file_label = label_new (y++, x, NULL);
        group_add_widget (g, ui->tgt_file_label);

        ui->tgt_file = label_new (y++, x, NULL);
        group_add_widget (g, ui->tgt_file);

        ui->progress_file_gauge = gauge_new (y++, x + 3, dlg_width - (x + 3) * 2, FALSE, 100, 0);
        if (!classic_progressbar && (current_panel == right_panel))
            ui->progress_file_gauge->from_left_to_right = FALSE;
        group_add_widget_autopos (g, ui->progress_file_gauge, WPOS_KEEP_TOP | WPOS_KEEP_HORZ, NULL);

        ui->progress_file_label = label_new (y++, x, NULL);
        group_add_widget (g, ui->progress_file_label);

        if (verbose && dialog_type == FILEGUI_DIALOG_MULTI_ITEM)
        {
            ui->total_bytes_label = hline_new (y++, -1, -1);
            group_add_widget (g, ui->total_bytes_label);

            if (ctx->progress_totals_computed)
            {
                ui->progress_total_gauge =
                    gauge_new (y++, x + 3, dlg_width - (x + 3) * 2, FALSE, 100, 0);
                if (!classic_progressbar && (current_panel == right_panel))
                    ui->progress_total_gauge->from_left_to_right = FALSE;
                group_add_widget_autopos (g, ui->progress_total_gauge,
                                          WPOS_KEEP_TOP | WPOS_KEEP_HORZ, NULL);
            }

            ui->total_files_processed_label = label_new (y++, x, NULL);
            group_add_widget (g, ui->total_files_processed_label);

            ui->time_label = label_new (y++, x, NULL);
            group_add_widget (g, ui->time_label);
        }
    }
    else
    {
        ui->src_file = label_new (y++, x, NULL);
        group_add_widget (g, ui->src_file);

        ui->total_files_processed_label = label_new (y++, x, NULL);
        group_add_widget (g, ui->total_files_processed_label);
    }

    group_add_widget (g, hline_new (y++, -1, -1));

    progress_buttons[0].w = WIDGET (button_new (y, 0, progress_buttons[0].action,
                                                progress_buttons[0].flags, progress_buttons[0].text,
                                                progress_button_callback));
    if (progress_buttons[0].len == -1)
        progress_buttons[0].len = button_get_len (BUTTON (progress_buttons[0].w));

    progress_buttons[1].w = WIDGET (button_new (y, 0, progress_buttons[1].action,
                                                progress_buttons[1].flags, progress_buttons[1].text,
                                                progress_button_callback));
    if (progress_buttons[1].len == -1)
        progress_buttons[1].len = button_get_len (BUTTON (progress_buttons[1].w));

    if (progress_buttons[2].len == -1)
    {
        /* create and destroy button to get it length */
        progress_buttons[2].w = WIDGET (button_new (y, 0, progress_buttons[2].action,
                                                    progress_buttons[2].flags,
                                                    progress_buttons[2].text,
                                                    progress_button_callback));
        progress_buttons[2].len = button_get_len (BUTTON (progress_buttons[2].w));
        widget_destroy (progress_buttons[2].w);
    }
    progress_buttons[2].w = progress_buttons[1].w;

    progress_buttons[3].w = WIDGET (button_new (y, 0, progress_buttons[3].action,
                                                progress_buttons[3].flags, progress_buttons[3].text,
                                                NULL));
    if (progress_buttons[3].len == -1)
        progress_buttons[3].len = button_get_len (BUTTON (progress_buttons[3].w));

    group_add_widget (g, progress_buttons[0].w);
    group_add_widget (g, progress_buttons[1].w);
    group_add_widget (g, progress_buttons[3].w);

    buttons_width = 2 +
        progress_buttons[0].len + MAX (progress_buttons[1].len, progress_buttons[2].len) +
        progress_buttons[3].len;

    /* adjust dialog sizes  */
    r = w->rect;
    r.lines = y + 3;
    r.cols = MAX (COLS * 2 / 3, buttons_width + 6);
    widget_set_size_rect (w, &r);

    place_progress_buttons (ui->op_dlg, FALSE);

    widget_select (progress_buttons[0].w);

    /* We will manage the dialog without any help, that's why
       we have to call dlg_init */
    dlg_init (ui->op_dlg);
}

/* --------------------------------------------------------------------------------------------- */

void
file_op_context_destroy_ui (file_op_context_t *ctx)
{
    if (ctx != NULL && ctx->ui != NULL)
    {
        file_op_context_ui_t *ui = (file_op_context_ui_t *) ctx->ui;

        dlg_run_done (ui->op_dlg);
        widget_destroy (WIDGET (ui->op_dlg));
        MC_PTR_FREE (ctx->ui);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
   show progressbar for file
 */

void
file_progress_show (file_op_context_t *ctx, off_t done, off_t total,
                    const char *stalled_msg, gboolean force_update)
{
    file_op_context_ui_t *ui;

    if (!verbose || ctx == NULL || ctx->ui == NULL)
        return;

    ui = ctx->ui;

    if (total == 0)
    {
        gauge_show (ui->progress_file_gauge, FALSE);
        return;
    }

    gauge_set_value (ui->progress_file_gauge, 1024, (int) (1024 * done / total));
    gauge_show (ui->progress_file_gauge, TRUE);

    if (!force_update)
        return;

    if (!ui->showing_eta || ctx->eta_secs <= 0.5)
        label_set_text (ui->progress_file_label, stalled_msg);
    else
    {
        char buffer2[BUF_TINY];

        file_eta_prepare_for_show (buffer2, ctx->eta_secs, FALSE);
        if (ctx->bps == 0)
            label_set_textv (ui->progress_file_label, "%s %s", buffer2, stalled_msg);
        else
        {
            char buffer3[BUF_TINY];

            file_bps_prepare_for_show (buffer3, ctx->bps);
            label_set_textv (ui->progress_file_label, "%s (%s) %s", buffer2, buffer3, stalled_msg);
        }

    }
}

/* --------------------------------------------------------------------------------------------- */

void
file_progress_show_count (file_op_context_t *ctx, size_t done, size_t total)
{
    file_op_context_ui_t *ui;

    if (ctx == NULL || ctx->ui == NULL)
        return;

    ui = ctx->ui;

    if (ui->total_files_processed_label == NULL)
        return;

    if (ctx->progress_totals_computed)
        label_set_textv (ui->total_files_processed_label, _("Files processed: %zu / %zu"), done,
                         total);
    else
        label_set_textv (ui->total_files_processed_label, _("Files processed: %zu"), done);
}

/* --------------------------------------------------------------------------------------------- */

void
file_progress_show_total (file_op_total_context_t *tctx, file_op_context_t *ctx,
                          uintmax_t copied_bytes, gboolean show_summary)
{
    char buffer2[BUF_TINY];
    char buffer3[BUF_TINY];
    file_op_context_ui_t *ui;

    if (ctx == NULL || ctx->ui == NULL)
        return;

    ui = ctx->ui;

    if (ui->progress_total_gauge != NULL)
    {
        if (ctx->progress_bytes == 0)
            gauge_show (ui->progress_total_gauge, FALSE);
        else
        {
            gauge_set_value (ui->progress_total_gauge, 1024,
                             (int) (1024 * copied_bytes / ctx->progress_bytes));
            gauge_show (ui->progress_total_gauge, TRUE);
        }
    }

    if (!show_summary && tctx->bps == 0)
        return;

    if (ui->time_label != NULL)
    {
        gint64 tv_current;
        char buffer4[BUF_TINY];

        tv_current = g_get_monotonic_time ();
        file_frmt_time (buffer2, (tv_current - tctx->transfer_start) / G_USEC_PER_SEC);

        if (ctx->progress_totals_computed)
        {
            file_eta_prepare_for_show (buffer3, tctx->eta_secs, TRUE);
            if (tctx->bps == 0)
                label_set_textv (ui->time_label, _("Time: %s %s"), buffer2, buffer3);
            else
            {
                file_bps_prepare_for_show (buffer4, (long) tctx->bps);
                label_set_textv (ui->time_label, _("Time: %s %s (%s)"), buffer2, buffer3, buffer4);
            }
        }
        else
        {
            if (tctx->bps == 0)
                label_set_textv (ui->time_label, _("Time: %s"), buffer2);
            else
            {
                file_bps_prepare_for_show (buffer4, (long) tctx->bps);
                label_set_textv (ui->time_label, _("Time: %s (%s)"), buffer2, buffer4);
            }
        }
    }

    if (ui->total_bytes_label != NULL)
    {
        size_trunc_len (buffer2, 5, tctx->copied_bytes, 0, panels_options.kilobyte_si);

        if (!ctx->progress_totals_computed)
            hline_set_textv (ui->total_bytes_label, _(" Total: %s "), buffer2);
        else
        {
            size_trunc_len (buffer3, 5, ctx->progress_bytes, 0, panels_options.kilobyte_si);
            hline_set_textv (ui->total_bytes_label, _(" Total: %s / %s "), buffer2, buffer3);
        }
    }
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */

void
file_progress_show_source (file_op_context_t *ctx, const vfs_path_t *vpath)
{
    file_op_context_ui_t *ui;

    if (ctx == NULL || ctx->ui == NULL)
        return;

    ui = ctx->ui;

    if (vpath != NULL)
    {
        label_set_text (ui->src_file_label, _("Source"));
        label_set_text (ui->src_file, truncFileStringSecure (ui->op_dlg, vfs_path_as_str (vpath)));
    }
    else
    {
        label_set_text (ui->src_file_label, NULL);
        label_set_text (ui->src_file, NULL);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
file_progress_show_target (file_op_context_t *ctx, const vfs_path_t *vpath)
{
    file_op_context_ui_t *ui;

    if (ctx == NULL || ctx->ui == NULL)
        return;

    ui = ctx->ui;

    if (vpath != NULL)
    {
        label_set_text (ui->tgt_file_label, _("Target"));
        label_set_text (ui->tgt_file, truncFileStringSecure (ui->op_dlg, vfs_path_as_str (vpath)));
    }
    else
    {
        label_set_text (ui->tgt_file_label, NULL);
        label_set_text (ui->tgt_file, NULL);
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
file_progress_show_deleting (file_op_context_t *ctx, const char *s, size_t *count)
{
    static gint64 timestamp = 0;
    /* update with 25 FPS rate */
    static const gint64 delay = G_USEC_PER_SEC / 25;

    gboolean ret;

    if (ctx == NULL || ctx->ui == NULL)
        return FALSE;

    ret = mc_time_elapsed (&timestamp, delay);

    if (ret)
    {
        file_op_context_ui_t *ui;

        ui = ctx->ui;

        if (ui->src_file_label != NULL)
            label_set_text (ui->src_file_label, _("Deleting"));

        label_set_text (ui->src_file, truncFileStringSecure (ui->op_dlg, s));
    }

    if (count != NULL)
        (*count)++;

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

FileProgressStatus
file_progress_real_query_replace (file_op_context_t *ctx, enum OperationMode mode,
                                  const char *src, struct stat *src_stat,
                                  const char *dst, struct stat *dst_stat)
{
    file_op_context_ui_t *ui;
    FileProgressStatus replace_with_zero;

    if (ctx == NULL || ctx->ui == NULL)
        return FILE_CONT;

    ui = ctx->ui;

    if (ui->replace_result == REPLACE_YES || ui->replace_result == REPLACE_NO
        || ui->replace_result == REPLACE_APPEND)
    {
        ui->src_filename = src;
        ui->src_stat = src_stat;
        ui->tgt_filename = dst;
        ui->dst_stat = dst_stat;
        ui->replace_result = overwrite_query_dialog (ctx, mode);
    }

    replace_with_zero = (src_stat->st_size == 0
                         && ui->dont_overwrite_with_zero) ? FILE_SKIP : FILE_CONT;

    switch (ui->replace_result)
    {
    case REPLACE_OLDER:
        do_refresh ();
        if (src_stat->st_mtime > dst_stat->st_mtime)
            return replace_with_zero;
        else
            return FILE_SKIP;

    case REPLACE_SIZE:
        do_refresh ();
        if (src_stat->st_size == dst_stat->st_size)
            return FILE_SKIP;
        else
            return replace_with_zero;

    case REPLACE_SMALLER:
        do_refresh ();
        if (src_stat->st_size > dst_stat->st_size)
            return FILE_CONT;
        else
            return FILE_SKIP;

    case REPLACE_ALL:
        do_refresh ();
        return replace_with_zero;

    case REPLACE_REGET:
        /* Careful: we fall through and set do_append */
        ctx->do_reget = dst_stat->st_size;
        MC_FALLTHROUGH;

    case REPLACE_APPEND:
        ctx->do_append = TRUE;
        MC_FALLTHROUGH;

    case REPLACE_YES:
        do_refresh ();
        return FILE_CONT;

    case REPLACE_NO:
    case REPLACE_NONE:
        do_refresh ();
        return FILE_SKIP;

    case REPLACE_ABORT:
    default:
        return FILE_ABORT;
    }
}

/* --------------------------------------------------------------------------------------------- */

char *
file_mask_dialog (file_op_context_t *ctx, gboolean only_one, const char *format, const void *text,
                  const char *def_text, gboolean *do_bg)
{
    gboolean preserve;
    size_t fmd_xlen;
    vfs_path_t *vpath;
    gboolean source_easy_patterns = easy_patterns;
    char fmd_buf[BUF_MEDIUM];
    char *dest_dir = NULL;
    char *tmp;
    char *def_text_secure;

    if (ctx == NULL)
        return NULL;

    /* unselect checkbox if target filesystem doesn't support attributes */
    preserve = copymove_persistent_attr && filegui__check_attrs_on_fs (def_text);

    ctx->stable_symlinks = FALSE;
    *do_bg = FALSE;

    /* filter out a possible password from def_text */
    vpath = vfs_path_from_str_flags (def_text, only_one ? VPF_NO_CANON : VPF_NONE);
    tmp = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_PASSWORD);
    vfs_path_free (vpath, TRUE);

    if (source_easy_patterns)
        def_text_secure = str_glob_escape (tmp);
    else
        def_text_secure = str_regex_escape (tmp);
    g_free (tmp);

    if (only_one)
    {
        int format_len, text_len;
        int max_len;

        format_len = str_term_width1 (format);
        text_len = str_term_width1 (text);
        max_len = COLS - 2 - 6;

        if (format_len + text_len <= max_len)
        {
            fmd_xlen = format_len + text_len + 6;
            fmd_xlen = MAX (fmd_xlen, 68);
        }
        else
        {
            text = str_trunc ((const char *) text, max_len - format_len);
            fmd_xlen = max_len + 6;
        }

        g_snprintf (fmd_buf, sizeof (fmd_buf), format, (const char *) text);
    }
    else
    {
        fmd_xlen = COLS * 2 / 3;
        fmd_xlen = MAX (fmd_xlen, 68);
        g_snprintf (fmd_buf, sizeof (fmd_buf), format, *(const int *) text);
    }

    {
        char *source_mask = NULL;
        char *orig_mask;
        int val;
        struct stat buf;

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABELED_INPUT (fmd_buf, input_label_above, easy_patterns ? "*" : "^(.*)$",
                                 "input-def", &source_mask, NULL, FALSE, FALSE,
                                 INPUT_COMPLETE_FILENAMES),
            QUICK_START_COLUMNS,
                QUICK_SEPARATOR (FALSE),
            QUICK_NEXT_COLUMN,
                QUICK_CHECKBOX (N_("&Using shell patterns"), &source_easy_patterns, NULL),
            QUICK_STOP_COLUMNS,
            QUICK_LABELED_INPUT (N_("to:"), input_label_above, def_text_secure, "input2", &dest_dir,
                                 NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
            QUICK_SEPARATOR (TRUE),
            QUICK_START_COLUMNS,
                QUICK_CHECKBOX (N_("Follow &links"), &ctx->follow_links, NULL),
                QUICK_CHECKBOX (N_("Preserve &attributes"), &preserve, NULL),
            QUICK_NEXT_COLUMN,
                QUICK_CHECKBOX (N_("Di&ve into subdir if exists"), &ctx->dive_into_subdirs, NULL),
                QUICK_CHECKBOX (N_("&Stable symlinks"), &ctx->stable_symlinks, NULL),
            QUICK_STOP_COLUMNS,
            QUICK_START_BUTTONS (TRUE, TRUE),
                QUICK_BUTTON (N_("&OK"), B_ENTER, NULL, NULL),
#ifdef ENABLE_BACKGROUND
                QUICK_BUTTON (N_("&Background"), B_USER, NULL, NULL),
#endif /* ENABLE_BACKGROUND */
                QUICK_BUTTON (N_("&Cancel"), B_CANCEL, NULL, NULL),
            QUICK_END
            /* *INDENT-ON* */
        };

        WRect r = { -1, -1, 0, fmd_xlen };

        quick_dialog_t qdlg = {
            r, op_names[ctx->operation], "[Mask Copy/Rename]",
            quick_widgets, NULL, NULL
        };

        while (TRUE)
        {
            val = quick_dialog_skip (&qdlg, 4);

            if (val == B_CANCEL)
            {
                g_free (def_text_secure);
                return NULL;
            }

            ctx->stat_func = ctx->follow_links ? mc_stat : mc_lstat;

            if (preserve)
            {
                ctx->preserve = TRUE;
                ctx->umask_kill = (mode_t) (~0);
                ctx->preserve_uidgid = (geteuid () == 0);
            }
            else
            {
                mode_t i2;

                ctx->preserve = ctx->preserve_uidgid = FALSE;
                i2 = umask (0);
                umask (i2);
                ctx->umask_kill = i2 ^ ((mode_t) (~0));
            }

            if (*dest_dir == '\0')
            {
                g_free (def_text_secure);
                g_free (source_mask);
                g_free (dest_dir);
                return NULL;
            }

            ctx->search_handle = mc_search_new (source_mask, NULL);
            if (ctx->search_handle != NULL)
                break;

            message (D_ERROR, MSG_ERROR, _("Invalid source pattern '%s'"), source_mask);
            MC_PTR_FREE (dest_dir);
            MC_PTR_FREE (source_mask);
        }

        g_free (def_text_secure);
        g_free (source_mask);

        ctx->search_handle->is_case_sensitive = TRUE;
        if (source_easy_patterns)
            ctx->search_handle->search_type = MC_SEARCH_T_GLOB;
        else
            ctx->search_handle->search_type = MC_SEARCH_T_REGEX;

        tmp = dest_dir;
        dest_dir = tilde_expand (tmp);
        g_free (tmp);
        vpath = vfs_path_from_str (dest_dir);

        ctx->dest_mask = strrchr (dest_dir, PATH_SEP);
        if (ctx->dest_mask == NULL)
            ctx->dest_mask = dest_dir;
        else
            ctx->dest_mask++;

        orig_mask = ctx->dest_mask;

        if (*ctx->dest_mask == '\0'
            || (!ctx->dive_into_subdirs && !is_wildcarded (ctx->dest_mask)
                && (!only_one
                    || (mc_stat (vpath, &buf) == 0 && S_ISDIR (buf.st_mode))))
            || (ctx->dive_into_subdirs
                && ((!only_one && !is_wildcarded (ctx->dest_mask))
                    || (only_one && mc_stat (vpath, &buf) == 0 && S_ISDIR (buf.st_mode)))))
            ctx->dest_mask = g_strdup ("\\0");
        else
        {
            ctx->dest_mask = g_strdup (ctx->dest_mask);
            *orig_mask = '\0';
        }

        if (*dest_dir == '\0')
        {
            g_free (dest_dir);
            dest_dir = g_strdup ("./");
        }

        vfs_path_free (vpath, TRUE);

        if (val == B_USER)
            *do_bg = TRUE;
    }

    return dest_dir;
}

/* --------------------------------------------------------------------------------------------- */
