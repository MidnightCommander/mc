/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* New dialogs... */
#include <config.h>
#include "util.h"
#include "panel.h"
#include <gnome.h>
#include "file.h"
#include "filegui.h"
#include "regex.h"
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

static GtkWidget *op_win = NULL;
static GtkWidget *fmd_win = NULL;
static GtkObject *count_label = NULL;
static GtkObject *byte_prog = NULL;
static GtkWidget *file_label = NULL;
static GtkWidget *op_source_label = NULL;
static GtkWidget *op_target_label = NULL;
static GtkWidget *op_radio = NULL;
int op_preserve = 1;
static gint copy_status;
static gint minor_copy_status;

#define GDIALOG_TO_STRING "To: "
#define GDIALOG_FROM_STRING "Copying from: "
#define GDIALOG_PROGRESS_WIDTH 350



/* Callbacks go here... */
static void
fmd_check_box_callback (GtkWidget *widget, gpointer data)
{
        if (data)
                *((gint*)data) = GTK_TOGGLE_BUTTON (widget)->active;
}

static gchar *
trim_file_name (gchar *path, gint length, gint cur_length)
{
        static gint dotdotdot = 0;
        gchar *path_copy = NULL;
        gint len;

        if (!dotdotdot)
                dotdotdot = gdk_string_width (op_source_label->style->font, "...");
        /* Cut the font length of path to length. */

        length -= dotdotdot;
        len = (gint) ((1.0 - (gfloat) length / (gfloat) cur_length) * strlen (path));
        
        /* we guess a starting point */
        if (gdk_string_width (op_source_label->style->font, path + len) < length) {
                while (gdk_string_width (op_source_label->style->font, path + len) < length)
                        len --;
                len++;
        } else {
                while (gdk_string_width (op_source_label->style->font, path + len) > length)
                        len ++;
        }
        path_copy = g_malloc (sizeof (gchar [1 + 3 + strlen (path) - len]));
        sprintf (path_copy, "...%s", path + len);
        return path_copy;
}

FileProgressStatus
file_progress_show_source (char *path)
{
        static gint from_width = 0;
        gint path_width;
        gchar *path_copy = NULL;

        g_return_val_if_fail (op_source_label != NULL, FILE_CONT);

        if (path == NULL){
                gtk_label_set (GTK_LABEL (op_source_label), "");
                return FILE_CONT;
        }
        
        if (!from_width){
                from_width = gdk_string_width (op_source_label->style->font,
                                             GDIALOG_FROM_STRING);
        }
        path_width = gdk_string_width (op_source_label->style->font, path);
        if (from_width + path_width < GDIALOG_PROGRESS_WIDTH)
                gtk_label_set (GTK_LABEL (op_source_label), path);
        else {
                path_copy = trim_file_name (path, GDIALOG_PROGRESS_WIDTH - from_width, path_width);
                
                gtk_label_set (GTK_LABEL (op_source_label), path_copy);
                g_free (path_copy);
        }

	return FILE_CONT;
}

FileProgressStatus
file_progress_show_target (char *path)
{
        static gint to_width = 0;
        gint path_width;
        gchar *path_copy = NULL;

        g_return_val_if_fail (op_target_label != NULL, FILE_CONT);

        if (path == NULL){
                gtk_label_set (GTK_LABEL (op_target_label), "");
                return FILE_CONT;
        }
        
        if (!to_width) 
                to_width = gdk_string_width (op_target_label->style->font,
                                             GDIALOG_TO_STRING);
        path_width = gdk_string_width (op_target_label->style->font, path);
        if (to_width + path_width < GDIALOG_PROGRESS_WIDTH)
                gtk_label_set (GTK_LABEL (op_target_label), path);
        else {
                path_copy = trim_file_name (path, GDIALOG_PROGRESS_WIDTH - to_width, path_width);
                gtk_label_set (GTK_LABEL (op_target_label), path_copy);
                g_free (path_copy);
        }

	return FILE_CONT;
}

