/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* New dialogs... */
#include <config.h>
#include "panel.h"
#include <gnome.h>
#include "dialog.h"
#include "global.h"
#include "file.h"
#include "filegui.h"
#include "fileopctx.h"
#include "eregex.h"
#include "main.h"
#include "../vfs/vfs.h"

enum {
        REPLACE_PROMPT,
        REPLACE_ALWAYS,
        REPLACE_UPDATE,
        REPLACE_NEVER,
        REPLACE_ABORT,
        REPLACE_SIZE,
        REPLACE_OPTION_MENU
} FileReplaceCode;

/* This structure describes the UI and internal data required by a file
 * operation context.
 */
typedef struct {
        /* The progress window */
        GtkWidget *op_win;

	/* Set to FALSE in file_op_context_create_ui, set on the cancel_cb if
         * user click on Cancel.
         */
        gboolean aborting;

        /* Source file label */
        GtkWidget *op_source_label;

        /* Target file label */
        GtkWidget *op_target_label;

        /* File number label */
        GtkObject *count_label;

        /* Current file label */
        GtkWidget *file_label;

        /* Bytes progress bar */
        GtkObject *byte_prog;

        /* Copy status in query replace dialog */
        int copy_status;
        int minor_copy_status;

        /* Overwrite toggle */
        GtkWidget *op_radio;
} FileOpContextUI;

static char *gdialog_to_string = N_("To: ");
static char *gdialog_from_string = N_("Copying from: ");
static char *gdialog_deleting_string = N_("Deleting file: ");

#define GDIALOG_PROGRESS_WIDTH 350

/* Callbacks go here... */
static void
fmd_check_box_callback (GtkWidget *widget, gpointer data)
{
        if (data)
                *((gint*)data) = GTK_TOGGLE_BUTTON (widget)->active;
}

static gchar *
trim_file_name (FileOpContextUI *ui, gchar *path, gint length, gint cur_length)
{
        static gint dotdotdot = 0;
        gchar *path_copy = NULL;
        gint len;

        if (!dotdotdot)
                dotdotdot = gdk_string_width (ui->op_source_label->style->font, "...");
        /* Cut the font length of path to length. */

        length -= dotdotdot;
        len = (gint) ((1.0 - (gfloat) length / (gfloat) cur_length) * strlen (path));

        /* we guess a starting point */
        if (gdk_string_width (ui->op_source_label->style->font, path + len) < length) {
                while (gdk_string_width (ui->op_source_label->style->font, path + len) < length)
                        len--;
                len++;
        } else {
                while (gdk_string_width (ui->op_source_label->style->font, path + len) > length)
                        len++;
        }

        path_copy = g_strdup_printf ("...%s", path + len);
        return path_copy;
}

FileProgressStatus
file_progress_show_source (FileOpContext *ctx, char *path)
{
        static gint from_width = 0;
        FileOpContextUI *ui;
        gint path_width;
        gchar *path_copy = NULL;

        g_return_val_if_fail (ctx != NULL, FILE_CONT);

        /* ctx->ui might be NULL for background processes */
        if (ctx->ui == NULL)
                return FILE_CONT;

        ui = ctx->ui;

        g_return_val_if_fail (ui->op_source_label != NULL, FILE_CONT);

        if (ui->aborting)
                return FILE_ABORT;

        if (path == NULL){
                gtk_label_set_text (GTK_LABEL (ui->op_source_label), "");
                return FILE_CONT;
        }

        if (!from_width){
                from_width = gdk_string_width (ui->op_source_label->style->font,
                                               _(gdialog_from_string));
        }
        path_width = gdk_string_width (ui->op_source_label->style->font, path);
        if (from_width + path_width < GDIALOG_PROGRESS_WIDTH)
                gtk_label_set_text (GTK_LABEL (ui->op_source_label), path);
        else {
                path_copy = trim_file_name (ui, path, GDIALOG_PROGRESS_WIDTH - from_width,
                                            path_width);

                gtk_label_set_text (GTK_LABEL (ui->op_source_label), path_copy);
                g_free (path_copy);
        }

	return FILE_CONT;
}

