/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* New dialogs... */
#include <gnome.h>
#include "../vfs/vfs.h"
#include "file.h"
#include "panel.h"

static GtkWidget *op_win = NULL;

FileProgressStatus
file_progress_show_source (char *path)
{
  	g_warning ("Show source!\n");
	return FILE_CONT;
}


FileProgressStatus
file_progress_show_target (char *path)
{
	g_warning ("memo: file_progress_show_target!\n");
	return FILE_CONT;
}

FileProgressStatus
file_progress_show_deleting (char *path)
{
	g_warning ("memo: file_progress_show_deleting!\n");
	return FILE_CONT;
}

FileProgressStatus
file_progress_show (long done, long total)
{
	g_warning ("memo: file_progress_show!\n");
	return FILE_CONT;
}

FileProgressStatus
file_progress_show_count (long done, long total)
{
	g_warning ("memo: file_progress_show_count!\n");
	return FILE_CONT;
}

FileProgressStatus
file_progress_show_bytes (long done, long total)
{
	g_warning ("memo: file_progress_show_bytes!\n");
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
	g_warning ("memo: file_progress_set_stalled_label!\n");
}
char *
file_mask_dialog (FileOperation operation, char *text, char *def_text, int only_one, int *do_background)
{
	g_warning ("memo: file_mask_dialog!\n");
	return NULL;
}

void
create_op_win (FileOperation op, int with_eta)
{
	if (op_win == NULL) {
                op_win = gnome_dialog_new ("Op Win -- change this title", GNOME_STOCK_BUTTON_CANCEL, NULL);
                gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (op_win)->vbox), gtk_label_new ("borp\n"), FALSE, FALSE, 0);
        }
        gtk_widget_show(GTK_WIDGET(op_win));
        g_print ("create_op_win\n");
}

void
destroy_op_win (void)
{
	if (op_win) {
                g_print ("hiding in  destroy_op_win\n");
                gtk_widget_hide (op_win);
                
        }
}
void
fmd_init_i18n()
{
        /* unneccessary func */
}