FileProgressStatus
file_progress_show_deleting (char *path)
{
	g_warning ("memo: file_progress_show_deleting!\npath\t%s\n",path);
        return FILE_CONT;
}

FileProgressStatus
file_progress_show (long done, long total)
{
        static gchar count[10];
        snprintf (count, 9, "%d%%", (gint)(100.0 *(gfloat)done/(gfloat)total));
        gtk_label_set (GTK_LABEL (file_label), count);
        while (gtk_events_pending ())
                gtk_main_iteration ();
	return FILE_CONT;
}

FileProgressStatus
file_progress_show_count (long done, long total)
{
        static gchar count[14]; /* that's a lot of files... */
        snprintf (count, 13, "%d/%d", done, total);
        gtk_label_set (GTK_LABEL (count_label), count);
        while (gtk_events_pending ())
                gtk_main_iteration ();
	return FILE_CONT;
}

FileProgressStatus
file_progress_show_bytes (double done, double total)
{
        if (!total)
                gtk_progress_bar_update (GTK_PROGRESS_BAR (byte_prog), 0.0);
        else
                gtk_progress_bar_update (GTK_PROGRESS_BAR (byte_prog), (gfloat) done/(gfloat) total);
        while (gtk_events_pending ())
                gtk_main_iteration ();
	return FILE_CONT;
}
static void
option_menu_policy_callback (GtkWidget *menu, gpointer data)
{
        minor_copy_status = (gint) data;
        copy_status = (gint) data;
        gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (op_radio), TRUE);
}

static void
policy_callback (GtkWidget *button, gpointer data)
{
        if (GTK_TOGGLE_BUTTON (button)->active) {
                g_print ("toggle\n");
                if ((gint) data == REPLACE_OPTION_MENU) {
                        copy_status = minor_copy_status;
                } else
                        copy_status = (gint) data;
        }
}
FileProgressStatus
file_progress_query_replace_policy (gboolean dialog_needed)
{
        GtkWidget *qrp_dlg;
        GtkWidget *radio;
        GtkWidget *vbox;
        GtkWidget *hbox;
        GtkWidget *icon;
        GtkWidget *label;
        GtkWidget *hrbox;
        GSList *group = NULL;
        GtkWidget *omenu;
        GtkWidget *menu;
        GtkWidget *menu_item;

        copy_status = REPLACE_PROMPT;
        if (dialog_needed == FALSE)
                return FILE_CONT;
        minor_copy_status = REPLACE_ALWAYS;
        g_print ("in file_progress_query_replace_policy\n");
        qrp_dlg = gnome_dialog_new (_("Files Exist"),
                                    GNOME_STOCK_BUTTON_OK,
                                    GNOME_STOCK_BUTTON_CANCEL,
                                    NULL);
        vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
        hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
        icon = gnome_stock_pixmap_widget (hbox, GNOME_STOCK_PIXMAP_HELP);

        gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, GNOME_PAD_SMALL);

        label = gtk_label_new (_("Some of the files you are trying to copy already "
                                 "exist in the destination folder."));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, FALSE, 0);

        radio = gtk_radio_button_new_with_label (group, _("Prompt me before overwriting any file."));
        gtk_signal_connect (GTK_OBJECT (radio), "toggled", GTK_SIGNAL_FUNC (policy_callback), (gpointer) REPLACE_PROMPT);
        gtk_box_pack_start (GTK_BOX (vbox), radio, FALSE, FALSE, 0);
        group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));


        radio = gtk_radio_button_new_with_label (group, _("Don't overwrite any files."));
        gtk_signal_connect (GTK_OBJECT (radio), "toggled", GTK_SIGNAL_FUNC (policy_callback), (gpointer) REPLACE_NEVER);
        gtk_box_pack_start (GTK_BOX (vbox), radio, FALSE, FALSE, 0);
        group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));

        
        op_radio = gtk_radio_button_new (group);
        gtk_signal_connect (GTK_OBJECT (op_radio), "toggled", GTK_SIGNAL_FUNC (policy_callback), (gpointer) REPLACE_OPTION_MENU);
        hrbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_box_pack_start (GTK_BOX (hrbox), gtk_label_new (_("Overwrite:")), FALSE, FALSE, 0);
        
        /* we set up the option menu. */
        omenu = gtk_option_menu_new ();
        gtk_box_pack_start (GTK_BOX (hrbox), omenu, FALSE, FALSE, 0);
        menu = gtk_menu_new ();
        menu_item = gtk_menu_item_new_with_label ( _("Older files."));
        gtk_menu_append (GTK_MENU (menu), menu_item);
        gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                            GTK_SIGNAL_FUNC (option_menu_policy_callback), (gpointer) REPLACE_UPDATE);
        menu_item = gtk_menu_item_new_with_label ( _("Files only if size differs."));
        gtk_menu_append (GTK_MENU (menu), menu_item);
        gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                            GTK_SIGNAL_FUNC (option_menu_policy_callback), (gpointer) REPLACE_SIZE);
        menu_item = gtk_menu_item_new_with_label ( _("All files."));
        gtk_menu_append (GTK_MENU (menu), menu_item);
        gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                            GTK_SIGNAL_FUNC (option_menu_policy_callback), (gpointer) REPLACE_ALWAYS);
        gtk_widget_show_all (menu);
        gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);

        gtk_signal_connect (GTK_OBJECT (op_radio), "toggled", GTK_SIGNAL_FUNC (policy_callback), (gpointer) REPLACE_ALWAYS);
        gtk_box_pack_start (GTK_BOX (vbox), op_radio, FALSE, FALSE, 0);
        group = gtk_radio_button_group (GTK_RADIO_BUTTON (op_radio));
        gtk_container_add (GTK_CONTAINER (op_radio), hrbox);

        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (qrp_dlg)->vbox),
                            hbox, FALSE, FALSE, 0);

        gtk_widget_show_all (GTK_WIDGET (GNOME_DIALOG (qrp_dlg)->vbox));
        switch (gnome_dialog_run_and_close (GNOME_DIALOG (qrp_dlg))) {
        case 0:
                break;
        case -1:
        default:
                copy_status = REPLACE_ABORT;
                return FILE_ABORT;
        }
        return FILE_CONT;
}


