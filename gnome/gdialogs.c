/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* New dialogs... */
#include <gnome.h>
#include "../vfs/vfs.h"
#include "file.h"
#include "panel.h"
#include "filegui.h"

static GtkWidget *op_win = NULL;
static GtkWidget *fmd_win = NULL;
static GtkWidget *byte_progress;
static GtkWidget *op_source_label;
static GtkWidget *op_target_label;
static GtkWidget *file_progress;
int op_preserve = 1;
/* Callbacks go here... */
static void
fmd_check_box_callback (GtkWidget *widget, gpointer data)
{
        if (data)
                *((gint*)data) = GTK_TOGGLE_BUTTON (widget)->active;
}

FileProgressStatus
file_progress_show_source (char *path)
{
        gtk_label_set (GTK_LABEL (op_source_label), path);
	return FILE_CONT;
}


FileProgressStatus
file_progress_show_target (char *path)
{
        gtk_label_set(GTK_LABEL (op_target_label), path);
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
	g_warning ("memo: file_progress_show!\ndone\t%d\ntotal\t%d\n", done, total);
	return FILE_CONT;
}

FileProgressStatus
file_progress_show_count (long done, long total)
{
	g_warning ("memo: file_progress_show_count!\ndone\t%d\ntotal\t%d\n", done, total);
	return FILE_CONT;
}

FileProgressStatus
file_progress_show_bytes (long done, long total)
{
	g_warning ("memo: file_progress_show_bytes!\ndone\t%d\ntotal\t%d\n", done, total);
	return FILE_CONT;
}

FileProgressStatus
file_progress_real_query_replace (enum OperationMode mode, char *destname, struct stat *_s_stat, struct stat *_d_stat)
{
	g_warning ("memo: file_progress_real_query_replace!\n");
	return FILE_CONT;
}

void
file_progress_set_stalled_label (char *stalled_msg)
{
	g_warning ("memo: file_progress_set_stalled_label!\nmsg\t%s\n",stalled_msg);
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
                fmd_win = gnome_dialog_new ("Copy\n", GNOME_STOCK_BUTTON_OK, 
                                            GNOME_STOCK_BUTTON_CANCEL, NULL);
        else if (operation == OP_MOVE)
                fmd_win = gnome_dialog_new ("Move\n", GNOME_STOCK_BUTTON_OK, 
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
        gnome_widget_add_help (cbox, "Selecting this will run the copying in the background.  "
                               "This is useful for transfers over networks that might take a long "
                               "time to complete.");
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
        gnome_widget_add_help (cbox, "FIXME: Add something here Miguel");
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
        GtkWidget *label;
        GtkWidget *hbox;
        g_print ("in create_op_win\n");
        switch (op) {
        case OP_MOVE:
                op_win = gnome_dialog_new ("Move Progress", GNOME_STOCK_BUTTON_CANCEL, NULL);
                break;
        case OP_COPY:
                op_win = gnome_dialog_new ("Copy Progress", GNOME_STOCK_BUTTON_CANCEL, NULL);
                break;
        case OP_DELETE:
                op_win = gnome_dialog_new ("Delete Progress", GNOME_STOCK_BUTTON_CANCEL, NULL);
                break;
        }

        alignment = gtk_alignment_new (0.0, 0.5, 0, 0);
        hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (alignment), hbox);
        gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (N_("Copying from: ")), FALSE, FALSE, 0);
        op_source_label = gtk_label_new ("");
        gtk_box_pack_start (GTK_BOX (hbox), op_source_label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (op_win)->vbox),
                            alignment, FALSE, FALSE, 0);

        alignment = gtk_alignment_new (0.0, 0.5, 0, 0);
        hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (alignment), hbox);
        gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (N_("To: ")), FALSE, FALSE, 0);
        op_target_label = gtk_label_new ("");
        gtk_box_pack_start (GTK_BOX (hbox), op_target_label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (op_win)->vbox),
                            alignment, FALSE, FALSE, 0);
        gtk_window_set_modal (GTK_WINDOW (op_win), TRUE);
        gtk_widget_show_all (GNOME_DIALOG (op_win)->vbox);
        gtk_widget_show_now (op_win);
        
}

void
destroy_op_win (void)
{
        if (op_win)
                 gtk_widget_destroy (op_win);
}
void
fmd_init_i18n()
{
        /* unneccessary func */
}