FileProgressStatus
file_progress_show_target (FileOpContext *ctx, char *path)
{
        static gint to_width = 0;
        FileOpContextUI *ui;
        gint path_width;
        gchar *path_copy = NULL;

        g_return_val_if_fail (ctx != NULL, FILE_CONT);
        /* ctx->ui might be NULL for background processes */
        if (ctx->ui == NULL)
                return FILE_CONT;

        ui = ctx->ui;

        g_return_val_if_fail (ui->op_target_label != NULL, FILE_CONT);

        if (ui->aborting)
                return FILE_ABORT;

        if (path == NULL){
                gtk_label_set_text (GTK_LABEL (ui->op_target_label), "");
                return FILE_CONT;
        }

        if (!to_width)
                to_width = gdk_string_width (ui->op_target_label->style->font,
                                             _(gdialog_to_string));
        path_width = gdk_string_width (ui->op_target_label->style->font, path);
        if (to_width + path_width < GDIALOG_PROGRESS_WIDTH)
                gtk_label_set_text (GTK_LABEL (ui->op_target_label), path);
        else {
                path_copy = trim_file_name (ui, path, GDIALOG_PROGRESS_WIDTH - to_width, path_width);
                gtk_label_set_text (GTK_LABEL (ui->op_target_label), path_copy);
                g_free (path_copy);
        }

	return FILE_CONT;
}

FileProgressStatus
file_progress_show_deleting (FileOpContext *ctx, char *path)
{
        static gint deleting_width = 0;
        FileOpContextUI *ui;
        gint path_width;
        gchar *path_copy = NULL;

        g_return_val_if_fail (ctx != NULL, FILE_CONT);

        /* ctx->ui might be NULL for background processes */
        if (ctx->ui == NULL)
                return FILE_CONT;

        ui = ctx->ui;

        if (ui->aborting)
                return FILE_ABORT;

        if (path == NULL){
                gtk_label_set_text (GTK_LABEL (ui->op_source_label), "");
                return FILE_CONT;
        }

        if (!deleting_width){
                deleting_width = gdk_string_width (ui->op_source_label->style->font,
                                                   _(gdialog_deleting_string));
        }
        path_width = gdk_string_width (ui->op_source_label->style->font, path);
        if (deleting_width + path_width < GDIALOG_PROGRESS_WIDTH)
                gtk_label_set_text (GTK_LABEL (ui->op_source_label), path);
        else {
                path_copy = trim_file_name (ui, path, GDIALOG_PROGRESS_WIDTH - deleting_width,
                                            path_width);

                gtk_label_set_text (GTK_LABEL (ui->op_source_label), path_copy);
                g_free (path_copy);
        }
        return FILE_CONT;
}

FileProgressStatus
file_progress_show (FileOpContext *ctx, long done, long total)
{
        static gchar count[20];
        FileOpContextUI *ui;
        double perc;

        g_return_val_if_fail (ctx != NULL, FILE_CONT);

        /* ctx->ui might be NULL for background processes */
        if (ctx->ui == NULL)
                return FILE_CONT;

        ui = ctx->ui;

        if (ui->aborting)
                return FILE_ABORT;

        if (total > 0) {
                perc = (double) done / (double) total;
                snprintf (count, 9, "%3d%% ", (gint) (100.0 * perc));
                gtk_label_set_text (GTK_LABEL (ui->file_label), count);
        }
        while (gtk_events_pending ())
                gtk_main_iteration ();
	return FILE_CONT;
}

FileProgressStatus
file_progress_show_count (FileOpContext *ctx, long done, long total)
{
        static gchar count[100];
        FileOpContextUI *ui;

        g_return_val_if_fail (ctx != NULL, FILE_CONT);

        /* ctx->ui might be NULL for background processes */
        if (ctx->ui == NULL)
                return FILE_CONT;

        ui = ctx->ui;

        if (ui->aborting)
                return FILE_ABORT;

        snprintf (count, 100, "%ld/%ld ", done, total);
        gtk_label_set_text (GTK_LABEL (ui->count_label), count);
        while (gtk_events_pending ())
                gtk_main_iteration ();
	return FILE_CONT;
}

FileProgressStatus
file_progress_show_bytes (FileOpContext *ctx, double done, double total)
{
        FileOpContextUI *ui;
        double perc;

        g_return_val_if_fail (ctx != NULL, FILE_CONT);

        /* ctx->ui might be NULL for background processes */
        if (ctx->ui == NULL)
                return FILE_CONT;

        ui = ctx->ui;

        if (ui->aborting)
                return FILE_ABORT;

        if (total == 0.0)
                perc = 1.0;
        else
                perc = done / total;

        gtk_progress_bar_update (GTK_PROGRESS_BAR (ui->byte_prog), CLAMP (perc, 0.0, 1.0));

        while (gtk_events_pending ())
                gtk_main_iteration ();
	return FILE_CONT;
}

static void
option_menu_policy_callback (GtkWidget *item, gpointer data)
{
        FileOpContextUI *ui;
        int status;

        ui = data;
        status = GPOINTER_TO_INT (gtk_object_get_user_data (GTK_OBJECT (item)));

        ui->minor_copy_status = status;
        ui->copy_status = status;
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ui->op_radio), TRUE);
}

