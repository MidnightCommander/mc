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

/* Prototypes */
static void try_callback ();
static void revert_callback ();
static void ok_callback ();
static void cancel_callback ();
static void help_callback ();
GtkWidget *capplet;

static void
try_callback ()
{
        g_print ("testing...\n");
        write_user_keys ();
}
static void
revert_callback ()
{

}
static void
ok_callback ()
{

}
static void
cancel_callback ()
{

}
static void
help_callback ()
{

}
static void
init_mime_capplet ()
{
        GtkWidget *vbox;
        GtkWidget *hbox;
        GtkWidget *button;

	capplet = capplet_widget_new ();
        vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_container_add (GTK_CONTAINER (capplet), vbox);
        gtk_box_pack_start (GTK_BOX (vbox), get_mime_clist (), TRUE, TRUE, 0);
        hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
        button = gtk_button_new_with_label (_("Remove"));
        gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        button = gtk_button_new_with_label (_("Add"));
        gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        button = gtk_button_new_with_label (_("Edit"));
        gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_widget_show_all (capplet);
        gtk_signal_connect(GTK_OBJECT(capplet), "try",
                           GTK_SIGNAL_FUNC(try_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "revert",
                           GTK_SIGNAL_FUNC(revert_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "ok",
                           GTK_SIGNAL_FUNC(ok_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "cancel",
                           GTK_SIGNAL_FUNC(cancel_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "help",
                           GTK_SIGNAL_FUNC(help_callback), NULL);
}

int
main (int argc, char **argv)
{
        int init_results;

        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
        textdomain (PACKAGE);

        init_results = gnome_capplet_init("mouse-properties", VERSION,
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
