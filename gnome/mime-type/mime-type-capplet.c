/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Copyright (C) 1998 Redhat Software Inc. 
 * Authors: Jonathan Blandford <jrb@redhat.com>
 */
#include <config.h>
#include "capplet-widget.h"
#include "gnome.h"
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <regex.h>
#include <ctype.h>
#include "mime-data.h"
#include "mime-info.h"
#include "edit-window.h"
/* Prototypes */
static void try_callback ();
static void revert_callback ();
static void ok_callback ();
static void cancel_callback ();
static void help_callback ();
GtkWidget *capplet = NULL;
GtkWidget *delete_button = NULL;

static GtkWidget *
left_aligned_button (gchar *label)
{
  GtkWidget *button = gtk_button_new_with_label (label);
  gtk_misc_set_alignment (GTK_MISC (GTK_BIN (button)->child),
			  0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (GTK_BIN (button)->child),
			GNOME_PAD_SMALL, 0);

  return button;
}

static void
try_callback ()
{
        write_user_keys ();
        write_user_mime ();
}
static void
revert_callback ()
{
        write_initial_keys ();
        write_initial_mime ();
        discard_key_info ();
        discard_mime_info ();
        initialize_main_win_vals ();
}
static void
ok_callback ()
{
        write_user_keys ();
        write_user_mime ();
}
static void
cancel_callback ()
{
        write_initial_keys ();
        write_initial_mime ();
}
static void
help_callback ()
{
        /* Sigh... empty as always */
}
static void
init_mime_capplet ()
{
        GtkWidget *vbox;
        GtkWidget *hbox;
        GtkWidget *button;

	capplet = capplet_widget_new ();
        delete_button = left_aligned_button (_("Delete"));
        gtk_signal_connect (GTK_OBJECT (delete_button), "clicked",
                            delete_clicked, NULL);
                            
        hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD_SMALL);
        gtk_container_add (GTK_CONTAINER (capplet), hbox);
        gtk_box_pack_start (GTK_BOX (hbox), get_mime_clist (), TRUE, TRUE, 0);
        vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
        button = left_aligned_button (_("Add..."));
        gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                            add_clicked, NULL);
        button = left_aligned_button (_("Edit..."));
        gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                            edit_clicked, NULL);
        gtk_box_pack_start (GTK_BOX (vbox), delete_button, FALSE, FALSE, 0);
        gtk_widget_show_all (capplet);
        gtk_signal_connect(GTK_OBJECT(capplet), "try",
                           GTK_SIGNAL_FUNC(try_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "revert",
                           GTK_SIGNAL_FUNC(revert_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "ok",
                           GTK_SIGNAL_FUNC(ok_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "cancel",
                           GTK_SIGNAL_FUNC(cancel_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "page_hidden",
                           GTK_SIGNAL_FUNC(hide_edit_window), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "page_shown",
                           GTK_SIGNAL_FUNC(show_edit_window), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "help",
                           GTK_SIGNAL_FUNC(help_callback), NULL);
}

int
main (int argc, char **argv)
{
        int init_results;

        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
        textdomain (PACKAGE);

        init_results = gnome_capplet_init("mime-type-capplet", VERSION,
                                          argc, argv, NULL, 0, NULL);

	if (init_results < 0) {
                exit (0);
	}

	if (init_results == 0) {
		init_mime_type ();
		init_mime_capplet ();
	        capplet_gtk_main ();
	}
        return 0;
}
