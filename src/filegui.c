/* File management GUI for the text mode edition
 *
 * Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
 * 2004, 2005, 2006, 2007, 2009 Free Software Foundation, Inc.
 *
 * Written by: 1994, 1995       Janne Kukonlehto
 *             1994, 1995       Fred Leeflang
 *             1994, 1995, 1996 Miguel de Icaza
 *             1995, 1996       Jakub Jelinek
 *	       1997             Norbert Warmuth
 *	       1998		Pavel Machek
 *             2009             Slava Zanko
 *
 * The copy code was based in GNU's cp, and was written by:
 * Torbjorn Granlund, David MacKenzie, and Jim Meyering.
 *
 * The move code was based in GNU's mv, and was written by:
 * Mike Parker and David MacKenzie.
 *
 * Janne Kukonlehto added much error recovery to them for being used
 * in an interactive program.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(STAT_STATVFS) \
     && (defined(HAVE_STRUCT_STATVFS_F_BASETYPE) \
         || defined(HAVE_STRUCT_STATVFS_F_FSTYPENAME))
#   include <sys/statvfs.h>
#   define STRUCT_STATFS struct statvfs
#   define STATFS statvfs
#elif defined(HAVE_STATFS) && !defined(STAT_STATFS4)
#   ifdef HAVE_SYS_VFS_H
#      include <sys/vfs.h>
#   elif defined(HAVE_SYS_MOUNT_H) && defined(HAVE_SYS_PARAM_H)
#      include <sys/param.h>
#      include <sys/mount.h>
#   elif defined(HAVE_SYS_STATFS_H)
#      include <sys/statfs.h>
#   endif
#   define STRUCT_STATFS struct statfs
#   define STATFS statfs
#endif

#include <unistd.h>

#include "lib/global.h"

#include "lib/tty/key.h"		/* tty_get_event */
#include "lib/search/search.h"
#include "lib/vfs/mc-vfs/vfs.h"
#include "lib/strutil.h"

#include "setup.h"		/* verbose */
#include "dialog.h"		/* do_refresh() */
#include "widget.h"		/* WLabel */
#include "main-widgets.h"
#include "main.h"		/* the_hint */
#include "wtools.h"		/* QuickDialog */
#include "panel.h"		/* current_panel */
#include "fileopctx.h"		/* FILE_CONT */
#include "filegui.h"
#include "src/strescape.h"

/* }}} */
typedef enum {
    MSDOS_SUPER_MAGIC     = 0x4d44,
    NTFS_SB_MAGIC         = 0x5346544e,
    NTFS_3G_MAGIC         = 0x65735546,
    PROC_SUPER_MAGIC      = 0x9fa0,
    SMB_SUPER_MAGIC       = 0x517B,
    NCP_SUPER_MAGIC       = 0x564c,
    USBDEVICE_SUPER_MAGIC = 0x9fa2
} filegui_nonattrs_fs_t;

/* Hack: the vfs code should not rely on this */
#define WITH_FULL_PATHS 1

/* This structure describes the UI and internal data required by a file
 * operation context.
 */
typedef struct {
    /* ETA and bps */

    int showing_eta;
    int showing_bps;
    int eta_extra;

    /* Dialog and widgets for the operation progress window */

    Dlg_head *op_dlg;

    WLabel *file_label[2];
    WLabel *file_string[2];
    WLabel *progress_label[3];
    WGauge *progress_gauge[3];
    WLabel *eta_label;
    WLabel *bps_label;
    WLabel *stalled_label;

    /* Query replace dialog */

    Dlg_head *replace_dlg;
    const char *replace_filename;
    int replace_result;
    struct stat *s_stat, *d_stat;
} FileOpContextUI;


/* Used to save the hint line */
static int last_hint_line;

/* File operate window sizes */
#define WX 62
#define WY 10
#define BY 10
#define WX_ETA_EXTRA  12

#define FCOPY_GAUGE_X 14
#define FCOPY_LABEL_X 5

/* Used for button result values */
enum {
    REPLACE_YES = B_USER,
    REPLACE_NO,
    REPLACE_APPEND,
    REPLACE_ALWAYS,
    REPLACE_UPDATE,
    REPLACE_NEVER,
    REPLACE_ABORT,
    REPLACE_SIZE,
    REPLACE_REGET
};