FileProgressStatus
file_progress_real_query_replace (enum OperationMode mode, char *destname, struct stat *_s_stat, struct stat *_d_stat)
{
        GtkWidget *qr_dlg;
        gchar msg[128];
        GtkWidget *label;

        /* so what's the situation?  Do we prompt or don't we prompt. */
        if (copy_status == REPLACE_PROMPT){
                qr_dlg = gnome_dialog_new ("File Exists", "Yes", "No", "Cancel", NULL);
                
                snprintf (msg, 127, "The target file already exists:", destname);
                label = gtk_label_new ("The target file already exists.");
                gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
                gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
                gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
                gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (qr_dlg)->vbox),
                                    label, FALSE, FALSE, 0);

                gtk_widget_show_all (GNOME_DIALOG (qr_dlg)->vbox);
                g_print ("here?\n");
                switch (gnome_dialog_run_and_close (GNOME_DIALOG (qr_dlg))) {
                case 0:
                        return FILE_CONT;
                case 1:
                        return FILE_SKIP;
                default:
                        return FILE_ABORT;
                }
        }

        switch (copy_status){
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
file_progress_set_stalled_label (char *stalled_msg)
{
        if (!stalled_msg || !*stalled_msg)
                return;
	g_warning ("FIXME: file_progress_set_stalled_label!\nmsg\t%s\n",stalled_msg);
}
char *
file_mask_dialog (FileOperation operation, char *text, char *def_text, int only_one, int *do_background)
{
        GtkWidget *notebook;
        GtkWidget *hbox;
        GtkWidget *vbox, *label;
        GtkWidget *alignment;
        GtkWidget *fentry;
        GtkWidget *cbox;
        GtkWidget *icon;
        int source_easy_patterns = easy_patterns;
        char *source_mask, *orig_mask, *dest_dir;
        const char *error;
        struct stat buf;

        file_mask_stable_symlinks = 0;
        
        /* Basic window */
        if (operation == OP_COPY)
                fmd_win = gnome_dialog_new ("Copy", GNOME_STOCK_BUTTON_OK, 
                                            GNOME_STOCK_BUTTON_CANCEL, NULL);
        else if (operation == OP_MOVE)
                fmd_win = gnome_dialog_new ("Move", GNOME_STOCK_BUTTON_OK, 
                                            GNOME_STOCK_BUTTON_CANCEL, NULL);

        hbox = gtk_hbox_new (FALSE, GNOME_PAD);
        notebook = gtk_notebook_new ();
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (fmd_win)->vbox),
                            notebook, FALSE, FALSE, 0);
        /*FIXME: I wan't a bigger, badder, better Icon here... */
        icon = gnome_stock_pixmap_widget (hbox, GNOME_STOCK_PIXMAP_HELP);
        gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
        vbox = gtk_vbox_new (FALSE, GNOME_PAD);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                                  hbox, 
                                  gtk_label_new (N_("Destination")));
        alignment = gtk_alignment_new (0.0, 0.5, 0, 0);
        label = gtk_label_new (text);
        gtk_container_add (GTK_CONTAINER (alignment), label);
        fentry = gnome_file_entry_new ("gmc-copy-file", N_("Find Destination Folder"));
        gnome_file_entry_set_directory (GNOME_FILE_ENTRY (fentry), TRUE);
        gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (fentry))),
                            def_text);
        gnome_file_entry_set_default_path (GNOME_FILE_ENTRY (fentry), def_text);
        cbox = gtk_check_button_new_with_label (N_("Copy as a background process"));
        gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (cbox), *do_background);
        gtk_signal_connect (GTK_OBJECT (cbox), "toggled", (GtkSignalFunc) fmd_check_box_callback, do_background);
