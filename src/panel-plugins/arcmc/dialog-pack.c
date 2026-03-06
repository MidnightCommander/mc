/*
   Archive browser panel plugin — pack dialog UI.

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
#include "lib/widget.h"

#include "arcmc-types.h"
#include "dialog-pack.h"

/*** file scope variables ************************************************************************/

/* names for "Other" format selector (indices match ARCMC_FMT_TAR_GZ .. ARCMC_FMT_CPIO) */
static const char *const other_format_names[] = {
    "tar.gz", "tar.bz2", "tar.xz", "tar", "cpio",
};

#define OTHER_FMT_DISPLAY_LEN 10

/* extensions for each ARCMC_FMT_* value */
const char *const format_extensions[ARCMC_FMT_COUNT] = {
    ".zip",     /* ARCMC_FMT_ZIP */
    ".7z",      /* ARCMC_FMT_7Z */
    ".tar.gz",  /* ARCMC_FMT_TAR_GZ */
    ".tar.bz2", /* ARCMC_FMT_TAR_BZ2 */
    ".tar.xz",  /* ARCMC_FMT_TAR_XZ */
    ".tar",     /* ARCMC_FMT_TAR */
    ".cpio",    /* ARCMC_FMT_CPIO */
};

/* currently selected "Other" format index (0-based within other_format_names) */
static int current_other_fmt_idx = 0;

/* widget IDs for the pack dialog callback */
static unsigned long pack_fmt_radio_id = 0;
static unsigned long pack_path_input_id = 0;
static unsigned long pack_show_password_id = 0;
static unsigned long pack_password_input_id = 0;
static unsigned long pack_verify_input_id = 0;

/*** file scope functions ************************************************************************/

/* Replace the extension in the archive path input widget.
   `fmt` is an ARCMC_FMT_* value. */
