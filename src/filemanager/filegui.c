/*
   File management GUI for the text mode edition

   The copy code was based in GNU's cp, and was written by:
   Torbjorn Granlund, David MacKenzie, and Jim Meyering.

   The move code was based in GNU's mv, and was written by:
   Mike Parker and David MacKenzie.

   Janne Kukonlehto added much error recovery to them for being used
   in an interactive program.

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009, 2011, 2012, 2013
   The Free Software Foundation, Inc.

   Written by:
   Janne Kukonlehto, 1994, 1995
   Fred Leeflang, 1994, 1995
   Miguel de Icaza, 1994, 1995, 1996
   Jakub Jelinek, 1995, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Slava Zanko, 2009-2012
   Andrew Borodin <aborodin@vmail.ru>, 2009, 2010, 2011, 2012, 2013

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

/* Keep this conditional in sync with the similar conditional in m4.include/mc-get-fs-info. */
#if ((STAT_STATVFS || STAT_STATVFS64)                                       \
     && (HAVE_STRUCT_STATVFS_F_BASETYPE || HAVE_STRUCT_STATVFS_F_FSTYPENAME \
         || (! HAVE_STRUCT_STATFS_F_FSTYPENAME)))
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
#elif HAVE_SYS_VFS_H
#include <sys/vfs.h>
#elif HAVE_SYS_MOUNT_H && HAVE_SYS_PARAM_H
/* NOTE: freebsd5.0 needs sys/param.h and sys/mount.h for statfs.
   It does have statvfs.h, but shouldn't use it, since it doesn't
   HAVE_STRUCT_STATVFS_F_BASETYPE.  So find a clean way to fix it.  */
/* NetBSD 1.5.2 needs these, for the declaration of struct statfs. */
#include <sys/param.h>
#include <sys/mount.h>
#if HAVE_NFS_NFS_CLNT_H && HAVE_NFS_VFS_H
/* Ultrix 4.4 needs these for the declaration of struct statfs.  */
#include <netinet/in.h>
#include <nfs/nfs_clnt.h>
#include <nfs/vfs.h>
#endif
#elif HAVE_OS_H                 /* BeOS */
#include <fs_info.h>
#endif

#if USE_STATVFS
#if ! STAT_STATVFS && STAT_STATVFS64
#define STRUCT_STATVFS struct statvfs64
#define STATFS statvfs64
#else
#define STRUCT_STATVFS struct statvfs
#define STATFS statvfs
/* Return true if statvfs works.  This is false for statvfs on systems
   with GNU libc on Linux kernels before 2.6.36, which stats all
   preceding entries in /proc/mounts; that makes df hang if even one
   of the corresponding file systems is hard-mounted but not available.  */
#if ! (__linux__ && (__GLIBC__ || __UCLIBC__))
static int
statvfs_works (void)
{
    return 1;
}
#else
#include <string.h>             /* for strverscmp */
#include <sys/utsname.h>
#include <sys/statfs.h>
#define STAT_STATFS2_BSIZE 1

static int
statvfs_works (void)
{
    static int statvfs_works_cache = -1;
    struct utsname name;

    if (statvfs_works_cache < 0)
        statvfs_works_cache = (uname (&name) == 0 && 0 <= strverscmp (name.release, "2.6.36"));
    return statvfs_works_cache;
}
#endif
#endif
#else
#define STATFS statfs
#define STRUCT_STATVFS struct statfs
#if HAVE_OS_H                   /* BeOS */
/* BeOS has a statvfs function, but it does not return sensible values
   for f_files, f_ffree and f_favail, and lacks f_type, f_basetype and
   f_fstypename.  Use 'struct fs_info' instead.  */
