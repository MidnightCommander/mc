#include <config.h>
#include "x.h"
#include <stdio.h>
#include <sys/stat.h>
#include "dir.h"
#include "panel.h"
#include "gscreen.h"
#include "main.h"
#include "cmd.h"
#include "boxes.h"
#include "panelize.h"
#include "gcmd.h"
#include "dialog.h"

void
gnome_listing_cmd (GtkWidget *widget, WPanel *panel)
{
	GtkAllocation *alloc = &GTK_WIDGET (panel->list)->allocation;
	int   view_type, use_msformat;
	char  *user, *status;
	
	view_type = display_box (panel, &user, &status, &use_msformat, get_current_index ());
	
	if (view_type == -1)
		return;

	configure_panel_listing (panel, view_type, use_msformat, user, status);
}

void
gnome_compare_panels (void)
{
	if (get_other_panel () == NULL){
		message (1, MSG_ERROR, _(" There is no other panel to compare contents to "));
		return;
	}
	compare_dirs_cmd ();
}

void
gnome_open_terminal (void)
{
	my_system (1, shell, "nxterm");
}

void
gnome_about_cmd (void)
{
        GtkWidget *about;
        gchar *authors[] = {
		"The Midnight Commander Team",
		"http://mc.blackdown.org/mc",
		"bug reports: mc-bugs@nuclecu.unam.mx",
		NULL
	};

        about = gnome_about_new (_("GNU Midnight Commander"), VERSION,
				 "(C) 1994-1998 the Free Software Fundation",
				 authors,
				 _("The GNOME edition of the Midnight Commander file manager."),
				 NULL);
        gtk_widget_show (about);
}

void
gnome_quit_cmd (void)
{
	gtk_main_quit ();
}

void
gnome_open_panel (GtkWidget *widget, WPanel *panel)
{
	new_panel_at (panel->cwd);
}

int
gnome_close_panel (GtkWidget *widget, WPanel *panel)
{
	Dlg_head *h = panel->widget.parent;

	/* Remove the widgets from the dialog head */
	remove_widget (h, panel->current_dir);
	remove_widget (h, panel->filter_w);
	remove_widget (h, panel);

	/* Kill them */
	destroy_widget (panel->current_dir);
	destroy_widget (panel->filter_w);
	destroy_widget ((void *)panel);

	return TRUE;
}