static void
pack_update_extension (Widget *dlg_w, int fmt)
{
    Widget *path_w;
    WInput *path_input;
    const char *old_text;
    const char *ext;
    const char *dot;
    char *base;
    char *new_text;

    path_w = widget_find_by_id (dlg_w, pack_path_input_id);
    if (path_w == NULL)
        return;

    path_input = INPUT (path_w);
    old_text = path_input->buffer->str;
    ext = format_extensions[fmt];

    /* strip known extensions from the end */
    {
        size_t i;
        size_t old_len = strlen (old_text);

        base = g_strdup (old_text);

        for (i = 0; i < G_N_ELEMENTS (format_extensions); i++)
        {
            size_t ext_len = strlen (format_extensions[i]);

            if (old_len >= ext_len
                && g_ascii_strcasecmp (base + old_len - ext_len, format_extensions[i]) == 0)
            {
                base[old_len - ext_len] = '\0';
                break;
            }
        }
    }

    /* if no base name yet, use a dot-less placeholder */
    if (base[0] == '\0')
    {
        g_free (base);
        /* check if old text had at least a dot */
        dot = strrchr (old_text, '.');
        if (dot != NULL)
            base = g_strndup (old_text, (gsize) (dot - old_text));
        else
            base = g_strdup (old_text);
    }

    new_text = g_strconcat (base, ext, NULL);
    input_assign_text (path_input, new_text);
    g_free (new_text);
    g_free (base);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
pack_dlg_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_NOTIFY:
        /* format radio changed */
        if (sender != NULL && sender->id == pack_fmt_radio_id)
        {
            int sel = RADIO (sender)->sel;
            int fmt;

            switch (sel)
            {
            case 0:
                fmt = ARCMC_FMT_ZIP;
                break;
            case 1:
                fmt = ARCMC_FMT_7Z;
                break;
            case 2:
            default:
                fmt = ARCMC_FMT_TAR_GZ + current_other_fmt_idx;
                break;
            }

            pack_update_extension (w, fmt);
            return MSG_HANDLED;
        }

        /* show password checkbox toggled */
        if (sender != NULL && sender->id == pack_show_password_id)
        {
            gboolean shown = (CHECK (sender)->state != 0);
            Widget *pass_w, *verify_w;

            pass_w = widget_find_by_id (w, pack_password_input_id);
            if (pass_w != NULL)
            {
                INPUT (pass_w)->is_password = !shown;
                widget_draw (pass_w);
            }

            verify_w = widget_find_by_id (w, pack_verify_input_id);
            if (verify_w != NULL)
            {
                widget_disable (verify_w, shown);
            }
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
sel_other_format_button (WButton *button, int action)
{
    int result;
    WListbox *fmt_list;
    WDialog *fmt_dlg;
    int i;

    (void) action;

    {
        Widget *btn_w = WIDGET (button);
        int dlg_y = btn_w->rect.y;
        int dlg_x = btn_w->rect.x + btn_w->rect.cols + 1;

        fmt_dlg = dlg_create (TRUE, dlg_y, dlg_x, ARCMC_FMT_OTHER_COUNT + 2,
                              OTHER_FMT_DISPLAY_LEN + 4, WPOS_KEEP_DEFAULT, TRUE, dialog_colors,
                              NULL, NULL, "[arcmc]", _ ("Format"));
    }

    fmt_list = listbox_new (1, 1, ARCMC_FMT_OTHER_COUNT, OTHER_FMT_DISPLAY_LEN + 2, FALSE, NULL);

    for (i = 0; i < ARCMC_FMT_OTHER_COUNT; i++)
    {
        listbox_add_item (fmt_list, LISTBOX_APPEND_AT_END, 0, other_format_names[i], NULL, FALSE);
        if (i == current_other_fmt_idx)
            listbox_set_current (fmt_list, i);
    }

    group_add_widget_autopos (GROUP (fmt_dlg), fmt_list, WPOS_KEEP_ALL, NULL);

    result = dlg_run (fmt_dlg);
    if (result == B_ENTER)
    {
        Widget *pack_dlg_w;
        Widget *radio_w;

        current_other_fmt_idx = LISTBOX (fmt_list)->current;
        button_set_text (button,
                         str_fit_to_term (other_format_names[current_other_fmt_idx],
                                          OTHER_FMT_DISPLAY_LEN, J_LEFT_FIT));

        /* switch radio to "Other" and update extension */
        pack_dlg_w = WIDGET (WIDGET (button)->owner);
        radio_w = widget_find_by_id (pack_dlg_w, pack_fmt_radio_id);
        if (radio_w != NULL)
        {
            RADIO (radio_w)->sel = 2;
            widget_draw (radio_w);
        }
        pack_update_extension (pack_dlg_w, ARCMC_FMT_TAR_GZ + current_other_fmt_idx);
    }
    widget_destroy (WIDGET (fmt_dlg));

    return 0;
}

/*** public functions ****************************************************************************/

gboolean
arcmc_show_pack_dialog (arcmc_pack_opts_t *opts, const char *initial_path)
{
    /* *INDENT-OFF* */
    const char *format_options[] = {
        N_ ("&zip"),
        N_ ("&7z"),
        N_ ("O&ther:"),
    };
    const char *compression_options[] = {
        N_ ("St&ore"),
        N_ ("&Fastest"),
        N_ ("&Normal"),
        N_ ("&Maximum"),
    };
    /* *INDENT-ON* */

    const int format_options_num = G_N_ELEMENTS (format_options);
    const int compression_options_num = G_N_ELEMENTS (compression_options);

    char *archive_path = NULL;
    char *password = NULL;
    char *password_verify = NULL;
    int format_radio = 0; /* 0=zip, 1=7z, 2=other */
    int compression = 2;  /* Normal */
    int encrypt_files = 0;
    int encrypt_header = 0;
    int show_password = 0;
    int store_paths = 1;
    int delete_after = 0;
    int ret;

    /* *INDENT-OFF* */
    quick_widget_t quick_widgets[] = {
        QUICK_LABELED_INPUT (N_ ("Archive path:"), input_label_left, initial_path,
                             "arcmc-pack-path", &archive_path, &pack_path_input_id, FALSE, FALSE,
                             INPUT_COMPLETE_FILENAMES),
        QUICK_START_COLUMNS,
        QUICK_START_GROUPBOX (N_ ("Format")),
        QUICK_RADIO (format_options_num, format_options, &format_radio, &pack_fmt_radio_id),
        QUICK_BUTTON (str_fit_to_term (other_format_names[current_other_fmt_idx],
                                       OTHER_FMT_DISPLAY_LEN, J_LEFT_FIT),
                      B_USER, sel_other_format_button, NULL),
        QUICK_STOP_GROUPBOX,
        QUICK_NEXT_COLUMN,
        QUICK_START_GROUPBOX (N_ ("Compression")),
        QUICK_RADIO (compression_options_num, compression_options, &compression, NULL),
        QUICK_STOP_GROUPBOX,
        QUICK_STOP_COLUMNS,
        QUICK_START_GROUPBOX (N_ ("Encryption")),
        QUICK_START_COLUMNS,
        QUICK_CHECKBOX (N_ ("Encrypt &files"), &encrypt_files, NULL),
        QUICK_NEXT_COLUMN,
        QUICK_CHECKBOX (N_ ("Encrypt hea&ders"), &encrypt_header, NULL),
        QUICK_STOP_COLUMNS,
        QUICK_CHECKBOX (N_ ("Show pass&word"), &show_password, &pack_show_password_id),
        QUICK_START_COLUMNS,
        QUICK_LABELED_INPUT (N_ ("Password:"), input_label_above, "", "arcmc-pack-pass", &password,
                             &pack_password_input_id, TRUE, TRUE, INPUT_COMPLETE_NONE),
        QUICK_NEXT_COLUMN,
        QUICK_LABELED_INPUT (N_ ("Verify password:"), input_label_above, "",
                             "arcmc-pack-pass-verify", &password_verify, &pack_verify_input_id,
                             TRUE, TRUE, INPUT_COMPLETE_NONE),
        QUICK_STOP_COLUMNS,
        QUICK_STOP_GROUPBOX,
        QUICK_START_GROUPBOX (N_ ("Options")),
        QUICK_CHECKBOX (N_ ("Store relative &paths"), &store_paths, NULL),
        QUICK_CHECKBOX (N_ ("&Delete files after archiving"), &delete_after, NULL),
        QUICK_STOP_GROUPBOX,
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };
    /* *INDENT-ON* */

    WRect r = { -1, -1, 0, 60 };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = N_ ("Create archive"),
        .help = "[arcmc]",
        .widgets = quick_widgets,
        .callback = pack_dlg_callback,
        .mouse_callback = NULL,
    };

    ret = quick_dialog (&qdlg);

    if (ret == B_ENTER)
    {
        /* verify passwords match */
        if (password != NULL && password[0] != '\0')
        {
            if (password_verify == NULL || strcmp (password, password_verify) != 0)
            {
                message (D_ERROR, MSG_ERROR, "%s", _ ("Passwords do not match"));
                g_free (archive_path);
                g_free (password);
                g_free (password_verify);
                return FALSE;
            }
        }

        opts->archive_path = archive_path;
        opts->compression = compression;
        opts->encrypt_files = (encrypt_files != 0);
        opts->encrypt_header = (encrypt_header != 0);
        opts->store_paths = (store_paths != 0);
        opts->delete_after = (delete_after != 0);

        /* map radio selection to ARCMC_FMT_* */
        switch (format_radio)
        {
        case 0:
            opts->format = ARCMC_FMT_ZIP;
            break;
        case 1:
            opts->format = ARCMC_FMT_7Z;
            break;
        case 2:
        default:
            opts->format = ARCMC_FMT_TAR_GZ + current_other_fmt_idx;
            break;
        }

        if (password != NULL && password[0] != '\0')
            opts->password = password;
        else
        {
            opts->password = NULL;
            g_free (password);
        }

        g_free (password_verify);
        return TRUE;
    }

    g_free (archive_path);
    g_free (password);
    g_free (password_verify);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