static int
statfs (char const *filename, struct fs_info *buf)
{
    dev_t device = dev_for_path (filename);

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

#if HAVE_STRUCT_STATVFS_F_BASETYPE
#define STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME f_basetype
#else
#if HAVE_STRUCT_STATVFS_F_FSTYPENAME || HAVE_STRUCT_STATFS_F_FSTYPENAME
#define STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME f_fstypename
#elif HAVE_OS_H                 /* BeOS */
#define STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME fsh_name
#endif
#endif

#include <unistd.h>

#include "lib/global.h"

#include "lib/tty/key.h"        /* tty_get_event */
#include "lib/mcconfig.h"
#include "lib/search.h"
#include "lib/vfs/vfs.h"
#include "lib/strescape.h"
#include "lib/strutil.h"
#include "lib/timefmt.h"        /* file_date() */
#include "lib/util.h"
#include "lib/widget.h"

#include "src/setup.h"          /* verbose */

#include "midnight.h"
#include "fileopctx.h"          /* FILE_CONT */

#include "filegui.h"

/* }}} */

/*** global variables ****************************************************************************/

int classic_progressbar = 1;

/*** file scope macro definitions ****************************************************************/

/* Hack: the vfs code should not rely on this */
#define WITH_FULL_PATHS 1

#define truncFileString(dlg, s)       str_trunc (s, WIDGET (dlg)->cols - 10)
#define truncFileStringSecure(dlg, s) path_trunc (s, WIDGET (dlg)->cols - 10)

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
    REPLACE_ALWAYS,
    REPLACE_UPDATE,
    REPLACE_NEVER,
    REPLACE_ABORT,
    REPLACE_SIZE,
    REPLACE_REGET
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
    WLabel *file_string[2];
    WLabel *file_label[2];
    WGauge *progress_file_gauge;
    WLabel *progress_file_label;

    WGauge *progress_total_gauge;

    WLabel *total_files_processed_label;
    WLabel *time_label;
    WHLine *total_bytes_label;

    /* Query replace dialog */
    WDialog *replace_dlg;
    const char *replace_filename;
    replace_action_t replace_result;

    struct stat *s_stat, *d_stat;
} FileOpContextUI;

/*** file scope variables ************************************************************************/

