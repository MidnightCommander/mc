#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gtk/gtk.h"
#include "gdk/gdk.h"
#include "gdk/gdkx.h"
#include "gtkedit.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main (int argc, char *argv[])
{
    static GtkWidget *window = NULL;
    GtkWidget *edit;
    int infile;

    gtk_set_locale ();
    gnome_init ("Hi there", NULL, argc, argv, 0, NULL);
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_usize (window, 400, 400);
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
			GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			&window);
    gtk_container_border_width (GTK_CONTAINER (window), 3);
    edit = gtk_edit_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (window), edit);
    gtk_widget_show (edit);
    gtk_widget_realize (edit);
    infile = open ("edit.c", O_RDONLY);
    if (infile) {
	char buffer[1024];
	int nchars;
	while (1) {
	    nchars = read (infile, buffer, 1024);
	    gtk_edit_insert (GTK_EDIT (edit), NULL, NULL,
			     NULL, buffer, nchars);
	    if (nchars < 1024)
		break;
	}
	close (infile);
    }
    gtk_editable_set_position (edit, 0);
    gtk_widget_show (window);
    gtk_main ();
    return 0;
}