static int
filegui__check_attrs_on_fs(const char *fs_path)
{
#ifdef STATFS
    STRUCT_STATFS stfs;

    if (!setup_copymove_persistent_attr)
        return 0;

    if (STATFS(fs_path, &stfs)!=0)
        return 1;

# ifdef __linux__
    switch ((filegui_nonattrs_fs_t) stfs.f_type)
    {
    case MSDOS_SUPER_MAGIC:
    case NTFS_SB_MAGIC:
    case NTFS_3G_MAGIC:
    case PROC_SUPER_MAGIC:
    case SMB_SUPER_MAGIC:
    case NCP_SUPER_MAGIC:
    case USBDEVICE_SUPER_MAGIC:
        return 0;
        break;
    }
# elif defined(HAVE_STRUCT_STATFS_F_FSTYPENAME) \
      || defined(HAVE_STRUCT_STATVFS_F_FSTYPENAME)
    if (!strcmp(stfs.f_fstypename, "msdos")
        || !strcmp(stfs.f_fstypename, "msdosfs")
        || !strcmp(stfs.f_fstypename, "ntfs")
        || !strcmp(stfs.f_fstypename, "procfs")
        || !strcmp(stfs.f_fstypename, "smbfs")
        || strstr(stfs.f_fstypename, "fusefs"))
        return 0;
# elif defined(HAVE_STRUCT_STATVFS_F_BASETYPE)
    if (!strcmp(stfs.f_basetype, "pcfs")
        || !strcmp(stfs.f_basetype, "ntfs")
        || !strcmp(stfs.f_basetype, "proc")
        || !strcmp(stfs.f_basetype, "smbfs")
        || !strcmp(stfs.f_basetype, "fuse"))
        return 0;
# endif
#endif /* STATFS */

    return 1;
}

static FileProgressStatus
check_progress_buttons (FileOpContext *ctx)
{
    int c;
    Gpm_Event event;
    FileOpContextUI *ui;

    if (ctx->ui == NULL)
	return FILE_CONT;

    ui = ctx->ui;

    event.x = -1;		/* Don't show the GPM cursor */
    c = tty_get_event (&event, FALSE, FALSE);
    if (c == EV_NONE)
	return FILE_CONT;

    /* Reinitialize to avoid old values after events other than
       selecting a button */
    ui->op_dlg->ret_value = FILE_CONT;

    dlg_process_event (ui->op_dlg, c, &event);
    switch (ui->op_dlg->ret_value) {
    case FILE_SKIP:
	return FILE_SKIP;
	break;
    case B_CANCEL:
    case FILE_ABORT:
	return FILE_ABORT;
	break;
    default:
	return FILE_CONT;
    }
}

/* {{{ File progress display routines */

void
file_op_context_create_ui_without_init (FileOpContext *ctx, int with_eta)
{
    FileOpContextUI *ui;
    int x_size;
    int minus;
    int eta_offset;
    const char *sixty;
    const char *fifteen;

    g_return_if_fail (ctx != NULL);
    g_return_if_fail (ctx->ui == NULL);

    ui = g_new0 (FileOpContextUI, 1);
    ctx->ui = ui;

    minus = verbose ? 0 : 3;
    eta_offset = with_eta ? (WX_ETA_EXTRA) / 2 : 0;

    sixty = "";
    fifteen = "";

    ctx->recursive_result = 0;

    ui->replace_result = 0;
    ui->showing_eta = with_eta;
    ui->showing_bps = with_eta;
    ui->eta_extra = with_eta ? WX_ETA_EXTRA : 0;
    x_size = (WX + 4) + ui->eta_extra;

    ui->op_dlg =
	create_dlg (0, 0, WY - minus + 4, x_size, dialog_colors, NULL,
		    NULL, op_names[ctx->operation],
		    DLG_CENTER | DLG_REVERSE);

    last_hint_line = the_hint->widget.y;
    if ((ui->op_dlg->y + ui->op_dlg->lines) > last_hint_line)
	the_hint->widget.y = ui->op_dlg->y + ui->op_dlg->lines + 1;

    add_widget (ui->op_dlg,
		button_new (BY - minus, WX - 19 + eta_offset, FILE_ABORT,
			    NORMAL_BUTTON, _("&Abort"), 0));
    add_widget (ui->op_dlg,
		button_new (BY - minus, 14 + eta_offset, FILE_SKIP,
			    NORMAL_BUTTON, _("&Skip"), 0));

    add_widget (ui->op_dlg, ui->progress_gauge[2] =
		gauge_new (7, FCOPY_GAUGE_X, 0, 100, 0));
    add_widget (ui->op_dlg, ui->progress_label[2] =
		label_new (7, FCOPY_LABEL_X, fifteen));
    add_widget (ui->op_dlg, ui->bps_label = label_new (7, WX, ""));

    add_widget (ui->op_dlg, ui->progress_gauge[1] =
		gauge_new (8, FCOPY_GAUGE_X, 0, 100, 0));
    add_widget (ui->op_dlg, ui->progress_label[1] =
		label_new (8, FCOPY_LABEL_X, fifteen));
    add_widget (ui->op_dlg, ui->stalled_label = label_new (8, WX, ""));

    add_widget (ui->op_dlg, ui->progress_gauge[0] =
		gauge_new (6, FCOPY_GAUGE_X, 0, 100, 0));
    add_widget (ui->op_dlg, ui->progress_label[0] =
		label_new (6, FCOPY_LABEL_X, fifteen));
    add_widget (ui->op_dlg, ui->eta_label = label_new (6, WX, ""));

    add_widget (ui->op_dlg, ui->file_string[1] =
		label_new (4, FCOPY_GAUGE_X, sixty));
    add_widget (ui->op_dlg, ui->file_label[1] =
		label_new (4, FCOPY_LABEL_X, fifteen));
    add_widget (ui->op_dlg, ui->file_string[0] =
		label_new (3, FCOPY_GAUGE_X, sixty));
    add_widget (ui->op_dlg, ui->file_label[0] =
		label_new (3, FCOPY_LABEL_X, fifteen));
}

