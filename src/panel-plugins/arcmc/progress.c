/*
   Archive browser panel plugin — progress dialog.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"
#include "lib/tty/key.h"
#include "lib/widget.h"

#include "arcmc-types.h"
#include "progress.h"

/*** file scope functions ************************************************************************/

/* Format byte size into human-readable string with Russian units. */
static void
arcmc_format_size (off_t bytes, char *buf, size_t buflen)
{
    if (bytes < 1024)
        g_snprintf (buf, buflen, "%d Б", (int) bytes);
    else if (bytes < 1024 * 1024)
        g_snprintf (buf, buflen, "%.1f КБ", (double) bytes / 1024.0);
    else if (bytes < (off_t) 1024 * 1024 * 1024)
        g_snprintf (buf, buflen, "%.2f МБ", (double) bytes / (1024.0 * 1024.0));
    else
        g_snprintf (buf, buflen, "%.2f ГБ", (double) bytes / (1024.0 * 1024.0 * 1024.0));
}

/* --------------------------------------------------------------------------------------------- */

/* Dialog callback: intercept Esc/Cancel to set abort flag without closing dialog. */
static cb_ret_t
arcmc_progress_dlg_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    if (msg == MSG_ACTION && parm == CK_Cancel)
    {
        DIALOG (w)->ret_value = B_CANCEL;
        return MSG_HANDLED;
    }

    return dlg_default_callback (w, sender, msg, parm, data);
}

/* --------------------------------------------------------------------------------------------- */