static void
policy_callback (GtkWidget *button, gpointer data)
{
        FileOpContextUI *ui;
        int status;

        ui = data;
        status = GPOINTER_TO_INT (gtk_object_get_user_data (GTK_OBJECT (button)));

        if (GTK_TOGGLE_BUTTON (button)->active) {
                if (status == REPLACE_OPTION_MENU) {
                        ui->copy_status = ui->minor_copy_status;
                } else
                        ui->copy_status = status;
        }
}

FileProgressStatus
file_progress_query_replace_policy (FileOpContext *ctx, gboolean dialog_needed)
{
        FileOpContextUI *ui;
        GtkWidget *qrp_dlg;
        GtkWidget *radio;
        GtkWidget *vbox;
        GtkWidget *vbox2;
        GtkWidget *hbox;
        GtkWidget *icon;
        GtkWidget *label;
        GtkWidget *hrbox;
        GSList *group = NULL;
        GtkWidget *omenu;
        GtkWidget *menu;
        GtkWidget *menu_item;

        g_return_val_if_fail (ctx != NULL, FILE_CONT);

        /* ctx->ui might be NULL for background processes */
        if (ctx->ui == NULL)
                return FILE_CONT;

        ui = ctx->ui;

        if (ui->aborting)
                return FILE_ABORT;

        ui->copy_status = REPLACE_PROMPT;
        if (dialog_needed == FALSE)
                return FILE_CONT;
        ui->minor_copy_status = REPLACE_ALWAYS;
        qrp_dlg = gnome_dialog_new (_("Files Exist"),
                                    GNOME_STOCK_BUTTON_OK,
                                    GNOME_STOCK_BUTTON_CANCEL,
                                    NULL);
        gtk_window_set_position (GTK_WINDOW (qrp_dlg), GTK_WIN_POS_MOUSE);

        hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (qrp_dlg)->vbox), hbox, FALSE, FALSE, 0);

        icon = gnome_stock_pixmap_widget (hbox, GNOME_STOCK_PIXMAP_HELP);
        gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);

        vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, GNOME_PAD_SMALL);

        label = gtk_label_new (_("Some of the files you are trying to copy already "
                                 "exist in the destination folder.  Please select "
                                 "the action to be performed."));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, FALSE, 0);

        vbox2 = gtk_vbox_new (TRUE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), vbox2, FALSE, FALSE, 0);

        radio = gtk_radio_button_new_with_label (group, _("Prompt me before overwriting any file."));
        gtk_object_set_user_data (GTK_OBJECT (radio), GINT_TO_POINTER (REPLACE_PROMPT));
        gtk_signal_connect (GTK_OBJECT (radio), "toggled",
                            GTK_SIGNAL_FUNC (policy_callback), ui);
        gtk_box_pack_start (GTK_BOX (vbox2), radio, FALSE, FALSE, 0);
        group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));

        radio = gtk_radio_button_new_with_label (group, _("Don't overwrite any files."));
        gtk_object_set_user_data (GTK_OBJECT (radio), GINT_TO_POINTER (REPLACE_NEVER));
        gtk_signal_connect (GTK_OBJECT (radio), "toggled",
                            GTK_SIGNAL_FUNC (policy_callback), ui);
        gtk_box_pack_start (GTK_BOX (vbox2), radio, FALSE, FALSE, 0);
        group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));

        ui->op_radio = gtk_radio_button_new (group);
        gtk_object_set_user_data (GTK_OBJECT (ui->op_radio), GINT_TO_POINTER (REPLACE_OPTION_MENU));
        gtk_signal_connect (GTK_OBJECT (ui->op_radio), "toggled",
                            GTK_SIGNAL_FUNC (policy_callback), ui);
        gtk_box_pack_start (GTK_BOX (vbox2), ui->op_radio, FALSE, FALSE, 0);

        hrbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_container_add (GTK_CONTAINER (ui->op_radio), hrbox);

        gtk_box_pack_start (GTK_BOX (hrbox), gtk_label_new (_("Overwrite:")), FALSE, FALSE, 0);

        /* we set up the option menu. */
        omenu = gtk_option_menu_new ();
        gtk_box_pack_start (GTK_BOX (hrbox), omenu, FALSE, FALSE, 0);
        menu = gtk_menu_new ();

        menu_item = gtk_menu_item_new_with_label ( _("Older files."));
        gtk_menu_append (GTK_MENU (menu), menu_item);
        gtk_object_set_user_data (GTK_OBJECT (menu_item), GINT_TO_POINTER (REPLACE_UPDATE));
        gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                            GTK_SIGNAL_FUNC (option_menu_policy_callback), ui);

        menu_item = gtk_menu_item_new_with_label ( _("Files only if size differs."));
        gtk_menu_append (GTK_MENU (menu), menu_item);
        gtk_object_set_user_data (GTK_OBJECT (menu_item), GINT_TO_POINTER (REPLACE_SIZE));
        gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                            GTK_SIGNAL_FUNC (option_menu_policy_callback), ui);

        menu_item = gtk_menu_item_new_with_label ( _("All files."));
        gtk_menu_append (GTK_MENU (menu), menu_item);
        gtk_object_set_user_data (GTK_OBJECT (menu_item), GINT_TO_POINTER (REPLACE_ALWAYS));
        gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                            GTK_SIGNAL_FUNC (option_menu_policy_callback), ui);

        gtk_widget_show_all (menu);
        gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);

        gtk_widget_show_all (GTK_WIDGET (GNOME_DIALOG (qrp_dlg)->vbox));
        switch (gnome_dialog_run_and_close (GNOME_DIALOG (qrp_dlg))) {
        case 0:
                break;
        case -1:
        default:
                ui->copy_status = REPLACE_ABORT;
                return FILE_ABORT;
        }
        return FILE_CONT;
}

