/* File management GUI for the text mode edition
 *
 * Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
 * 2004, 2005, 2006, 2007 Free Software Foundation, Inc.
 *  
 * Written by: 1994, 1995       Janne Kukonlehto
 *             1994, 1995       Fred Leeflang
 *             1994, 1995, 1996 Miguel de Icaza
 *             1995, 1996       Jakub Jelinek
 *	       1997             Norbert Warmuth
 *	       1998		Pavel Machek
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


/* {{{ Include files */

#include <config.h>

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "global.h"
#include "setup.h"		/* verbose */
#include "dialog.h"		/* do_refresh() */
#include "color.h"		/* dialog_colors */
#include "widget.h"		/* WLabel */
#define WANT_WIDGETS
#include "main.h"		/* the_hint */
#include "wtools.h"		/* QuickDialog */
#include "panel.h"		/* current_panel */
#include "fileopctx.h"		/* FILE_CONT */
#include "filegui.h"
#include "key.h"		/* get_event */
#include "util.h"               /* strip_password() */
#include "tty.h"

#ifdef HAVE_CHARSET
#include "recode.h"
#endif

/* }}} */

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
    c = get_event (&event, 0, 0);
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
file_op_context_create_ui (FileOpContext *ctx, int with_eta)
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

#define truncFileString(ui, s)       name_trunc (s, ui->eta_extra + 47)
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

#define X_TRUNC 52

/*
 * FIXME: probably it is better to replace this with quick dialog machinery,
 * but actually I'm not familiar with it and have not much time :(
 *   alex
 */
static struct {
    const char *text;
    int ypos, xpos;
    int value;			/* 0 for labels */
} rd_widgets[] = {
    {
    N_("Target file \"%s\" already exists!"), 3, 4, 0}, {
    N_("&Abort"), BY + 3, 25, REPLACE_ABORT}, {
    N_("If &size differs"), BY + 1, 28, REPLACE_SIZE}, {
    N_("Non&e"), BY, 47, REPLACE_NEVER}, {
    N_("&Update"), BY, 36, REPLACE_UPDATE}, {
    N_("A&ll"), BY, 28, REPLACE_ALWAYS}, {
    N_("Overwrite all targets?"), BY, 4, 0}, {
    N_("&Reget"), BY - 1, 28, REPLACE_REGET}, {
    N_("A&ppend"), BY - 2, 45, REPLACE_APPEND}, {
    N_("&No"), BY - 2, 37, REPLACE_NO}, {
    N_("&Yes"), BY - 2, 28, REPLACE_YES}, {
    N_("Overwrite this target?"), BY - 2, 4, 0}, {
#if (defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64) || (defined _LARGE_FILES && _LARGE_FILES)
    N_("Target date: %s, size %llu"), 6, 4, 0}, {
    N_("Source date: %s, size %llu"), 5, 4, 0}
#else
    N_("Target date: %s, size %u"), 6, 4, 0}, {
    N_("Source date: %s, size %u"), 5, 4, 0}
#endif
};

#define ADD_RD_BUTTON(i)\
	add_widget (ui->replace_dlg,\
		button_new (rd_widgets [i].ypos, rd_widgets [i].xpos, rd_widgets [i].value,\
		NORMAL_BUTTON, rd_widgets [i].text, 0))

#define ADD_RD_LABEL(ui,i,p1,p2)\
	g_snprintf (buffer, sizeof (buffer), rd_widgets [i].text, p1, p2);\
	add_widget (ui->replace_dlg,\
		label_new (rd_widgets [i].ypos, rd_widgets [i].xpos, buffer))