#if 0
        gnome_widget_add_help (cbox, "Selecting this will run the copying in the background.  "
                               "This is useful for transfers over networks that might take a long "
                               "time to complete.");
#endif
        gtk_box_pack_end (GTK_BOX (vbox), cbox, FALSE, FALSE, 0);
        gtk_box_pack_end (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, FALSE, 0);
        gtk_box_pack_end (GTK_BOX (vbox), fentry, FALSE, FALSE, 0);
        gnome_file_entry_set_modal(GNOME_FILE_ENTRY (fentry),TRUE);
        
        gtk_box_pack_end (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

        /* Advanced Options */
        hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
        vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                                  hbox, 
                                  gtk_label_new (N_("Advanced Options")));
        gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

        cbox = gtk_check_button_new_with_label (N_("Preserve symlinks"));
        gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (cbox), file_mask_stable_symlinks);
        gtk_signal_connect (GTK_OBJECT (cbox), "toggled", (GtkSignalFunc) fmd_check_box_callback, &file_mask_stable_symlinks);
#if 0
        gnome_widget_add_help (cbox, "FIXME: Add something here Miguel");
#endif
        gtk_box_pack_start (GTK_BOX (vbox), cbox, FALSE, FALSE, 0);


        if (operation == OP_COPY) {
                cbox = gtk_check_button_new_with_label (N_("Follow links."));
                gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (cbox), file_mask_op_follow_links);
                gtk_signal_connect (GTK_OBJECT (cbox), "toggled", (GtkSignalFunc) fmd_check_box_callback, &file_mask_op_follow_links);
                gnome_widget_add_help (cbox, "Selecting this will copy the files that symlinks point "
                                       "to instead of just copying the link.");
                gtk_box_pack_start (GTK_BOX (vbox), cbox, FALSE, FALSE, 0);

                cbox = gtk_check_button_new_with_label (N_("Preserve file attributes."));
                gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (cbox), op_preserve);
                gtk_signal_connect (GTK_OBJECT (cbox), "toggled", (GtkSignalFunc) fmd_check_box_callback, &op_preserve);
                gnome_widget_add_help (cbox, "FIXME: Add something here Miguel");
                gtk_box_pack_start (GTK_BOX (vbox), cbox, FALSE, FALSE, 0);

                vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
                gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
                cbox = gtk_check_button_new_with_label (N_("Recursively copy subdirectories."));
                gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (cbox), dive_into_subdirs);
                gtk_signal_connect (GTK_OBJECT (cbox), "toggled", (GtkSignalFunc) fmd_check_box_callback, &dive_into_subdirs);
                gnome_widget_add_help (cbox, "FIXME: Add something here Miguel");
                gtk_box_pack_start (GTK_BOX (vbox), cbox, FALSE, FALSE, 0);
        }
        
        gtk_widget_show_all (GNOME_DIALOG (fmd_win)->vbox);
        gtk_window_set_modal (GTK_WINDOW (fmd_win), FALSE);
        gnome_dialog_set_close (GNOME_DIALOG (fmd_win), TRUE);
        gnome_dialog_close_hides (GNOME_DIALOG (fmd_win), TRUE);

        /* Off to the races!!! */
        if (gnome_dialog_run (GNOME_DIALOG (fmd_win)) == 1) {
                gtk_widget_destroy (fmd_win);
                return NULL;
        }
        dest_dir = gnome_file_entry_get_full_path(GNOME_FILE_ENTRY (fentry), TRUE);
        gtk_widget_destroy (fmd_win);
        easy_patterns = 1;
        if (!dest_dir || !*dest_dir){
                return NULL;
        }
        if (file_mask_op_follow_links && operation != OP_MOVE)
                file_mask_xstat = mc_stat;
        else
                file_mask_xstat = mc_lstat;
    
        if (op_preserve || operation == OP_MOVE){
                file_mask_preserve = 1;
                file_mask_umask_kill = 0777777;
                file_mask_preserve_uidgid = (geteuid () == 0) ? 1 : 0;
        }
        else {
            int i;
            file_mask_preserve = file_mask_preserve_uidgid = 0;
            i = umask (0);
            umask (i);
            file_mask_umask_kill = i ^ 0777777;
        }
        source_mask = strdup ("*");
        orig_mask = source_mask;
        if (!dest_dir || !*dest_dir){
                if (source_mask)
                        free (source_mask);
                return dest_dir;
        }

        if (!dest_dir){
                return NULL;
        }
        if (!*dest_dir) {
                g_free (dest_dir);
                return NULL;
        }
        if (source_easy_patterns){
                source_easy_patterns = easy_patterns;
                easy_patterns = 1;
                source_mask = convert_pattern (source_mask, match_file, 1);
                easy_patterns = source_easy_patterns;
                error = re_compile_pattern (source_mask, strlen (source_mask), &file_mask_rx);
                free (source_mask);
        } else
                error = re_compile_pattern (source_mask, strlen (source_mask), &file_mask_rx);
        
        if (error){
                g_warning ("%s\n",error);
        }
        if (orig_mask)
                free (orig_mask);
        file_mask_dest_mask = strrchr (dest_dir, PATH_SEP);
        if (file_mask_dest_mask == NULL)
                file_mask_dest_mask = dest_dir;
        else
                file_mask_dest_mask++;
        orig_mask = file_mask_dest_mask;
        if (!*file_mask_dest_mask || (!dive_into_subdirs && !is_wildcarded (file_mask_dest_mask) &&
                                      (!only_one || (!mc_stat (dest_dir, &buf) && S_ISDIR (buf.st_mode)))) ||
            (dive_into_subdirs && ((!only_one && !is_wildcarded (file_mask_dest_mask)) ||
                                   (only_one && !mc_stat (dest_dir, &buf) && S_ISDIR (buf.st_mode)))))
                file_mask_dest_mask = strdup ("*");
        else {
                file_mask_dest_mask = strdup (file_mask_dest_mask);
                *orig_mask = 0;
        }
        if (!*dest_dir){
                free (dest_dir);
                dest_dir = strdup ("./");
        }
        return dest_dir;
}