FileProgressStatus
file_progress_real_query_replace (FileOpContext *ctx, enum OperationMode mode, char *destname,
                                  struct stat *_s_stat, struct stat *_d_stat)
{
        FileOpContextUI *ui;
        GtkWidget *qr_dlg;
        gchar msg[128];
        GtkWidget *label;

        g_return_val_if_fail (ctx != NULL, FILE_CONT);
        g_return_val_if_fail (ctx->ui != NULL, FILE_CONT);

        ui = ctx->ui;

        if (ui->aborting)
                return FILE_ABORT;

        /* so what's the situation?  Do we prompt or don't we prompt. */
        if (ui->copy_status == REPLACE_PROMPT){
                qr_dlg = gnome_dialog_new (_("File Exists"),
                                           GNOME_STOCK_BUTTON_YES,
                                           GNOME_STOCK_BUTTON_NO,
                                           GNOME_STOCK_BUTTON_CANCEL, NULL);

                gtk_window_set_position (GTK_WINDOW (qr_dlg), GTK_WIN_POS_MOUSE);
                snprintf (msg, sizeof (msg)-1, _("The target file already exists: %s"), destname);
                label = gtk_label_new (msg);
                gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
                gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
                gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
                gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (qr_dlg)->vbox),
                                    label, FALSE, FALSE, 0);
                label = gtk_label_new (_("Replace it?"));
                gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
                gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
                gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (qr_dlg)->vbox),
                                    label, FALSE, FALSE, 0);
                gtk_widget_show_all (GNOME_DIALOG (qr_dlg)->vbox);
                switch (gnome_dialog_run_and_close (GNOME_DIALOG (qr_dlg))) {
                case 0:
                        return FILE_CONT;
                case 1:
                        return FILE_SKIP;
                default:
                        return FILE_ABORT;
                }
        }

        switch (ui->copy_status){
        case REPLACE_UPDATE:
                if (_s_stat->st_mtime > _d_stat->st_mtime)
                        return FILE_CONT;
                else
                        return FILE_SKIP;
        case REPLACE_SIZE:
                if (_s_stat->st_size == _d_stat->st_size)
                        return FILE_SKIP;
                else
                        return FILE_CONT;
        case REPLACE_ALWAYS:
                return FILE_CONT;
        case REPLACE_NEVER:
                return FILE_SKIP;
        case REPLACE_ABORT:
        default:
                return FILE_ABORT;
        }
}

void
file_progress_set_stalled_label (FileOpContext *ctx, char *stalled_msg)
{
        g_return_if_fail (ctx != NULL);

        if (ctx->ui == NULL)
                return;

        if (!stalled_msg || !*stalled_msg)
                return;
        /* FIXME */
	g_warning ("FIXME: file_progress_set_stalled_label!\nmsg\t%s\n",stalled_msg);
}