struct
{
    Widget *w;
    FileProgressStatus action;
    const char *text;
    button_flags_t flags;
    int len;
} progress_buttons[] =
{
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

static gboolean
filegui__check_attrs_on_fs (const char *fs_path)
{
    STRUCT_STATVFS stfs;

    if (!setup_copymove_persistent_attr)
        return FALSE;

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
    eta_hours = eta_secs / (60 * 60);
    eta_mins = (eta_secs - (eta_hours * 60 * 60)) / 60;
    eta_s = eta_secs - (eta_hours * 60 * 60 + eta_mins * 60);
    g_snprintf (buffer, BUF_TINY, _("%d:%02d.%02d"), eta_hours, eta_mins, eta_s);
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
    {
        g_snprintf (buffer, BUF_TINY, _("%.2f MB/s"), bps / (1024 * 1024.0));
    }
    else if (bps > 1024)
    {
        g_snprintf (buffer, BUF_TINY, _("%.2f KB/s"), bps / 1024.0);
    }
    else if (bps > 1)
    {
        g_snprintf (buffer, BUF_TINY, _("%ld B/s"), bps);
    }
    else
        *buffer = '\0';
}

/* --------------------------------------------------------------------------------------------- */
/*
 * FIXME: probably it is better to replace this with quick dialog machinery,
 * but actually I'm not familiar with it and have not much time :(
 *   alex
 */
static replace_action_t
overwrite_query_dialog (FileOpContext * ctx, enum OperationMode mode)
{
#define ADD_RD_BUTTON(i, ypos) \
    add_widget_autopos (ui->replace_dlg, \
                        button_new (ypos, rd_widgets [i].xpos, rd_widgets [i].value, \
                                    NORMAL_BUTTON, rd_widgets [i].text, NULL), \
                        rd_widgets [i].pos_flags, ui->replace_dlg->current->data)

#define ADD_RD_LABEL(i, p1, p2, ypos) \
    g_snprintf (buffer, sizeof (buffer), rd_widgets [i].text, p1, p2); \
    label2 = WIDGET (label_new (ypos, rd_widgets [i].xpos, buffer)); \
    add_widget_autopos (ui->replace_dlg, label2, rd_widgets [i].pos_flags, \
                        ui->replace_dlg->current != NULL ? ui->replace_dlg->current->data : NULL)

    /* dialog sizes */
    const int rd_ylen = 1;
    int rd_xlen = 60;
    int y = 2;
    unsigned long yes_id;

    struct
    {
        const char *text;
        int ypos, xpos;
        widget_pos_flags_t pos_flags;
        int value;              /* 0 for labels */
    } rd_widgets[] =
    {
    /* *INDENT-OFF* */
        /*  0 */
        { N_("Target file already exists!"), 3, 4, WPOS_KEEP_TOP | WPOS_CENTER_HORZ, 0 },
        /*  1 */
        { "%s", 4, 4, WPOS_KEEP_TOP | WPOS_CENTER_HORZ, 0 },
        /*  2 */
        { N_("New     : %s, size %s"), 6, 4, WPOS_KEEP_DEFAULT, 0 },
        /*  3 */
        { N_("Existing: %s, size %s"), 7, 4, WPOS_KEEP_DEFAULT, 0 },
        /*  4 */
        { N_("Overwrite this target?"), 9, 4, WPOS_KEEP_DEFAULT, 0 },
        /*  5 */
        { N_("&Yes"), 9, 28, WPOS_KEEP_DEFAULT, REPLACE_YES },
        /*  6 */
        { N_("&No"), 9, 37, WPOS_KEEP_DEFAULT, REPLACE_NO },
        /*  7 */
        { N_("A&ppend"), 9, 45, WPOS_KEEP_DEFAULT, REPLACE_APPEND },
        /*  8 */
        { N_("&Reget"), 10, 28, WPOS_KEEP_DEFAULT, REPLACE_REGET },
        /*  9 */
        { N_("Overwrite all targets?"), 11, 4, WPOS_KEEP_DEFAULT, 0 },
        /* 10 */
        { N_("A&ll"), 11, 28, WPOS_KEEP_DEFAULT, REPLACE_ALWAYS },
        /* 11 */
        { N_("&Update"), 11, 36, WPOS_KEEP_DEFAULT, REPLACE_UPDATE },
        /* 12 */
        { N_("Non&e"), 11, 47, WPOS_KEEP_DEFAULT, REPLACE_NEVER },
        /* 13 */
        { N_("If &size differs"), 12, 28, WPOS_KEEP_DEFAULT, REPLACE_SIZE },
        /* 14 */
        { N_("&Abort"), 14, 25, WPOS_KEEP_TOP | WPOS_CENTER_HORZ, REPLACE_ABORT }
    /* *INDENT-ON* */
    };

    const size_t num = G_N_ELEMENTS (rd_widgets);
    int *widgets_len;

    FileOpContextUI *ui = ctx->ui;

    char buffer[BUF_SMALL];
    char fsize_buffer[BUF_SMALL];
    Widget *label1, *label2;
    const char *title;
    vfs_path_t *stripped_vpath;
    const char *stripped_name;
    char *stripped_name_orig;
    int result;

    widgets_len = g_new0 (int, num);

    if (mode == Foreground)
        title = _("File exists");
    else
        title = _("Background process: File exists");

    stripped_vpath = vfs_path_from_str (ui->replace_filename);
    stripped_name = stripped_name_orig =
        vfs_path_to_str_flags (stripped_vpath, 0, VPF_STRIP_HOME | VPF_STRIP_PASSWORD);
    vfs_path_free (stripped_vpath);

    {
        size_t i;
        int l1, l2, l, row;
        int stripped_name_len;

        for (i = 0; i < num; i++)
        {
#ifdef ENABLE_NLS
            if (i != 1)         /* skip filename */
                rd_widgets[i].text = _(rd_widgets[i].text);
#endif /* ENABLE_NLS */
            widgets_len[i] = str_term_width1 (rd_widgets[i].text);
        }

        /*
         * longest of "Overwrite..." labels
         * (assume "Target date..." are short enough)
         */
        l1 = max (widgets_len[9], widgets_len[4]);

        /* longest of button rows */
        l = l2 = 0;
        row = 0;
        for (i = 1; i < num - 1; i++)
            if (rd_widgets[i].value != 0)
            {
                if (row != rd_widgets[i].ypos)
                {
                    row = rd_widgets[i].ypos;
                    l2 = max (l2, l);
                    l = 0;
                }
                l += widgets_len[i] + 4;
            }

        l2 = max (l2, l);       /* last row */
        rd_xlen = max (rd_xlen, l1 + l2 + 8);
        /* rd_xlen = max (rd_xlen, str_term_width1 (title) + 2); */
        stripped_name_len = str_term_width1 (stripped_name);
        rd_xlen = max (rd_xlen, min (COLS, stripped_name_len + 8));

        /* Now place widgets */
        l1 += 5;                /* start of first button in the row */
        l = l1;
        row = 0;
        for (i = 2; i < num - 1; i++)
            if (rd_widgets[i].value != 0)
            {
                if (row != rd_widgets[i].ypos)
                {
                    row = rd_widgets[i].ypos;
                    l = l1;
                }
                rd_widgets[i].xpos = l;
                l += widgets_len[i] + 4;
            }
    }

    /* FIXME - missing help node */
    ui->replace_dlg =
        create_dlg (TRUE, 0, 0, rd_ylen, rd_xlen, alarm_colors, NULL, NULL, "[Replace]", title,
                    DLG_CENTER);

    /* prompt */
    ADD_RD_LABEL (0, "", "", y++);
    /* file name */
    ADD_RD_LABEL (1, "", "", y++);
    label1 = label2;

    add_widget (ui->replace_dlg, hline_new (y++, -1, -1));

    /* source date and size */
    size_trunc_len (fsize_buffer, sizeof (fsize_buffer), ui->s_stat->st_size, -1,
                    panels_options.kilobyte_si);
    ADD_RD_LABEL (2, file_date (ui->s_stat->st_mtime), fsize_buffer, y++);
    rd_xlen = max (rd_xlen, label2->cols + 8);
    /* destination date and size */
    size_trunc_len (fsize_buffer, sizeof (fsize_buffer), ui->d_stat->st_size, -1,
                    panels_options.kilobyte_si);
    ADD_RD_LABEL (3, file_date (ui->d_stat->st_mtime), fsize_buffer, y++);
    rd_xlen = max (rd_xlen, label2->cols + 8);

    add_widget (ui->replace_dlg, hline_new (y++, -1, -1));

    ADD_RD_LABEL (4, 0, 0, y);  /* Overwrite this target? */
    yes_id = ADD_RD_BUTTON (5, y);      /* Yes */
    ADD_RD_BUTTON (6, y);       /* No */

    /* "this target..." widgets */
    if (!S_ISDIR (ui->d_stat->st_mode))
    {
        ADD_RD_BUTTON (7, y++); /* Append */

        if ((ctx->operation == OP_COPY) && (ui->d_stat->st_size != 0)
            && (ui->s_stat->st_size > ui->d_stat->st_size))
            ADD_RD_BUTTON (8, y++);     /* Reget */
    }

    add_widget (ui->replace_dlg, hline_new (y++, -1, -1));

    ADD_RD_LABEL (9, 0, 0, y);  /* Overwrite all targets? */
    ADD_RD_BUTTON (10, y);      /* All" */
    ADD_RD_BUTTON (11, y);      /* Update */
    ADD_RD_BUTTON (12, y++);    /* None */
    ADD_RD_BUTTON (13, y++);    /* If size differs */

    add_widget (ui->replace_dlg, hline_new (y++, -1, -1));

    ADD_RD_BUTTON (14, y);      /* Abort */

    label_set_text (LABEL (label1), str_trunc (stripped_name, rd_xlen - 8));
    dlg_set_size (ui->replace_dlg, y + 3, rd_xlen);
    dlg_select_by_id (ui->replace_dlg, yes_id);
    result = run_dlg (ui->replace_dlg);
    destroy_dlg (ui->replace_dlg);

    g_free (widgets_len);
    g_free (stripped_name_orig);

    return (result == B_CANCEL) ? REPLACE_ABORT : (replace_action_t) result;
#undef ADD_RD_LABEL
#undef ADD_RD_BUTTON
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
is_wildcarded (char *p)
{
    for (; *p; p++)
    {
        if (*p == '*')
            return TRUE;
        if (*p == '\\' && p[1] >= '1' && p[1] <= '9')
            return TRUE;
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
place_progress_buttons (WDialog * h, gboolean suspended)
{
    const size_t i = suspended ? 2 : 1;
    Widget *w = WIDGET (h);
    int buttons_width;

    buttons_width = 2 + progress_buttons[0].len + progress_buttons[3].len;
    buttons_width += progress_buttons[i].len;
    button_set_text (BUTTON (progress_buttons[i].w), progress_buttons[i].text);

    progress_buttons[0].w->x = w->x + (w->cols - buttons_width) / 2;
    progress_buttons[i].w->x = progress_buttons[0].w->x + progress_buttons[0].len + 1;
    progress_buttons[3].w->x = progress_buttons[i].w->x + progress_buttons[i].len + 1;
}

/* --------------------------------------------------------------------------------------------- */

static int
progress_button_callback (WButton * button, int action)
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
check_progress_buttons (FileOpContext * ctx)
{
    int c;
    Gpm_Event event;
    FileOpContextUI *ui;

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
            dlg_redraw (ui->op_dlg);
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
        dlg_redraw (ui->op_dlg);
        /* fallthrough */
    default:
        if (ctx->suspended)
            goto get_event;
        return FILE_CONT;
    }
}

/* --------------------------------------------------------------------------------------------- */
/* {{{ File progress display routines */

void
file_op_context_create_ui (FileOpContext * ctx, gboolean with_eta,
                           filegui_dialog_type_t dialog_type)
{
    FileOpContextUI *ui;
    int buttons_width;
    int dlg_width = 58, dlg_height = 17;
    int y = 2, x = 3;

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
    ctx->ui = g_new0 (FileOpContextUI, 1);

    ui = ctx->ui;
    ui->replace_result = REPLACE_YES;
    ui->showing_eta = with_eta && ctx->progress_totals_computed;
    ui->showing_bps = with_eta;

    ui->op_dlg =
        create_dlg (TRUE, 0, 0, dlg_height, dlg_width, dialog_colors, NULL, NULL, NULL,
                    op_names[ctx->operation], DLG_CENTER);

    ui->file_label[0] = label_new (y++, x, "");
    add_widget (ui->op_dlg, ui->file_label[0]);

    ui->file_string[0] = label_new (y++, x, "");
    add_widget (ui->op_dlg, ui->file_string[0]);

    ui->file_label[1] = label_new (y++, x, "");
    add_widget (ui->op_dlg, ui->file_label[1]);

    ui->file_string[1] = label_new (y++, x, "");
    add_widget (ui->op_dlg, ui->file_string[1]);

    ui->progress_file_gauge = gauge_new (y++, x + 3, dlg_width - (x + 3) * 2, FALSE, 100, 0);
    if (!classic_progressbar && (current_panel == right_panel))
        ui->progress_file_gauge->from_left_to_right = FALSE;
    add_widget_autopos (ui->op_dlg, ui->progress_file_gauge, WPOS_KEEP_TOP | WPOS_KEEP_HORZ, NULL);

    ui->progress_file_label = label_new (y++, x, "");
    add_widget (ui->op_dlg, ui->progress_file_label);

    if (verbose && dialog_type == FILEGUI_DIALOG_MULTI_ITEM)
    {
        ui->total_bytes_label = hline_new (y++, -1, -1);
        add_widget (ui->op_dlg, ui->total_bytes_label);

        if (ctx->progress_totals_computed)
        {
            ui->progress_total_gauge =
                gauge_new (y++, x + 3, dlg_width - (x + 3) * 2, FALSE, 100, 0);
            if (!classic_progressbar && (current_panel == right_panel))
                ui->progress_total_gauge->from_left_to_right = FALSE;
            add_widget_autopos (ui->op_dlg, ui->progress_total_gauge,
                                WPOS_KEEP_TOP | WPOS_KEEP_HORZ, NULL);
        }

        ui->total_files_processed_label = label_new (y++, x, "");
        add_widget (ui->op_dlg, ui->total_files_processed_label);

        ui->time_label = label_new (y++, x, "");
        add_widget (ui->op_dlg, ui->time_label);
    }

    add_widget (ui->op_dlg, hline_new (y++, -1, -1));

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
        send_message (progress_buttons[2].w, NULL, MSG_DESTROY, 0, NULL);
        g_free (progress_buttons[2].w);
    }
    progress_buttons[2].w = progress_buttons[1].w;

    progress_buttons[3].w = WIDGET (button_new (y, 0, progress_buttons[3].action,
                                                progress_buttons[3].flags, progress_buttons[3].text,
                                                NULL));
    if (progress_buttons[3].len == -1)
        progress_buttons[3].len = button_get_len (BUTTON (progress_buttons[3].w));

    add_widget (ui->op_dlg, progress_buttons[0].w);
    add_widget (ui->op_dlg, progress_buttons[1].w);
    add_widget (ui->op_dlg, progress_buttons[3].w);

    buttons_width = 2 +
        progress_buttons[0].len + max (progress_buttons[1].len, progress_buttons[2].len) +
        progress_buttons[3].len;

    /* adjust dialog sizes  */
    dlg_set_size (ui->op_dlg, y + 3, max (COLS * 2 / 3, buttons_width + 6));

    place_progress_buttons (ui->op_dlg, FALSE);

    dlg_select_widget (progress_buttons[0].w);

    /* We will manage the dialog without any help, that's why
       we have to call init_dlg */
    init_dlg (ui->op_dlg);
}

/* --------------------------------------------------------------------------------------------- */

void
file_op_context_destroy_ui (FileOpContext * ctx)
{
    if (ctx != NULL && ctx->ui != NULL)
    {
        FileOpContextUI *ui = (FileOpContextUI *) ctx->ui;

        dlg_run_done (ui->op_dlg);
        destroy_dlg (ui->op_dlg);
        g_free (ui);
        ctx->ui = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
   show progressbar for file
 */

void
file_progress_show (FileOpContext * ctx, off_t done, off_t total,
                    const char *stalled_msg, gboolean force_update)
{
    FileOpContextUI *ui;
    char buffer[BUF_TINY];
    char buffer2[BUF_TINY];
    char buffer3[BUF_TINY];

    if (!verbose || ctx == NULL || ctx->ui == NULL)
        return;

    ui = ctx->ui;

    if (total == 0)
    {
        gauge_show (ui->progress_file_gauge, 0);
        return;
    }

    gauge_set_value (ui->progress_file_gauge, 1024, (int) (1024 * done / total));
    gauge_show (ui->progress_file_gauge, 1);

    if (!force_update)
        return;

    if (ui->showing_eta && ctx->eta_secs > 0.5)
    {
        file_eta_prepare_for_show (buffer2, ctx->eta_secs, FALSE);
        if (ctx->bps == 0)
            g_snprintf (buffer, BUF_TINY, "%s %s", buffer2, stalled_msg);
        else
        {
            file_bps_prepare_for_show (buffer3, ctx->bps);
            g_snprintf (buffer, BUF_TINY, "%s (%s) %s", buffer2, buffer3, stalled_msg);
        }
    }
    else
    {
        g_snprintf (buffer, BUF_TINY, "%s", stalled_msg);
    }

    label_set_text (ui->progress_file_label, buffer);
}

/* --------------------------------------------------------------------------------------------- */

void
file_progress_show_count (FileOpContext * ctx, size_t done, size_t total)
{
    char buffer[BUF_TINY];
    FileOpContextUI *ui;

    if (ctx == NULL || ctx->ui == NULL)
        return;

    ui = ctx->ui;
    if (ctx->progress_totals_computed)
        g_snprintf (buffer, BUF_TINY, _("Files processed: %zu/%zu"), done, total);
    else
        g_snprintf (buffer, BUF_TINY, _("Files processed: %zu"), done);
    label_set_text (ui->total_files_processed_label, buffer);
}

/* --------------------------------------------------------------------------------------------- */

void
file_progress_show_total (FileOpTotalContext * tctx, FileOpContext * ctx, uintmax_t copied_bytes,
                          gboolean show_summary)
{
    char buffer[BUF_TINY];
    char buffer2[BUF_TINY];
    char buffer3[BUF_TINY];
    char buffer4[BUF_TINY];
    struct timeval tv_current;
    FileOpContextUI *ui;

    if (ctx == NULL || ctx->ui == NULL)
        return;

    ui = ctx->ui;

    if (ctx->progress_totals_computed)
    {
        if (ctx->progress_bytes == 0)
            gauge_show (ui->progress_total_gauge, 0);
        else
        {
            gauge_set_value (ui->progress_total_gauge, 1024,
                             (int) (1024 * copied_bytes / ctx->progress_bytes));
            gauge_show (ui->progress_total_gauge, 1);
        }
    }

    if (!show_summary && tctx->bps == 0)
        return;

    gettimeofday (&tv_current, NULL);
    file_frmt_time (buffer2, tv_current.tv_sec - tctx->transfer_start.tv_sec);

    if (ctx->progress_totals_computed)
    {
        file_eta_prepare_for_show (buffer3, tctx->eta_secs, TRUE);
        if (tctx->bps == 0)
            g_snprintf (buffer, BUF_TINY, _("Time: %s %s"), buffer2, buffer3);
        else
        {
            file_bps_prepare_for_show (buffer4, (long) tctx->bps);
            g_snprintf (buffer, BUF_TINY, _("Time: %s %s (%s)"), buffer2, buffer3, buffer4);
        }
    }
    else
    {
        if (tctx->bps == 0)
            g_snprintf (buffer, BUF_TINY, _("Time: %s"), buffer2);
        else
        {
            file_bps_prepare_for_show (buffer4, (long) tctx->bps);
            g_snprintf (buffer, BUF_TINY, _("Time: %s (%s)"), buffer2, buffer4);
        }
    }

    label_set_text (ui->time_label, buffer);

    size_trunc_len (buffer2, 5, tctx->copied_bytes, 0, panels_options.kilobyte_si);
    if (!ctx->progress_totals_computed)
        g_snprintf (buffer, BUF_TINY, _(" Total: %s "), buffer2);
    else
    {
        size_trunc_len (buffer3, 5, ctx->progress_bytes, 0, panels_options.kilobyte_si);
        g_snprintf (buffer, BUF_TINY, _(" Total: %s/%s "), buffer2, buffer3);
    }

    hline_set_text (ui->total_bytes_label, buffer);
}

/* }}} */

/* --------------------------------------------------------------------------------------------- */

void
file_progress_show_source (FileOpContext * ctx, const vfs_path_t * s_vpath)
{
    FileOpContextUI *ui;

    if (ctx == NULL || ctx->ui == NULL)
        return;

    ui = ctx->ui;

    if (s_vpath != NULL)
    {
        char *s;

        s = vfs_path_tokens_get (s_vpath, -1, 1);
        label_set_text (ui->file_label[0], _("Source"));
        label_set_text (ui->file_string[0], truncFileString (ui->op_dlg, s));
        g_free (s);
    }
    else
    {
        label_set_text (ui->file_label[0], "");
        label_set_text (ui->file_string[0], "");
    }
}

/* --------------------------------------------------------------------------------------------- */

void
file_progress_show_target (FileOpContext * ctx, const vfs_path_t * s_vpath)
{
    FileOpContextUI *ui;

    if (ctx == NULL || ctx->ui == NULL)
        return;

    ui = ctx->ui;

    if (s_vpath != NULL)
    {
        char *s;

        s = vfs_path_to_str (s_vpath);
        label_set_text (ui->file_label[1], _("Target"));
        label_set_text (ui->file_string[1], truncFileStringSecure (ui->op_dlg, s));
        g_free (s);
    }
    else
    {
        label_set_text (ui->file_label[1], "");
        label_set_text (ui->file_string[1], "");
    }
}

/* --------------------------------------------------------------------------------------------- */

void
file_progress_show_deleting (FileOpContext * ctx, const char *s)
{
    FileOpContextUI *ui;

    if (ctx == NULL || ctx->ui == NULL)
        return;

    ui = ctx->ui;
    label_set_text (ui->file_label[0], _("Deleting"));
    label_set_text (ui->file_label[0], truncFileStringSecure (ui->op_dlg, s));
}

/* --------------------------------------------------------------------------------------------- */

FileProgressStatus
file_progress_real_query_replace (FileOpContext * ctx,
                                  enum OperationMode mode, const char *destname,
                                  struct stat * _s_stat, struct stat * _d_stat)
{
    FileOpContextUI *ui;

    if (ctx == NULL || ctx->ui == NULL)
        return FILE_CONT;

    ui = ctx->ui;

    if (ui->replace_result < REPLACE_ALWAYS)
    {
        ui->replace_filename = destname;
        ui->s_stat = _s_stat;
        ui->d_stat = _d_stat;
        ui->replace_result = overwrite_query_dialog (ctx, mode);
    }

    switch (ui->replace_result)
    {
    case REPLACE_UPDATE:
        do_refresh ();
        if (_s_stat->st_mtime > _d_stat->st_mtime)
            return FILE_CONT;
        else
            return FILE_SKIP;

    case REPLACE_SIZE:
        do_refresh ();
        if (_s_stat->st_size == _d_stat->st_size)
            return FILE_SKIP;
        else
            return FILE_CONT;

    case REPLACE_REGET:
        /* Careful: we fall through and set do_append */
        ctx->do_reget = _d_stat->st_size;

    case REPLACE_APPEND:
        ctx->do_append = TRUE;

    case REPLACE_YES:
    case REPLACE_ALWAYS:
        do_refresh ();
        return FILE_CONT;
    case REPLACE_NO:
    case REPLACE_NEVER:
        do_refresh ();
        return FILE_SKIP;
    case REPLACE_ABORT:
    default:
        return FILE_ABORT;
    }
}

/* --------------------------------------------------------------------------------------------- */

char *
file_mask_dialog (FileOpContext * ctx, FileOperation operation,
                  gboolean only_one,
                  const char *format, const void *text, const char *def_text, gboolean * do_bg)
{
    size_t fmd_xlen;
    vfs_path_t *vpath;
    int source_easy_patterns = easy_patterns;
    char fmd_buf[BUF_MEDIUM];
    char *dest_dir, *tmp;
    char *def_text_secure;

    if (ctx == NULL)
        return NULL;

    /* unselect checkbox if target filesystem don't support attributes */
    ctx->op_preserve = filegui__check_attrs_on_fs (def_text);
    ctx->stable_symlinks = FALSE;
    *do_bg = FALSE;

    /* filter out a possible password from def_text */
    vpath = vfs_path_from_str_flags (def_text, only_one ? VPF_NO_CANON : VPF_NONE);
    tmp = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_PASSWORD);
    vfs_path_free (vpath);

    if (source_easy_patterns)
        def_text_secure = strutils_glob_escape (tmp);
    else
        def_text_secure = strutils_regex_escape (tmp);
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
            fmd_xlen = max (fmd_xlen, 68);
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
        fmd_xlen = max (fmd_xlen, 68);
        g_snprintf (fmd_buf, sizeof (fmd_buf), format, *(const int *) text);
    }

    {
        char *source_mask, *orig_mask;
        int val;
        struct stat buf;

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABELED_INPUT (fmd_buf, input_label_above,
                                 easy_patterns ? "*" : "^(.*)$", "input-def", &source_mask,
                                 NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
            QUICK_START_COLUMNS,
                QUICK_SEPARATOR (FALSE),
            QUICK_NEXT_COLUMN,
                QUICK_CHECKBOX (N_("&Using shell patterns"), &source_easy_patterns, NULL),
            QUICK_STOP_COLUMNS,
            QUICK_LABELED_INPUT (N_("to:"), input_label_above,
                                 def_text_secure, "input2", &dest_dir, NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
            QUICK_SEPARATOR (TRUE),
            QUICK_START_COLUMNS,
                QUICK_CHECKBOX (N_("Follow &links"), &ctx->follow_links, NULL),
                QUICK_CHECKBOX (N_("Preserve &attributes"), &ctx->op_preserve, NULL),
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

        quick_dialog_t qdlg = {
            -1, -1, fmd_xlen,
            op_names[operation], "[Mask Copy/Rename]",
            quick_widgets, NULL, NULL
        };

      ask_file_mask:
        val = quick_dialog_skip (&qdlg, 4);

        if (val == B_CANCEL)
        {
            g_free (def_text_secure);
            return NULL;
        }

        if (ctx->follow_links)
            ctx->stat_func = mc_stat;
        else
            ctx->stat_func = mc_lstat;

        if (ctx->op_preserve)
        {
            ctx->preserve = TRUE;
            ctx->umask_kill = 0777777;
            ctx->preserve_uidgid = (geteuid () == 0);
        }
        else
        {
            int i2;

            ctx->preserve = ctx->preserve_uidgid = FALSE;
            i2 = umask (0);
            umask (i2);
            ctx->umask_kill = i2 ^ 0777777;
        }

        if ((dest_dir == NULL) || (*dest_dir == '\0'))
        {
            g_free (def_text_secure);
            g_free (source_mask);
            return dest_dir;
        }

        ctx->search_handle = mc_search_new (source_mask, -1);

        if (ctx->search_handle == NULL)
        {
            message (D_ERROR, MSG_ERROR, _("Invalid source pattern `%s'"), source_mask);
            g_free (dest_dir);
            g_free (source_mask);
            goto ask_file_mask;
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
        vfs_path_free (vpath);
        if (val == B_USER)
            *do_bg = TRUE;
    }

    return dest_dir;
}

/* --------------------------------------------------------------------------------------------- */