static void
init_replace (FileOpContext *ctx, enum OperationMode mode)
{
    FileOpContextUI *ui;
    char buffer[BUF_SMALL];
    const char *title;
    static int rd_xlen = 60, rd_trunc = X_TRUNC;

#ifdef ENABLE_NLS
    static int i18n_flag;
    if (!i18n_flag) {
	int l1, l2, l, row;
	register int i = sizeof (rd_widgets) / sizeof (rd_widgets[0]);
	while (i--)
	    rd_widgets[i].text = _(rd_widgets[i].text);

	/* 
	 * longest of "Overwrite..." labels 
	 * (assume "Target date..." are short enough)
	 */
	l1 = max (mbstrlen (rd_widgets[6].text),
		  mbstrlen (rd_widgets[11].text));

	/* longest of button rows */
	i = sizeof (rd_widgets) / sizeof (rd_widgets[0]);
	for (row = l = l2 = 0; i--;) {
	    if (rd_widgets[i].value != 0) {
		if (row != rd_widgets[i].ypos) {
		    row = rd_widgets[i].ypos;
		    l2 = max (l2, l);
		    l = 0;
		}
		l += mbstrlen (rd_widgets[i].text) + 4;
	    }
	}
	l2 = max (l2, l);	/* last row */
	rd_xlen = max (rd_xlen, l1 + l2 + 8);
	rd_trunc = rd_xlen - 6;

	/* Now place buttons */
	l1 += 5;		/* start of first button in the row */
	i = sizeof (rd_widgets) / sizeof (rd_widgets[0]);

	for (l = l1, row = 0; --i > 1;) {
	    if (rd_widgets[i].value != 0) {
		if (row != rd_widgets[i].ypos) {
		    row = rd_widgets[i].ypos;
		    l = l1;
		}
		rd_widgets[i].xpos = l;
		l += mbstrlen (rd_widgets[i].text) + 4;
	    }
	}
	/* Abort button is centered */
	rd_widgets[1].xpos =
	    (rd_xlen - mbstrlen (rd_widgets[1].text) - 3) / 2;
    }
#endif				/* ENABLE_NLS */

    ui = ctx->ui;

    if (mode == Foreground)
	title = _(" File exists ");
    else
	title = _(" Background process: File exists ");

    /* FIXME - missing help node */
    ui->replace_dlg =
	create_dlg (0, 0, 16, rd_xlen, alarm_colors, NULL, "[Replace]",
		    title, DLG_CENTER | DLG_REVERSE);


    ADD_RD_LABEL (ui, 0,
		  name_trunc (ui->replace_filename,
			      rd_trunc - mbstrlen (rd_widgets[0].text)), 0);
    ADD_RD_BUTTON (1);

    ADD_RD_BUTTON (2);
    ADD_RD_BUTTON (3);
    ADD_RD_BUTTON (4);
    ADD_RD_BUTTON (5);
    ADD_RD_LABEL (ui, 6, 0, 0);

    /* "this target..." widgets */
    if (!S_ISDIR (ui->d_stat->st_mode)) {
	if ((ctx->operation == OP_COPY) && (ui->d_stat->st_size != 0)
	    && (ui->s_stat->st_size > ui->d_stat->st_size))
	    ADD_RD_BUTTON (7);	/* reget */

	ADD_RD_BUTTON (8);	/* Overwrite all targets? */
    }
    ADD_RD_BUTTON (9);
    ADD_RD_BUTTON (10);
    ADD_RD_LABEL (ui, 11, 0, 0);

    ADD_RD_LABEL (ui, 12, file_date (ui->d_stat->st_mtime),
		  (off_t) ui->d_stat->st_size);
    ADD_RD_LABEL (ui, 13, file_date (ui->s_stat->st_mtime),
		  (off_t) ui->s_stat->st_size);
}