char *
file_mask_dialog (FileOpContext *ctx, FileOperation operation, char *text, char *def_text,
                  int only_one, int *do_background)
{
        GtkWidget *fmd_win;
        GtkWidget *notebook;
        GtkWidget *hbox;
        GtkWidget *vbox, *label;
        GtkWidget *fentry;
        GtkWidget *cbox;
        GtkWidget *icon;
        int source_easy_patterns = easy_patterns;
        char *source_mask, *orig_mask, *dest_dir;
        const char *error;
        struct stat buf;
        int run;

        g_return_val_if_fail (ctx != NULL, NULL);

        ctx->stable_symlinks = 0;

        /* Basic window */
        if (operation == OP_COPY)
                fmd_win = gnome_dialog_new (_("Copy"), GNOME_STOCK_BUTTON_OK,
                                            GNOME_STOCK_BUTTON_CANCEL, NULL);
        else if (operation == OP_MOVE)
                fmd_win = gnome_dialog_new (_("Move"), GNOME_STOCK_BUTTON_OK,
                                            GNOME_STOCK_BUTTON_CANCEL, NULL);

        gtk_window_set_position (GTK_WINDOW (fmd_win), GTK_WIN_POS_MOUSE);

        notebook = gtk_notebook_new ();
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (fmd_win)->vbox),
                            notebook, FALSE, FALSE, 0);

        hbox = gtk_hbox_new (FALSE, GNOME_PAD);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                                  hbox,
                                  gtk_label_new (_("Destination")));

        /* FIXME: I want a bigger, badder, better Icon here... */
        icon = gnome_stock_pixmap_widget (hbox, GNOME_STOCK_PIXMAP_HELP);
        gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);

        vbox = gtk_vbox_new (FALSE, GNOME_PAD);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD);

        label = gtk_label_new (text);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

        fentry = gnome_file_entry_new ("gmc-copy-file", _("Find Destination Folder"));
        gnome_file_entry_set_directory (GNOME_FILE_ENTRY (fentry), TRUE);
        gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (fentry))),
                            def_text);
        gnome_file_entry_set_default_path (GNOME_FILE_ENTRY (fentry), def_text);
        gnome_file_entry_set_modal (GNOME_FILE_ENTRY (fentry), TRUE);
        gtk_box_pack_start (GTK_BOX (vbox), fentry, FALSE, FALSE, 0);

        /* Background operations are completely hosed, so we olympically disable
         * them.  How's that for foolproof bugfixing.
         */

        *do_background = FALSE;
#if 0
        cbox = gtk_check_button_new_with_label (_("Copy as a background process"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cbox), *do_background);
        gtk_signal_connect (GTK_OBJECT (cbox), "toggled",
                            (GtkSignalFunc) fmd_check_box_callback, do_background);
#if 0
        gnome_widget_add_help (cbox, "Selecting this will run the copying in the background.  "
                               "This is useful for transfers over networks that might take a long "
                               "time to complete.");
#endif
        gtk_box_pack_end (GTK_BOX (vbox), cbox, FALSE, FALSE, 0);
        gtk_box_pack_end (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, FALSE, 0);
#endif

        /* Advanced Options */
        hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
        vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                                  hbox,
                                  gtk_label_new (_("Advanced Options")));
        gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

        cbox = gtk_check_button_new_with_label (_("Preserve symlinks"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cbox), ctx->stable_symlinks);
        gtk_signal_connect (GTK_OBJECT (cbox), "toggled",
                            (GtkSignalFunc) fmd_check_box_callback, &ctx->stable_symlinks);
#if 0
        gnome_widget_add_help (cbox, "FIXME: Add something here Miguel");
#endif
        gtk_box_pack_start (GTK_BOX (vbox), cbox, FALSE, FALSE, 0);

        if (operation == OP_COPY) {
                cbox = gtk_check_button_new_with_label (_("Follow links."));
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cbox), ctx->follow_links);
                gtk_signal_connect (GTK_OBJECT (cbox), "toggled",
                                    (GtkSignalFunc) fmd_check_box_callback, &ctx->follow_links);
#if 0
                gnome_widget_add_help (cbox,
                                       _("Selecting this will copy the files that symlinks point "
                                         "to instead of just copying the link."));
#endif
                gtk_box_pack_start (GTK_BOX (vbox), cbox, FALSE, FALSE, 0);

                cbox = gtk_check_button_new_with_label (_("Preserve file attributes."));
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cbox), ctx->op_preserve);
                gtk_signal_connect (GTK_OBJECT (cbox), "toggled",
                                    (GtkSignalFunc) fmd_check_box_callback, &ctx->op_preserve);
#if 0
                gnome_widget_add_help (cbox,
                                       _("Preserves the permissions and the UID/GID if possible"));
#endif
                gtk_box_pack_start (GTK_BOX (vbox), cbox, FALSE, FALSE, 0);

                vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
                gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
                cbox = gtk_check_button_new_with_label (_("Recursively copy subdirectories."));
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cbox), ctx->dive_into_subdirs);
                gtk_signal_connect (GTK_OBJECT (cbox), "toggled",
                                    (GtkSignalFunc) fmd_check_box_callback,
                                    &ctx->dive_into_subdirs);
#if 0
                gnome_widget_add_help (cbox,
                                       _("If set, this will copy the directories recursively"));