void
file_op_context_create_ui (FileOpContext *ctx, int with_eta)
{
    FileOpContextUI *ui;

    g_return_if_fail (ctx != NULL);
    g_return_if_fail (ctx->ui == NULL);

    file_op_context_create_ui_without_init(ctx, with_eta);
    ui = ctx->ui;

    /* We will manage the dialog without any help, that's why
       we have to call init_dlg */
    init_dlg (ui->op_dlg);
    ui->op_dlg->running = 1;
}

void
file_op_context_destroy_ui (FileOpContext *ctx)
{
    FileOpContextUI *ui;

    g_return_if_fail (ctx != NULL);

    if (ctx->ui) {
	ui = ctx->ui;

	dlg_run_done (ui->op_dlg);
	destroy_dlg (ui->op_dlg);
	g_free (ui);
    }

    the_hint->widget.y = last_hint_line;

    ctx->ui = NULL;
}

static FileProgressStatus
show_no_bar (FileOpContext *ctx, int n)
{
    FileOpContextUI *ui;

    if (ctx->ui == NULL)
	return FILE_CONT;

    ui = ctx->ui;

    if (n >= 0) {
	label_set_text (ui->progress_label[n], "");
	gauge_show (ui->progress_gauge[n], 0);
    }
    return check_progress_buttons (ctx);
}

static FileProgressStatus
show_bar (FileOpContext *ctx, int n, double done, double total)
{
    FileOpContextUI *ui;

    if (ctx->ui == NULL)
	return FILE_CONT;

    ui = ctx->ui;

    /*
     * Gauge needs integers, so give it with integers between 0 and 1023.
     * This precision should be quite reasonable.
     */
    gauge_set_value (ui->progress_gauge[n], 1024,
		     (int) (1024 * done / total));
    gauge_show (ui->progress_gauge[n], 1);
    return check_progress_buttons (ctx);
}

static void
file_eta_show (FileOpContext *ctx)
{
    int eta_hours, eta_mins, eta_s;
    char eta_buffer[BUF_TINY];
    FileOpContextUI *ui;

    if (ctx->ui == NULL)
	return;

    ui = ctx->ui;

    if (!ui->showing_eta)
	return;

    if (ctx->eta_secs > 0.5) {
	eta_hours = ctx->eta_secs / (60 * 60);
	eta_mins = (ctx->eta_secs - (eta_hours * 60 * 60)) / 60;
	eta_s = ctx->eta_secs - (eta_hours * 60 * 60 + eta_mins * 60);
	g_snprintf (eta_buffer, sizeof (eta_buffer), _("ETA %d:%02d.%02d"),
		    eta_hours, eta_mins, eta_s);
    } else
	*eta_buffer = 0;

    label_set_text (ui->eta_label, eta_buffer);
}

static void
file_bps_show (FileOpContext *ctx)
{
    char bps_buffer[BUF_TINY];
    FileOpContextUI *ui;

    if (ctx->ui == NULL)
	return;

    ui = ctx->ui;

    if (!ui->showing_bps)
	return;

    if (ctx->bps > 1024 * 1024) {
	g_snprintf (bps_buffer, sizeof (bps_buffer), _("%.2f MB/s"),
		    ctx->bps / (1024 * 1024.0));
    } else if (ctx->bps > 1024) {
	g_snprintf (bps_buffer, sizeof (bps_buffer), _("%.2f KB/s"),
		    ctx->bps / 1024.0);
    } else if (ctx->bps > 1) {
	g_snprintf (bps_buffer, sizeof (bps_buffer), _("%ld B/s"),
		    ctx->bps);
    } else
	*bps_buffer = 0;

    label_set_text (ui->bps_label, bps_buffer);
}