void
file_progress_set_stalled_label (FileOpContext *ctx, const char *stalled_msg)
{
    FileOpContextUI *ui;

    g_return_if_fail (ctx != NULL);

    if (ctx->ui == NULL)
	return;

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
	init_replace (ctx, mode);
	run_dlg (ui->replace_dlg);
	ui->replace_result = ui->replace_dlg->ret_value;
	if (ui->replace_result == B_CANCEL)
	    ui->replace_result = REPLACE_ABORT;
	destroy_dlg (ui->replace_dlg);
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

#ifdef HAVE_CHARSET
#define FMDY 15
#else
#define FMDY 13
#endif

#define	FMD_XLEN 64
extern int fmd_xlen;
static QuickWidget fmd_widgets[] = {

#ifdef HAVE_CHARSET
 #define Y_OK 12
#else
 #define Y_OK 9
#endif

#ifdef WITH_BACKGROUND
 #define ADD 0
#else
 #define ADD -1
#endif

   #define FM_STAB_SYM         0
  #define FM_DIVE_INTO_SUBDIR 1
  #define FM_PRES_ATTR        2
  #define FM_FOLLOW_LINKS     3
  #define FM_DST_INPUT        4
  #define FM_DST_TITLE        5
  #define FM_USING_SHELL_PATT 6
  #define FM_SRC_INPUT        7
  #define FM_SRC_TITLE        8
  #define FM_CANCEL           9
#ifdef WITH_BACKGROUND
  #define FM_BKGND            10
#endif
  #define FM_OK               11+ADD
#ifdef HAVE_CHARSET
  #define FM_TO_CODEPAGE      12+ADD
  #define FM_FROM_CODEPAGE    13+ADD
  #define FM_RECODE_TITLE     14+ADD
  #define FM_RECODE_ARROW     15+ADD
#endif // HAVE_CHARSET


#ifdef HAVE_CHARSET
 #define SKIP             10
 #define B_FROM B_USER+1
 #define B_TO   B_USER+2
#else
 #define SKIP             10
#endif

    {quick_checkbox, 42,64, 8, FMDY, N_("&Stable Symlinks"),0,0,0,0,"stab-sym"},
    {quick_checkbox, 31,64, 7, FMDY, N_("&Dive into subdir if exists"),0,0,0,0,"dive"},
    {quick_checkbox, 3, 64, 8, FMDY, N_("preserve &Attributes"),9,0,0,0,"preserve"},
    {quick_checkbox, 3, 64, 7, FMDY, N_("follow &Links"),7,0,0,0,"follow"},
    {quick_input,    3, 64, 6, FMDY, "", 58, 0, 0, 0, "input2"},
    {quick_label,    3, 64, 5, FMDY, N_("to:"), 0, 0, 0, 0, "to"},
    {quick_checkbox, 37,64, 4, FMDY, N_("&Using shell patterns"),0,0, 0,0,"us-sh"},
    {quick_input,    3, 64, 3, FMDY, "", 58, 0, 0, 0, "input-def"},
    {quick_label,    3, 64, 2, FMDY, "", 0, 0, 0, 0, "ql"},
    {quick_button,   42,64, Y_OK, FMDY, N_("&Cancel"), 0, B_CANCEL, 0,0, "cancel"},
#ifdef WITH_BACKGROUND
    {quick_button,   25,64, Y_OK, FMDY, N_("&Background"), 0, B_USER, 0,0, "back"},
#endif
    {quick_button,   14,64, Y_OK, FMDY, N_("&OK"), 0, B_ENTER, 0, 0, "ok"},
#ifdef HAVE_CHARSET
    {quick_button,   46,64, 10, FMDY,"to codepage", 0, B_TO, 0, 0, "ql"},
    {quick_button,   25,64, 10, FMDY, "from codepage", 0, B_FROM, 0, 0, "ql"},
    {quick_label,    3, 64, 10, FMDY, N_("Recode file names:"), 0, 0, 0, 0, "ql"},
    {quick_label,    42,64, 10, FMDY, "->", 0, 0, 0, 0, "ql"},
#endif
    {0}
};

static int
is_wildcarded (char *p)
{
    for (; *p; p++) {
	if (*p == '*')
	    return 1;
	else if (*p == '\\' && p[1] >= '1' && p[1] <= '9')
	    return 1;
    }
    return 0;
}

void
fmd_init_i18n (int force)
{
#ifdef ENABLE_NLS
    static int initialized = FALSE;
    register int i;
    int len;

    if (initialized && !force)
	return;

    for (i = sizeof (op_names) / sizeof (op_names[0]); i--;)
	op_names[i] = _(op_names[i]);

    i = sizeof (fmd_widgets) / sizeof (fmd_widgets[0]) - 1;
    while (i--)
	if (fmd_widgets[i].text[0] != '\0')
	    fmd_widgets[i].text = _(fmd_widgets[i].text);

    len = mbstrlen (fmd_widgets[FM_FOLLOW_LINKS].text)
	+ mbstrlen (fmd_widgets[FM_DIVE_INTO_SUBDIR].text) + 15;
    fmd_xlen = max (fmd_xlen, len);

    len = mbstrlen (fmd_widgets[FM_PRES_ATTR].text)
	+ mbstrlen (fmd_widgets[FM_STAB_SYM].text) + 15;
    fmd_xlen = max (fmd_xlen, len);

    len = mbstrlen (fmd_widgets[FM_CANCEL].text)
	+ mbstrlen (fmd_widgets[FM_OK].text) + 11;

#ifdef FM_BKGND
    len += mbstrlen (fmd_widgets[FM_BKGND].text) + 6;
#endif

    fmd_xlen = max (fmd_xlen, len + 4);

    len = (fmd_xlen - (len + 6)) / 2;
    i = fmd_widgets[FM_OK].relative_x = len + 3;
    i += mbstrlen (fmd_widgets[FM_OK].text) + 8;

#ifdef FM_BKGND
    fmd_widgets[FM_BKGND].relative_x = i;
     i += mbstrlen (fmd_widgets[FM_BKGND].text) + 6;
#endif

    fmd_widgets[FM_CANCEL].relative_x = i;

#define	chkbox_xpos(i) \
	fmd_widgets [i].relative_x = fmd_xlen - mbstrlen (fmd_widgets [i].text) - 6

    chkbox_xpos (FM_USING_SHELL_PATT);
    chkbox_xpos (FM_DIVE_INTO_SUBDIR);
    chkbox_xpos (FM_STAB_SYM);

    if (fmd_xlen != FMD_XLEN) {
	i = sizeof (fmd_widgets) / sizeof (fmd_widgets[0]) - 1;
	while (i--)
	    fmd_widgets[i].x_divisions = fmd_xlen;

	fmd_widgets[FM_SRC_INPUT].hotkey_pos =
	    fmd_widgets[FM_DST_INPUT].hotkey_pos = fmd_xlen - 6;
    }
#undef chkbox_xpos

    initialized = TRUE;
#endif				/* !ENABLE_NLS */
}

char *
file_mask_dialog (FileOpContext *ctx, FileOperation operation, const char *text,
		  const char *def_text_orig, int only_one, int *do_background)
{
    int source_easy_patterns = easy_patterns;
    char *source_mask, *orig_mask, *dest_dir, *tmpdest;
    const char *error;
    char *def_text_secure;
    struct stat buf;
    int val;
    QuickDialog Quick_input;
    char *def_text;
#ifdef HAVE_CHARSET
    char *errmsg;
#endif
    g_return_val_if_fail (ctx != NULL, NULL);

    def_text = g_strdup(def_text_orig);

#if 0
    message (1, __FUNCTION__, "text = `%s' \n def_text = `%s'", text,
		def_text);
#endif

#ifdef UTF8
	fix_utf8(def_text);
#endif

    fmd_init_i18n (FALSE);

    /* Set up the result pointers */

    fmd_widgets[FM_PRES_ATTR].result = &ctx->op_preserve;
    fmd_widgets[FM_FOLLOW_LINKS].result = &ctx->follow_links;
    fmd_widgets[FM_STAB_SYM].result = &ctx->stable_symlinks;
    fmd_widgets[FM_DIVE_INTO_SUBDIR].result = &ctx->dive_into_subdirs;


    /* filter out a possible password from def_text */
    def_text_secure = strip_password (g_strdup (def_text), 1);

    /* Create the dialog */

    ctx->stable_symlinks = 0;
    fmd_widgets[FM_USING_SHELL_PATT].result = &source_easy_patterns;
    fmd_widgets[FM_SRC_INPUT].text = easy_patterns ? "*" : "^\\(.*\\)$";

    Quick_input.xlen = fmd_xlen;
    Quick_input.xpos = -1;
    Quick_input.title = op_names[operation];
    Quick_input.help = "[Mask Copy/Rename]";
    Quick_input.ylen = FMDY;
    Quick_input.i18n = 1;
    Quick_input.widgets = fmd_widgets;
    fmd_widgets[FM_SRC_TITLE].text = text;
    fmd_widgets[FM_DST_INPUT].text = def_text_secure;
    fmd_widgets[FM_DST_INPUT].str_result = &dest_dir;
    fmd_widgets[FM_SRC_INPUT].str_result = &source_mask;

    *do_background = 0;

#ifdef HAVE_CHARSET
    ctx->from_codepage=current_panel->src_codepage;
    ctx->to_codepage=left_panel->src_codepage;
    if (left_panel) {
        ctx->to_codepage=left_panel->src_codepage;
        if( (current_panel==left_panel) && right_panel ) ctx->to_codepage=right_panel->src_codepage;
    }
#endif

  ask_file_mask:

#ifdef HAVE_CHARSET
    if(operation!=OP_COPY && operation!=OP_MOVE) {
      ctx->from_codepage=-1;
      ctx->to_codepage=-1;
    }
    fmd_widgets[FM_FROM_CODEPAGE].text=get_codepage_id(ctx->from_codepage);
    fmd_widgets[FM_TO_CODEPAGE].text=get_codepage_id(ctx->to_codepage);
#endif

    if ((val = quick_dialog_skip (&Quick_input, SKIP)) == B_CANCEL) {
	g_free (def_text_secure);
	return 0;
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
	int i;
	ctx->preserve = ctx->preserve_uidgid = 0;
	i = umask (0);
	umask (i);
	ctx->umask_kill = i ^ 0777777;
    }

    orig_mask = source_mask;
    if (!dest_dir || !*dest_dir) {
	g_free (source_mask);
    g_free (def_text_secure);
        g_free(def_text);
	return dest_dir;
    }
    if (source_easy_patterns) {
	source_easy_patterns = easy_patterns;
	easy_patterns = 1;
	source_mask = convert_pattern (source_mask, match_file, 1);
	easy_patterns = source_easy_patterns;
	error =
	    re_compile_pattern (source_mask, strlen (source_mask),
				&ctx->rx);
	g_free (source_mask);
    } else
	error =
	    re_compile_pattern (source_mask, strlen (source_mask),
				&ctx->rx);

    if (error) {
	message (1, MSG_ERROR, _("Invalid source pattern `%s' \n %s "),
		    orig_mask, error);
	g_free (orig_mask);
	goto ask_file_mask;
    }
    g_free (orig_mask);

    tmpdest = dest_dir;
    dest_dir = tilde_expand(tmpdest);
    g_free(tmpdest);

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
	ctx->dest_mask = g_strdup ("*");
    else {
	ctx->dest_mask = g_strdup (ctx->dest_mask);
	*orig_mask = 0;
    }
    if (!*dest_dir) {
	g_free (dest_dir);
	dest_dir = g_strdup ("./");
    }
    if (val == B_USER)
	*do_background = 1;
#ifdef HAVE_CHARSET
    if(val == B_FROM) {
      if(operation==OP_COPY || operation==OP_MOVE) {
        if(display_codepage<=0) {
          message( 1, _(" Warning "),
                      _("To use this feature select your codepage in\n"
                        "Setup / Display Bits dialog!\n"
                        "Do not forget to save options." ));
          goto ask_file_mask;
        }
        ctx->from_codepage=select_charset(ctx->from_codepage,0,
                            _(" Choose \"FROM\" codepage for COPY/MOVE operaion "));
      }
      else
        message(1,"Warning",_("Recoding works only with COPY or MOVE operation"));
      goto ask_file_mask;
    }
    if(val == B_TO) {
      if(operation==OP_COPY || operation==OP_MOVE) {
        if(display_codepage<=0) {
          message( 1, _(" Warning "),
                      _("To use this feature select your codepage in\n"
                        "Setup / Display Bits dialog!\n"
                        "Do not forget to save options." ));
          goto ask_file_mask;
        }
        ctx->to_codepage=select_charset(ctx->to_codepage,0,
                            _(" Choose \"TO\" codepage for COPY/MOVE operaion "));
      }
      else
        message(1,"Warning",_("Recoding works only with COPY or MOVE operation"));
      goto ask_file_mask;
    }

    errmsg=my_init_tt(ctx->to_codepage,ctx->from_codepage,ctx->tr_table);
    if(errmsg) {
      my_reset_tt(ctx->tr_table,256);
      message( 1, MSG_ERROR, "%s", errmsg);
    }
#endif

    g_free(def_text_secure);
    g_free(def_text);
    return dest_dir;
}