/* Button callback: keep dialog open when Abort is pressed. */
static int
arcmc_progress_btn_callback (MC_UNUSED WButton *button, MC_UNUSED int action)
{
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* Poll events and check for abort. Returns TRUE to continue, FALSE if aborted. */
static gboolean
arcmc_progress_check_buttons (arcmc_progress_t *p)
{
    int c;
    Gpm_Event event;

    if (p == NULL || p->aborted || !p->visible)
        return (p == NULL || !p->aborted);

    event.x = -1;
    c = tty_get_event (&event, FALSE, FALSE);

    if (c == EV_NONE)
        return TRUE;

    p->dlg->ret_value = 0;
    dlg_process_event (p->dlg, c, &event);

    if (p->dlg->ret_value == B_CANCEL)
    {
        if (query_dialog (_ ("Arcmc"), _ ("Abort current operation?"), D_NORMAL, 2, _ ("&Yes"),
                          _ ("&No"))
            == 0)
        {
            p->aborted = TRUE;
            return FALSE;
        }

        /* user chose No — continue */
        p->dlg->ret_value = 0;
    }

    return TRUE;
}

/*** public functions ****************************************************************************/

/* Create progress dialog. Does not show it yet — display is deferred 1 second. */
arcmc_progress_t *
arcmc_progress_create (const char *title, const char *archive_path, off_t total_bytes)
{
    arcmc_progress_t *p;
    WGroup *g;
    WButton *abort_btn;
    int dlg_width = 64;
    int y = 2, x = 3;
    int gauge_width;
    int btn_width;

    p = g_new0 (arcmc_progress_t, 1);
    p->op_title = g_strdup (title);
    p->total_bytes = total_bytes;
    p->start_time = g_get_monotonic_time ();
    p->aborted = FALSE;
    p->visible = FALSE;
    p->last_total_col = -1;
    p->last_file_col = -1;

    gauge_width = dlg_width - 2 * x;
    p->gauge_cols = gauge_width - 7; /* visual bar length (gauge reserves 7 chars for "] NNN%") */

    p->dlg = dlg_create (TRUE, 0, 0, 15, dlg_width, WPOS_CENTER, FALSE, dialog_colors,
                         arcmc_progress_dlg_callback, NULL, NULL, title);
    g = GROUP (p->dlg);

    /* archive path */
    p->lbl_archive = label_new (y++, x, archive_path);
    group_add_widget (g, p->lbl_archive);

    /* separator with percentage text */
    p->hline_top = hline_new (y++, -1, -1);
    group_add_widget (g, p->hline_top);

    /* current file */
    p->lbl_file = label_new (y++, x, "");
    group_add_widget (g, p->lbl_file);

    /* file size */
    p->lbl_file_size = label_new (y++, x, "");
    group_add_widget (g, p->lbl_file_size);

    /* file gauge */
    p->gauge_file = gauge_new (y++, x, gauge_width, FALSE, 100, 0);
    group_add_widget_autopos (g, p->gauge_file, WPOS_KEEP_TOP | WPOS_KEEP_HORZ, NULL);

    /* separator */
    group_add_widget (g, hline_new (y++, -1, -1));

    /* total size + speed + ETA */
    p->lbl_total_size = label_new (y++, x, "");
    group_add_widget (g, p->lbl_total_size);

    /* ratio */
    p->lbl_ratio = label_new (y++, x, "");
    group_add_widget (g, p->lbl_ratio);

    /* total gauge */
    p->gauge_total = gauge_new (y++, x, gauge_width, FALSE, 100, 0);
    group_add_widget_autopos (g, p->gauge_total, WPOS_KEEP_TOP | WPOS_KEEP_HORZ, NULL);

    /* separator before button */
    group_add_widget (g, hline_new (y++, -1, -1));

    /* Abort button — centered */
    abort_btn =
        button_new (y, 0, B_CANCEL, NORMAL_BUTTON, _ ("&Abort"), arcmc_progress_btn_callback);
    btn_width = button_get_width (abort_btn);
    WIDGET (abort_btn)->rect.x = (dlg_width - btn_width) / 2;
    group_add_widget (g, abort_btn);
    widget_select (WIDGET (abort_btn));

    return p;
}

/* --------------------------------------------------------------------------------------------- */

void
arcmc_progress_destroy (arcmc_progress_t *p)
{
    if (p == NULL)
        return;

    if (p->visible)
        dlg_run_done (p->dlg);

    widget_destroy (WIDGET (p->dlg));
    g_free (p->op_title);
    g_free (p);
}

/* --------------------------------------------------------------------------------------------- */

/* Update progress dialog labels and gauges.
   Returns TRUE to continue, FALSE if aborted. */
gboolean
arcmc_progress_update (arcmc_progress_t *p, const char *filename, off_t file_size, off_t file_done,
                       off_t total_done, off_t written)
{
    gint64 now;
    double elapsed;
    char buf_done[64], buf_total[64], buf_speed[64], buf_fdone[64], buf_ftotal[64];
    char buf_written[64];
    char line[256];

    if (p == NULL)
        return TRUE;
    if (p->aborted)
        return FALSE;

    int total_col, file_col;
    gboolean file_changed, total_changed;

    p->done_bytes = total_done;
    p->written_bytes = written;
    p->file_bytes = file_size;
    p->file_done = file_done;

    now = g_get_monotonic_time ();

    /* defer display for 1 second */
    if (!p->visible)
    {
        if (now - p->start_time < G_USEC_PER_SEC)
            return TRUE;

        dlg_init (p->dlg);
        p->visible = TRUE;
    }

    /* always check for abort (Esc / button) */
    if (!arcmc_progress_check_buttons (p))
        return FALSE;

    /* compute current column positions */
    total_col =
        (p->total_bytes > 0) ? (int) ((gint64) p->gauge_cols * total_done / p->total_bytes) : 0;
    file_col = (file_size > 0) ? (int) ((gint64) p->gauge_cols * file_done / file_size) : 0;

    file_changed = (file_col != p->last_file_col);
    total_changed = (total_col != p->last_total_col);

    if (!file_changed && !total_changed)
        return TRUE;

    /* --- file section: update only when file bar advances --- */
    if (file_changed)
    {
        p->last_file_col = file_col;

        if (filename != NULL)
            label_set_text (p->lbl_file, filename);

        if (file_size > 0)
        {
            arcmc_format_size (file_done, buf_fdone, sizeof (buf_fdone));
            arcmc_format_size (file_size, buf_ftotal, sizeof (buf_ftotal));
            g_snprintf (line, sizeof (line), " %s / %s", buf_fdone, buf_ftotal);
            label_set_text (p->lbl_file_size, line);

            gauge_set_value (p->gauge_file, p->gauge_cols, file_col);
            gauge_show (p->gauge_file, TRUE);
        }
        else
            gauge_show (p->gauge_file, FALSE);
    }

    /* --- total section: update only when total bar advances --- */
    if (total_changed)
    {
        p->last_total_col = total_col;

        /* percent on the top hline — lightweight, no frame repaint */
        {
            int pct = 0;

            if (p->total_bytes > 0)
                pct = (int) (100 * total_done / p->total_bytes);

            hline_set_textv (p->hline_top, " {%d%%} %s ", pct, p->op_title);
        }

        /* total size + speed + ETA */
        elapsed = (double) (now - p->start_time) / G_USEC_PER_SEC;

        arcmc_format_size (total_done, buf_done, sizeof (buf_done));
        arcmc_format_size (p->total_bytes, buf_total, sizeof (buf_total));

        if (elapsed > 0.5)
        {
            double speed = (double) total_done / elapsed;
            double eta =
                (p->total_bytes > total_done) ? (double) (p->total_bytes - total_done) / speed : 0;
            int eta_min, eta_sec;

            arcmc_format_size ((off_t) speed, buf_speed, sizeof (buf_speed));
            eta_sec = (int) eta;
            eta_min = eta_sec / 60;
            eta_sec %= 60;

            g_snprintf (line, sizeof (line), " %s / %s @ %s/с -%02d:%02d:%02d", buf_done, buf_total,
                        buf_speed, eta_min / 60, eta_min % 60, eta_sec);
        }
        else
            g_snprintf (line, sizeof (line), " %s / %s", buf_done, buf_total);

        label_set_text (p->lbl_total_size, line);

        /* ratio line */
        if (written > 0 && total_done > 0)
        {
            int ratio = (int) (100 * written / total_done);

            arcmc_format_size (total_done, buf_done, sizeof (buf_done));
            arcmc_format_size (written, buf_written, sizeof (buf_written));
            g_snprintf (line, sizeof (line), " %s → %s = %d%%", buf_done, buf_written, ratio);
            label_set_text (p->lbl_ratio, line);
        }

        gauge_set_value (p->gauge_total, p->gauge_cols, total_col);
        gauge_show (p->gauge_total, TRUE);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