void
create_op_win (FileOperation op, int with_eta)
{
        GtkWidget *alignment;
        GtkWidget *hbox;

        
        switch (op) {
        case OP_MOVE:
                op_win = gnome_dialog_new ("Move Progress", GNOME_STOCK_BUTTON_CANCEL, NULL);
                break;
        case OP_COPY:
                op_win = gnome_dialog_new ("Copy Progress", GNOME_STOCK_BUTTON_CANCEL, NULL);
                break;
        case OP_DELETE:
                op_win = gnome_dialog_new ("Delete Progress", GNOME_STOCK_BUTTON_CANCEL, NULL);
                gtk_widget_show_all (GNOME_DIALOG (op_win)->vbox);
                gtk_widget_show_now (op_win);
                return;
        }

        alignment = gtk_alignment_new (0.0, 0.5, 0, 0);
        hbox = gtk_hbox_new (FALSE, 0);
        gtk_container_add (GTK_CONTAINER (alignment), hbox);
        gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (N_(GDIALOG_FROM_STRING)), FALSE, FALSE, 0);
        op_source_label = gtk_label_new ("");
        
        gtk_box_pack_start (GTK_BOX (hbox), op_source_label, FALSE, FALSE, 0);
        gtk_box_set_spacing (GTK_BOX (GNOME_DIALOG (op_win)->vbox), GNOME_PAD_SMALL);
        gtk_container_set_border_width (GTK_CONTAINER (GNOME_DIALOG (op_win)->vbox), GNOME_PAD);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (op_win)->vbox),
                            alignment, FALSE, FALSE, 0);
        
        alignment = gtk_alignment_new (0.0, 0.5, 0, 0);
        hbox = gtk_hbox_new (FALSE, 0);
        gtk_container_add (GTK_CONTAINER (alignment), hbox);
        gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (N_(GDIALOG_TO_STRING)), FALSE, FALSE, 0);
        op_target_label = gtk_label_new ("");
        gtk_box_pack_start (GTK_BOX (hbox), op_target_label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (op_win)->vbox),
                            alignment, FALSE, FALSE, 0);
        
        alignment = gtk_alignment_new (0.0, 0.5, 0, 0);
        hbox = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (N_("File ")), FALSE, FALSE, 0);
        count_label = GTK_OBJECT (gtk_label_new (""));
        gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (count_label), FALSE, FALSE, 0);
        
        gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (N_(" is ")), FALSE, FALSE, 0);
        file_label = gtk_label_new ("");
        gtk_box_pack_start (GTK_BOX (hbox), file_label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (N_(" Done.")), FALSE, FALSE, 0);
        
        gtk_container_add (GTK_CONTAINER (alignment), hbox);
        
        
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (op_win)->vbox),
                            alignment, FALSE, FALSE, 0);
        
        byte_prog = GTK_OBJECT (gtk_progress_bar_new ());
        gtk_widget_set_usize (GTK_WIDGET (byte_prog), GDIALOG_PROGRESS_WIDTH, -1);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (op_win)->vbox),
                            GTK_WIDGET (byte_prog), FALSE, FALSE, 0);
        

        /*done with things */
        gtk_widget_show_all (GNOME_DIALOG (op_win)->vbox);
        gtk_widget_show_now (op_win);
}

void
destroy_op_win (void)
{
        if (op_win){
                 gtk_widget_destroy (op_win);
                 op_win = NULL;
                 op_source_label = NULL;
                 op_target_label = NULL;
        }
}
void
fmd_init_i18n()
{
        /* unneccessary func */
}