#endif
                gtk_box_pack_start (GTK_BOX (vbox), cbox, FALSE, FALSE, 0);
        }

        gtk_widget_show_all (GNOME_DIALOG (fmd_win)->vbox);
        gnome_dialog_set_close (GNOME_DIALOG (fmd_win), TRUE);
        gnome_dialog_close_hides (GNOME_DIALOG (fmd_win), TRUE);

        /* Off to the races!!! */
        run = gnome_dialog_run (GNOME_DIALOG (fmd_win));

        if (run == 1 || run == -1) {
                gtk_widget_destroy (fmd_win);
                return NULL;
        }

        dest_dir = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (fentry), FALSE);
        gtk_widget_destroy (fmd_win);

        easy_patterns = 1;
        if (!dest_dir || !*dest_dir)
                return NULL;

        if (ctx->follow_links && operation != OP_MOVE)
                ctx->stat_func = mc_stat;
        else
                ctx->stat_func = mc_lstat;

        if (ctx->op_preserve || operation == OP_MOVE){
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
        source_mask = g_strdup ("*");
        orig_mask = source_mask;
        if (!dest_dir || !*dest_dir){
                if (source_mask)
                        g_free (source_mask);
                return dest_dir;
        }

        if (!dest_dir)
                return NULL;

        if (!*dest_dir) {
                g_free (dest_dir);
                return NULL;
        }

        if (source_easy_patterns) {
                source_easy_patterns = easy_patterns;
                easy_patterns = 1;
                source_mask = convert_pattern (source_mask, match_file, 1);
                easy_patterns = source_easy_patterns;
                error = re_compile_pattern (source_mask, strlen (source_mask), &ctx->rx);
                g_free (source_mask);
        } else
                error = re_compile_pattern (source_mask, strlen (source_mask), &ctx->rx);

        if (error)
                g_warning ("%s\n",error);

        if (orig_mask)
                g_free (orig_mask);
        ctx->dest_mask = strrchr (dest_dir, PATH_SEP);
        if (ctx->dest_mask == NULL)
                ctx->dest_mask = dest_dir;
        else
                ctx->dest_mask++;
        orig_mask = ctx->dest_mask;
        if (!*ctx->dest_mask
            || (!ctx->dive_into_subdirs && !is_wildcarded (ctx->dest_mask)
                && (!only_one || (!mc_stat (dest_dir, &buf)
                                  && S_ISDIR (buf.st_mode))))
            || (ctx->dive_into_subdirs && ((!only_one && !is_wildcarded (ctx->dest_mask))
                                           || (only_one && !mc_stat (dest_dir, &buf)
                                               && S_ISDIR (buf.st_mode)))))
                ctx->dest_mask = g_strdup ("*");
        else {
                ctx->dest_mask = g_strdup (ctx->dest_mask);
                *orig_mask = 0;
        }
        if (!*dest_dir){
                g_free (dest_dir);
                dest_dir = g_strdup ("./");
        }
        return dest_dir;
}

int
file_delete_query_recursive (FileOpContext *ctx, enum OperationMode mode, gchar *s)
{
        GtkWidget *dialog;
        GtkWidget *togglebutton;
        gchar *title;
        gchar *msg;
        gint button;
        gboolean rest_same;

        if (ctx->recursive_result < RECURSIVE_ALWAYS) {
                msg = g_strdup_printf (_("%s\n\nDirectory not empty. Delete it recursively?"),
                                       name_trunc (s, 80));
                dialog = gnome_message_box_new (msg,
                                                GNOME_MESSAGE_BOX_QUESTION,
                                                GNOME_STOCK_BUTTON_YES,
                                                GNOME_STOCK_BUTTON_NO,
                                                GNOME_STOCK_BUTTON_CANCEL,
                                                NULL);
                g_free (msg);

                title = g_strconcat (_(" Delete: "), name_trunc (s, 30), " ", NULL);
                gtk_window_set_title (GTK_WINDOW (dialog), title);
                g_free (title);

                togglebutton = gtk_check_button_new_with_label (_("Do the same for the rest"));
                gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
                                    togglebutton, FALSE, FALSE, 0);
                gtk_widget_show_all (GNOME_DIALOG (dialog)->vbox);

                gnome_dialog_close_hides (GNOME_DIALOG (dialog), TRUE);

                button = gnome_dialog_run (GNOME_DIALOG (dialog));
                rest_same = GTK_TOGGLE_BUTTON (togglebutton)->active;

                gtk_widget_destroy (dialog);

                switch (button) {
                case 0:
                        ctx->recursive_result = rest_same ? RECURSIVE_ALWAYS : RECURSIVE_YES;
                        break;
                case 1:
                        ctx->recursive_result = rest_same ? RECURSIVE_NEVER : RECURSIVE_NO;
                        break;
                case 2:
                        ctx->recursive_result = RECURSIVE_ABORT;
                        break;
                default:
                }

                if (ctx->recursive_result != RECURSIVE_ABORT)
                        do_refresh ();
        }

        switch (ctx->recursive_result){
        case RECURSIVE_YES:
        case RECURSIVE_ALWAYS:
                return FILE_CONT;

        case RECURSIVE_NO:
        case RECURSIVE_NEVER:
                return FILE_SKIP;

        case RECURSIVE_ABORT:

        default:
                return FILE_ABORT;
        }
}

