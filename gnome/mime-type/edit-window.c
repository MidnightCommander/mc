#include "edit-window.h"
typedef struct {
	GtkWidget *window;
} edit_window;
static edit_window *main_win = NULL;

static void
initialize_main_win ()
{
	main_win = g_new (edit_window, 1);
	main_win->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	
}


void
launch_edit_window (MimeInfo *mi)
{
	if (main_win == NULL)
		initialize_main_win ();
	gtk_widget_show_all (main_win->window);
}