FileProgressStatus
file_progress_show (FileOpContext *ctx, off_t done, off_t total)
{
    FileOpContextUI *ui;

    g_return_val_if_fail (ctx != NULL, FILE_CONT);

    if (ctx->ui == NULL)
	return FILE_CONT;

    ui = ctx->ui;

    if (!verbose)
	return check_progress_buttons (ctx);
    if (total > 0) {
	label_set_text (ui->progress_label[0], _("File"));
	file_eta_show (ctx);
	file_bps_show (ctx);
	return show_bar (ctx, 0, done, total);
    } else
	return show_no_bar (ctx, 0);
}

FileProgressStatus
file_progress_show_count (FileOpContext *ctx, off_t done, off_t total)
{
    FileOpContextUI *ui;

    g_return_val_if_fail (ctx != NULL, FILE_CONT);

    if (ctx->ui == NULL)
	return FILE_CONT;

    ui = ctx->ui;

    if (!verbose)
	return check_progress_buttons (ctx);
    if (total > 0) {
	label_set_text (ui->progress_label[1], _("Count"));
	return show_bar (ctx, 1, done, total);
    } else
	return show_no_bar (ctx, 1);
}

FileProgressStatus
file_progress_show_bytes (FileOpContext *ctx, double done, double total)
{
    FileOpContextUI *ui;

    g_return_val_if_fail (ctx != NULL, FILE_CONT);

    if (ctx->ui == NULL)
	return FILE_CONT;

    ui = ctx->ui;

    if (!verbose)
	return check_progress_buttons (ctx);
    if (total > 0) {
	label_set_text (ui->progress_label[2], _("Bytes"));
	return show_bar (ctx, 2, done, total);
    } else
	return show_no_bar (ctx, 2);
}

/* }}} */

#define truncFileString(ui, s)       str_trunc (s, ui->eta_extra + 47)
#define truncFileStringSecure(ui, s) path_trunc (s, ui->eta_extra + 47)

FileProgressStatus
file_progress_show_source (FileOpContext *ctx, const char *s)
{
    FileOpContextUI *ui;

    g_return_val_if_fail (ctx != NULL, FILE_CONT);

    if (ctx->ui == NULL)
	return FILE_CONT;

    ui = ctx->ui;

    if (s != NULL) {
#ifdef WITH_FULL_PATHS
	int i = strlen (current_panel->cwd);

	/* We remove the full path we have added before */
	if (!strncmp (s, current_panel->cwd, i)) {
	    if (s[i] == PATH_SEP)
		s += i + 1;
	}
#endif				/* WITH_FULL_PATHS */

	label_set_text (ui->file_label[0], _("Source"));
	label_set_text (ui->file_string[0], truncFileString (ui, s));
	return check_progress_buttons (ctx);
    } else {
	label_set_text (ui->file_label[0], "");
	label_set_text (ui->file_string[0], "");
	return check_progress_buttons (ctx);
    }
}

FileProgressStatus
file_progress_show_target (FileOpContext *ctx, const char *s)
{
    FileOpContextUI *ui;

    g_return_val_if_fail (ctx != NULL, FILE_CONT);

    if (ctx->ui == NULL)
	return FILE_CONT;

    ui = ctx->ui;

    if (s != NULL) {
	label_set_text (ui->file_label[1], _("Target"));
	label_set_text (ui->file_string[1], truncFileStringSecure (ui, s));
	return check_progress_buttons (ctx);
    } else {
	label_set_text (ui->file_label[1], "");
	label_set_text (ui->file_string[1], "");
	return check_progress_buttons (ctx);
    }
}

FileProgressStatus
file_progress_show_deleting (FileOpContext *ctx, const char *s)
{
    FileOpContextUI *ui;

    g_return_val_if_fail (ctx != NULL, FILE_CONT);

    if (ctx->ui == NULL)
	return FILE_CONT;

    ui = ctx->ui;

    label_set_text (ui->file_label[0], _("Deleting"));
    label_set_text (ui->file_label[0], truncFileStringSecure (ui, s));
    return check_progress_buttons (ctx);
}

/*
 * FIXME: probably it is better to replace this with quick dialog machinery,
 * but actually I'm not familiar with it and have not much time :(
 *   alex
 */