static void
cancel_cb (GtkWidget *widget, gpointer data)
{
        FileOpContextUI *ui;

        ui = data;
        ui->aborting = TRUE;
}

/* Handler for the close signal from the GnomeDialog in the file operation
 * context UI.  We mark the operation as "aborted" and ask GnomeDialog not to
 * close the window for us.
 */
static gboolean
close_cb (GtkWidget *widget, gpointer data)
{
        FileOpContextUI *ui;

        ui = data;
        ui->aborting = TRUE;
        return TRUE;
}

void
file_op_context_create_ui (FileOpContext *ctx, FileOperation op, int with_eta)
{
        FileOpContextUI *ui;
        GtkWidget *alignment;
        GtkWidget *hbox;

        g_return_if_fail (ctx != NULL);
        g_return_if_fail (ctx->ui == NULL);

        ui = g_new0 (FileOpContextUI, 1);
        ctx->ui = ui;

        switch (op) {
        case OP_MOVE:
                ui->op_win = gnome_dialog_new (_("Move Progress"), GNOME_STOCK_BUTTON_CANCEL, NULL);
                break;
        case OP_COPY:
                ui->op_win = gnome_dialog_new (_("Copy Progress"), GNOME_STOCK_BUTTON_CANCEL, NULL);
                break;
        case OP_DELETE:
                ui->op_win = gnome_dialog_new (_("Delete Progress"), GNOME_STOCK_BUTTON_CANCEL, NULL);
                break;
        }
        gtk_window_set_position (GTK_WINDOW (ui->op_win), GTK_WIN_POS_MOUSE);

        gnome_dialog_button_connect (GNOME_DIALOG (ui->op_win), 0,
                                     GTK_SIGNAL_FUNC (cancel_cb),
                                     ui);

        gtk_signal_connect (GTK_OBJECT (ui->op_win), "close",
                            GTK_SIGNAL_FUNC (close_cb),
                            ui);

        if (op != OP_DELETE) {
                alignment = gtk_alignment_new (0.0, 0.5, 0, 0);
                hbox = gtk_hbox_new (FALSE, 0);
                gtk_container_add (GTK_CONTAINER (alignment), hbox);
                gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_(gdialog_from_string)),
                                    FALSE, FALSE, 0);
                ui->op_source_label = gtk_label_new ("");

                gtk_box_pack_start (GTK_BOX (hbox), ui->op_source_label, FALSE, FALSE, 0);
                gtk_box_set_spacing (GTK_BOX (GNOME_DIALOG (ui->op_win)->vbox), GNOME_PAD_SMALL);
                gtk_container_set_border_width (GTK_CONTAINER (GNOME_DIALOG (ui->op_win)->vbox),
                                                GNOME_PAD);
                gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ui->op_win)->vbox),
                                    alignment, FALSE, FALSE, 0);

                alignment = gtk_alignment_new (0.0, 0.5, 0, 0);
                hbox = gtk_hbox_new (FALSE, 0);
                gtk_container_add (GTK_CONTAINER (alignment), hbox);
                gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_(gdialog_to_string)),
                                    FALSE, FALSE, 0);
                ui->op_target_label = gtk_label_new ("");
                gtk_box_pack_start (GTK_BOX (hbox), ui->op_target_label, FALSE, FALSE, 0);
                gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ui->op_win)->vbox),
                                    alignment, FALSE, FALSE, 0);

        } else {
                alignment = gtk_alignment_new (0.0, 0.5, 0, 0);
                hbox = gtk_hbox_new (FALSE, 0);
                gtk_container_add (GTK_CONTAINER (alignment), hbox);
                gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_(gdialog_deleting_string)),
                                    FALSE, FALSE, 0);
                ui->op_source_label = gtk_label_new ("");

                gtk_box_pack_start (GTK_BOX (hbox), ui->op_source_label, FALSE, FALSE, 0);
                gtk_box_set_spacing (GTK_BOX (GNOME_DIALOG (ui->op_win)->vbox), GNOME_PAD_SMALL);
                gtk_container_set_border_width (GTK_CONTAINER (GNOME_DIALOG (ui->op_win)->vbox),
                                                GNOME_PAD);
                gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ui->op_win)->vbox),
                                    alignment, FALSE, FALSE, 0);
        }

        alignment = gtk_alignment_new (0.0, 0.5, 0, 0);
        hbox = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("File ")), FALSE, FALSE, 0);
        ui->count_label = GTK_OBJECT (gtk_label_new (""));
        gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (ui->count_label), FALSE, FALSE, 0);

        gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("is ")), FALSE, FALSE, 0);
        ui->file_label = gtk_label_new ("");
        gtk_box_pack_start (GTK_BOX (hbox), ui->file_label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("done.")), FALSE, FALSE, 0);
        gtk_container_add (GTK_CONTAINER (alignment), hbox);

        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ui->op_win)->vbox),
                            alignment, FALSE, FALSE, 0);

        ui->byte_prog = GTK_OBJECT (gtk_progress_bar_new ());
        gtk_widget_set_usize (GTK_WIDGET (ui->byte_prog), GDIALOG_PROGRESS_WIDTH, -1);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ui->op_win)->vbox),
                            GTK_WIDGET (ui->byte_prog), FALSE, FALSE, 0);

	/* done with things */
        gtk_widget_show_all (GNOME_DIALOG (ui->op_win)->vbox);
        gtk_window_set_modal (GTK_WINDOW (ui->op_win), TRUE);
        gtk_widget_show_now (ui->op_win);
}

void
file_op_context_destroy_ui (FileOpContext *ctx)
{
        FileOpContextUI *ui;

        g_return_if_fail (ctx != NULL);

        if (ctx->ui == NULL)
                return;

        ui = ctx->ui;

        gtk_widget_destroy (ui->op_win);

        g_free (ui);
        ctx->ui = NULL;
}

void
fmd_init_i18n (int force)
{
        /* unneccessary func */
}

/* Callback for the gnome_request_dialog() in the input dialog */
static void
input_dialog_cb (gchar *string, gpointer data)
{
        char **s;

        s = data;
        *s = string;

        gtk_main_quit ();
}

/* Our implementation of the general-purpose input dialog */
char *
real_input_dialog_help (char *header, char *text, char *help, char *def_text)
{
        int is_password;
        GtkWidget *dialog;
        char *string;

        if (strncmp (text, _("Password:"), strlen (_("Password"))) == 0)
                is_password = TRUE;
        else
                is_password = FALSE;

        dialog = gnome_request_dialog (is_password, text, def_text, 0,
                                       input_dialog_cb, &string,
                                       NULL);
        gtk_window_set_title (GTK_WINDOW (dialog), header);
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
        gtk_main ();
        return string;
}

/* Our implementation of the symlink-to dialog */
void
symlink_dialog (char *existing, char *new, char **ret_existing, char **ret_new)
{
        GtkWidget *dialog;
        GtkWidget *vbox;
        GtkWidget *w;
        GtkWidget *entry1, *entry2;
        WPanel *panel;
        int ret;

        g_return_if_fail (existing != NULL);
        g_return_if_fail (new != NULL);
        g_return_if_fail (ret_existing != NULL);
        g_return_if_fail (ret_new != NULL);

        /* Create the dialog */

        dialog = gnome_dialog_new (_("Symbolic Link"),
                                   GNOME_STOCK_BUTTON_OK,
                                   GNOME_STOCK_BUTTON_CANCEL,
                                   NULL);
        gnome_dialog_close_hides (GNOME_DIALOG (dialog), TRUE);
        gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);

        panel = cpanel;
        if (!is_a_desktop_panel (panel))
                gnome_dialog_set_parent (GNOME_DIALOG (dialog), panel->xwindow);

        /* File symlink will point to */

        vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), vbox, FALSE, FALSE, 0);

        w = gtk_label_new (_("Existing filename (filename symlink will point to):"));
        gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

        entry1 = gtk_entry_new ();
        gtk_entry_set_text (GTK_ENTRY (entry1), existing);
        gtk_box_pack_start (GTK_BOX (vbox), entry1, FALSE, FALSE, 0);
        gnome_dialog_editable_enters (GNOME_DIALOG (dialog), GTK_EDITABLE (entry1));

        /* Name of symlink */

        vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), vbox, FALSE, FALSE, 0);

        w = gtk_label_new (_("Symbolic link filename:"));
        gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

        entry2 = gtk_entry_new ();
        gtk_entry_set_text (GTK_ENTRY (entry2), new);
        gtk_box_pack_start (GTK_BOX (vbox), entry2, FALSE, FALSE, 0);
        gnome_dialog_editable_enters (GNOME_DIALOG (dialog), GTK_EDITABLE (entry2));
        gtk_widget_grab_focus (entry2);

        /* Run */

        gtk_widget_show_all (GNOME_DIALOG (dialog)->vbox);
        ret = gnome_dialog_run (GNOME_DIALOG (dialog));

        if (ret != 0) {
                *ret_existing = NULL;
                *ret_new = NULL;
        } else {
                *ret_existing = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry1)));
                *ret_new = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry2)));
        }

        if (ret != -1)
                gtk_widget_destroy (dialog);
}