static int
overwrite_query_dialog (FileOpContext *ctx, enum OperationMode mode)
{
#define ADD_RD_BUTTON(i)\
	add_widget (ui->replace_dlg,\
		button_new (rd_widgets [i].ypos, rd_widgets [i].xpos, rd_widgets [i].value,\
		NORMAL_BUTTON, rd_widgets [i].text, 0))

#define ADD_RD_LABEL(i, p1, p2)\
	g_snprintf (buffer, sizeof (buffer), rd_widgets [i].text, p1, p2);\
	add_widget (ui->replace_dlg,\
		label_new (rd_widgets [i].ypos, rd_widgets [i].xpos, buffer))

    /* dialog sizes */
    const int rd_ylen = 17;
    int rd_xlen = 60;

    struct {
	const char *text;
	int ypos, xpos;
	int value;			/* 0 for labels */
    } rd_widgets[] = {
	/*  0 */ { N_("Target file already exists!"), 3, 4, 0 },
	/*  1 */ { "%s", 4, 4, 0 },
#if (defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64) || (defined _LARGE_FILES && _LARGE_FILES)
	/*  2 */ { N_("Source date: %s, size %llu"), 6, 4, 0 },
	/*  3 */ { N_("Target date: %s, size %llu"), 7, 4, 0 },
#else
	/*  2 */ { N_("Source date: %s, size %u"), 6, 4, 0 },
	/*  3 */ { N_("Target date: %s, size %u"), 7, 4, 0 },
#endif
	/*  4 */ { N_("&Abort"), 14, 25, REPLACE_ABORT },
	/*  5 */ { N_("If &size differs"), 12, 28, REPLACE_SIZE },
	/*  6 */ { N_("Non&e"), 11, 47, REPLACE_NEVER },
	/*  7 */ { N_("&Update"), 11, 36, REPLACE_UPDATE },
	/*  8 */ { N_("A&ll"), 11, 28, REPLACE_ALWAYS },
	/*  9 */ { N_("Overwrite all targets?"), 11, 4, 0 },
	/* 10 */ { N_("&Reget"), 10, 28, REPLACE_REGET },
	/* 11 */ { N_("A&ppend"), 9, 45, REPLACE_APPEND },
	/* 12 */ { N_("&No"), 9, 37, REPLACE_NO },
	/* 13 */ { N_("&Yes"), 9, 28, REPLACE_YES },
	/* 14 */ { N_("Overwrite this target?"), 9, 4, 0 }
    };

    const int num = sizeof (rd_widgets) / sizeof (rd_widgets[0]);
    int *widgets_len;

    FileOpContextUI *ui = ctx->ui;

    char buffer[BUF_SMALL];
    const char *title;
    const char *stripped_name = strip_home_and_password (ui->replace_filename);
    int stripped_name_len;

    int result;

    widgets_len = g_new0 (int, num);

    if (mode == Foreground)
	title = _(" File exists ");
    else
	title = _(" Background process: File exists ");

    stripped_name_len = str_term_width1 (stripped_name);

    {
	int i, l1, l2, l, row;

	for (i = 0; i < num; i++) {
#ifdef ENABLE_NLS
	    if (i != 1) /* skip filename */
		rd_widgets[i].text = _(rd_widgets[i].text);
#endif				/* ENABLE_NLS */
	    widgets_len [i] = str_term_width1 (rd_widgets[i].text);
	}

	/*
	 * longest of "Overwrite..." labels
	 * (assume "Target date..." are short enough)
	 */
	l1 = max (widgets_len[9], widgets_len[14]);

	/* longest of button rows */
	i = num;
	for (row = l = l2 = 0; i--;)
	    if (rd_widgets[i].value != 0) {
		if (row != rd_widgets[i].ypos) {
		    row = rd_widgets[i].ypos;
		    l2 = max (l2, l);
		    l = 0;
		}
		l += widgets_len[i] + 4;
	    }

	l2 = max (l2, l);	/* last row */
	rd_xlen = max (rd_xlen, l1 + l2 + 8);
	rd_xlen = max (rd_xlen, str_term_width1 (title) + 2);
	rd_xlen = max (rd_xlen, min (COLS, stripped_name_len + 8));

	/* Now place widgets */
	l1 += 5;		/* start of first button in the row */
	i = num;
	for (l = l1, row = 0; --i > 1;)
	    if (rd_widgets[i].value != 0) {
		if (row != rd_widgets[i].ypos) {
		    row = rd_widgets[i].ypos;
		    l = l1;
		}
		rd_widgets[i].xpos = l;
		l += widgets_len[i] + 4;
	    }

	/* Abort button is centered */
	rd_widgets[4].xpos = (rd_xlen - widgets_len[4] - 3) / 2;
    }

    /* FIXME - missing help node */
    ui->replace_dlg =
	create_dlg (0, 0, rd_ylen, rd_xlen, alarm_colors, NULL, "[Replace]",
		    title, DLG_CENTER | DLG_REVERSE);

    /* prompt -- centered */
    add_widget (ui->replace_dlg,
		label_new (rd_widgets [0].ypos,
			    (rd_xlen - widgets_len [0]) / 2,
			    rd_widgets [0].text));
    /* file name -- centered */
    stripped_name = str_trunc (stripped_name, rd_xlen - 8);
    stripped_name_len = str_term_width1 (stripped_name);
    add_widget (ui->replace_dlg,
		label_new (rd_widgets [1].ypos,
			    (rd_xlen - stripped_name_len) / 2,
			    stripped_name));

    /* source date */
    ADD_RD_LABEL (2, file_date (ui->s_stat->st_mtime),
		  (off_t) ui->s_stat->st_size);
    /* destination date */
    ADD_RD_LABEL (3, file_date (ui->d_stat->st_mtime),
		  (off_t) ui->d_stat->st_size);

    ADD_RD_BUTTON (4);			/* Abort */
    ADD_RD_BUTTON (5);			/* If size differs */
    ADD_RD_BUTTON (6);			/* None */
    ADD_RD_BUTTON (7);			/* Update */
    ADD_RD_BUTTON (8);			/* All" */
    ADD_RD_LABEL (9, 0, 0);		/* Overwrite all targets? */

    /* "this target..." widgets */
    if (!S_ISDIR (ui->d_stat->st_mode)) {
	if ((ctx->operation == OP_COPY) && (ui->d_stat->st_size != 0)
	    && (ui->s_stat->st_size > ui->d_stat->st_size))
	    ADD_RD_BUTTON (10);		/* Reget */

	ADD_RD_BUTTON (11);		/* Append */
    }
    ADD_RD_BUTTON (12);			/* No */
    ADD_RD_BUTTON (13);			/* Yes */
    ADD_RD_LABEL (14, 0, 0);		/* Overwrite this target? */

    result = run_dlg (ui->replace_dlg);
    destroy_dlg (ui->replace_dlg);

    g_free (widgets_len);

    return result;
#undef ADD_RD_LABEL
#undef ADD_RD_BUTTON
}

void
file_progress_set_stalled_label (FileOpContext *ctx, const char *stalled_msg)
{
    FileOpContextUI *ui;

    g_return_if_fail (ctx != NULL);
    g_return_if_fail (ctx->ui != NULL);

    ui = ctx->ui;
    label_set_text (ui->stalled_label, stalled_msg);
}

FileProgressStatus
file_progress_real_query_replace (FileOpContext *ctx,
				  enum OperationMode mode, const char *destname,
				  struct stat *_s_stat,
				  struct stat *_d_stat)
{
    FileOpContextUI *ui;

    g_return_val_if_fail (ctx != NULL, FILE_CONT);
    g_return_val_if_fail (ctx->ui != NULL, FILE_CONT);

    ui = ctx->ui;

    if (ui->replace_result < REPLACE_ALWAYS) {
	ui->replace_filename = destname;
	ui->s_stat = _s_stat;
	ui->d_stat = _d_stat;
	ui->replace_result = overwrite_query_dialog (ctx, mode);
	if (ui->replace_result == B_CANCEL)
	    ui->replace_result = REPLACE_ABORT;
    }

    switch (ui->replace_result) {
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
	ctx->do_append = 1;

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

static gboolean
is_wildcarded (char *p)
{
    for (; *p; p++) {
	if (*p == '*')
	    return TRUE;
	if (*p == '\\' && p[1] >= '1' && p[1] <= '9')
	    return TRUE;
    }
    return FALSE;
}

char *
file_mask_dialog (FileOpContext *ctx, FileOperation operation,
		    gboolean only_one,
		    const char *format, const void *text,
		    const char *def_text, gboolean *do_background)
{
    const size_t FMDY = 13;
    const size_t FMDX = 68;
    size_t fmd_xlen;

    /* buttons */
    const size_t gap = 1;
    size_t b0_len, b2_len;
    size_t b1_len = 0;

    int source_easy_patterns = easy_patterns;
    size_t i, len;
    char fmd_buf [BUF_MEDIUM];
    char *source_mask, *orig_mask, *dest_dir, *tmp;
    char *def_text_secure;
    int val;

    QuickWidget fmd_widgets[] =
    {
	/* 0 */ QUICK_BUTTON (42, 64, 10, FMDY, N_("&Cancel"), B_CANCEL, NULL),
#ifdef WITH_BACKGROUND
	/* 1 */ QUICK_BUTTON (25, 64, 10, FMDY, N_("&Background"), B_USER, NULL),
#define OFFSET 0
#else
#define OFFSET 1
#endif		/* WITH_BACKGROUND */
	/*  2 - OFFSET */
	QUICK_BUTTON (14, FMDX, 10, FMDY, N_("&OK"), B_ENTER, NULL),
	/*  3 - OFFSET */
	QUICK_CHECKBOX (42, FMDX, 8, FMDY, N_("&Stable Symlinks"), &ctx->stable_symlinks),
	/*  4 - OFFSET */
	QUICK_CHECKBOX (31, FMDX, 7, FMDY, N_("di&Ve into subdir if exists"), &ctx->dive_into_subdirs),
	/*  5 - OFFSET */
	QUICK_CHECKBOX (3, FMDX, 8, FMDY, N_("preserve &Attributes"), &ctx->op_preserve),
	/*  6 - OFFSET */
	QUICK_CHECKBOX (3, FMDX, 7, FMDY, N_("follow &Links"), &ctx->follow_links),
	/*  7 - OFFSET */
	QUICK_INPUT (3, FMDX, 6, FMDY, "", 58, 0, "input2", &dest_dir),
	/*  8 - OFFSET */
	QUICK_LABEL (3, FMDX, 5, FMDY, N_("to:")),
	/*  9 - OFFSET */
	QUICK_CHECKBOX (37, FMDX, 4, FMDY, N_("&Using shell patterns"), &source_easy_patterns),
	/* 10 - OFFSET */
	QUICK_INPUT (3, FMDX, 3, FMDY, easy_patterns ? "*" : "^\\(.*\\)$", 58, 0, "input-def", &source_mask),
	/* 11 - OFFSET */
	QUICK_LABEL (3, FMDX, 2, FMDY, fmd_buf),
	QUICK_END
    };

    g_return_val_if_fail (ctx != NULL, NULL);

#ifdef ENABLE_NLS
    /* buttons */
    for (i = 0; i <= 2 - OFFSET; i++)
	fmd_widgets[i].u.button.text = _(fmd_widgets[i].u.button.text);

    /* checkboxes */
    for (i = 3 - OFFSET; i <= 9 - OFFSET; i++)
	if (i != 7 - OFFSET)
	    fmd_widgets[i].u.checkbox.text = _(fmd_widgets[i].u.checkbox.text);
#endif				/* !ENABLE_NLS */

    fmd_xlen = max (FMDX, (size_t) COLS * 2/3);

    len = str_term_width1 (fmd_widgets[6 - OFFSET].u.checkbox.text)
	+ str_term_width1 (fmd_widgets[4 - OFFSET].u.checkbox.text) + 15;
    fmd_xlen = max (fmd_xlen, len);

    len = str_term_width1 (fmd_widgets[5 - OFFSET].u.checkbox.text)
	+ str_term_width1 (fmd_widgets[3 - OFFSET].u.checkbox.text) + 15;
    fmd_xlen = max (fmd_xlen, len);

    /* buttons */
    b2_len = str_term_width1 (fmd_widgets[2 - OFFSET].u.button.text) + 6 + gap; /* OK */
#ifdef WITH_BACKGROUND
    b1_len = str_term_width1 (fmd_widgets[1].u.button.text) + 4 + gap; /* Background */
#endif
    b0_len = str_term_width1 (fmd_widgets[0].u.button.text) + 4; /* Cancel */
    len = b0_len + b1_len + b2_len;
    fmd_xlen = min (max (fmd_xlen, len + 6), (size_t) COLS);

    if (only_one) {
	int flen;

	flen = str_term_width1 (format);
	i = fmd_xlen - flen - 4; /* FIXME */
	g_snprintf (fmd_buf, sizeof (fmd_buf),
		    format, str_trunc ((const char *) text, i));
    } else {
	g_snprintf (fmd_buf, sizeof (fmd_buf), format, *(const int *) text);
	fmd_xlen = max (fmd_xlen, (size_t) str_term_width1 (fmd_buf) + 6);
    }

    for (i = sizeof (fmd_widgets) / sizeof (fmd_widgets[0]); i > 0; )
	fmd_widgets[--i].x_divisions = fmd_xlen;

    i = (fmd_xlen - len)/2;
    /* OK button */
    fmd_widgets[2 - OFFSET].relative_x = i;
    i += b2_len;
#ifdef WITH_BACKGROUND
    /* Background button */
    fmd_widgets[1].relative_x = i;
    i += b1_len;
#endif
    /* Cancel button */
    fmd_widgets[0].relative_x = i;

#define chkbox_xpos(i) \
    fmd_widgets [i].relative_x = fmd_xlen - str_term_width1 (fmd_widgets [i].u.checkbox.text) - 6
    chkbox_xpos (3 - OFFSET);
    chkbox_xpos (4 - OFFSET);
    chkbox_xpos (9 - OFFSET);
#undef chkbox_xpos

    /* inputs */
    fmd_widgets[ 7 - OFFSET].u.input.len =
    fmd_widgets[10 - OFFSET].u.input.len = fmd_xlen - 6;

    /* unselect checkbox if target filesystem don't support attributes */
    ctx->op_preserve = filegui__check_attrs_on_fs (def_text);

    /* filter out a possible password from def_text */
    tmp = strip_password (g_strdup (def_text), 1);
    if (source_easy_patterns)
        def_text_secure = strutils_glob_escape (tmp);
    else
        def_text_secure = strutils_regex_escape (tmp);
    g_free (tmp);

    /* destination */
    fmd_widgets[7 - OFFSET].u.input.text = def_text_secure;

    ctx->stable_symlinks = 0;
    *do_background = FALSE;

    {
	struct stat buf;

	QuickDialog Quick_input =
	{
	    fmd_xlen, FMDY, -1, -1, op_names [operation],
	    "[Mask Copy/Rename]", fmd_widgets, TRUE
	};

     ask_file_mask:
	val = quick_dialog_skip (&Quick_input, 4);

	if (val == B_CANCEL) {
	    g_free (def_text_secure);
	    return NULL;
	}

	if (ctx->follow_links)
	    ctx->stat_func = mc_stat;
	else
	    ctx->stat_func = mc_lstat;

	if (ctx->op_preserve) {
	    ctx->preserve = 1;
	    ctx->umask_kill = 0777777;
	    ctx->preserve_uidgid = (geteuid () == 0) ? 1 : 0;
	} else {
	    int i2;
	    ctx->preserve = ctx->preserve_uidgid = 0;
	    i2 = umask (0);
	    umask (i2);
	    ctx->umask_kill = i2 ^ 0777777;
	}

	if (!dest_dir || !*dest_dir) {
	    g_free (def_text_secure);
	    g_free (source_mask);
	    return dest_dir;
	}

	ctx->search_handle = mc_search_new(source_mask,-1);

	if (ctx->search_handle == NULL) {
	    message (D_ERROR, MSG_ERROR, _("Invalid source pattern `%s'"),
			source_mask);
	    g_free (dest_dir);
	    g_free (source_mask);
	    goto ask_file_mask;
	}

	g_free (def_text_secure);
	g_free (source_mask);

	ctx->search_handle->is_case_sentitive = TRUE;
	if (source_easy_patterns)
	    ctx->search_handle->search_type = MC_SEARCH_T_GLOB;
	else
	    ctx->search_handle->search_type = MC_SEARCH_T_REGEX;

	tmp = dest_dir;
	dest_dir = tilde_expand (tmp);
	g_free (tmp);

	ctx->dest_mask = strrchr (dest_dir, PATH_SEP);
	if (ctx->dest_mask == NULL)
	    ctx->dest_mask = dest_dir;
	else
	    ctx->dest_mask++;
	orig_mask = ctx->dest_mask;
	if (!*ctx->dest_mask
	    || (!ctx->dive_into_subdirs && !is_wildcarded (ctx->dest_mask)
		&& (!only_one
		    || (!mc_stat (dest_dir, &buf) && S_ISDIR (buf.st_mode))))
	    || (ctx->dive_into_subdirs
		&& ((!only_one && !is_wildcarded (ctx->dest_mask))
		    || (only_one && !mc_stat (dest_dir, &buf)
			&& S_ISDIR (buf.st_mode)))))
	    ctx->dest_mask = g_strdup ("\\0");
	else {
	    ctx->dest_mask = g_strdup (ctx->dest_mask);
	    *orig_mask = '\0';
	}
	if (!*dest_dir) {
	    g_free (dest_dir);
	    dest_dir = g_strdup ("./");
	}
	if (val == B_USER)
	    *do_background = TRUE;
    }

    return dest_dir;
}
