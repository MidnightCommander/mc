/*
 * Various Menu-invoked Command implementations specific to the GNOME port
 *
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@kernel.org)
 */
#include <config.h>
#include "x.h"
#include <stdio.h>
#include <sys/stat.h>
#include "dir.h"
#include "panel.h"
#include "gscreen.h"
#include "main.h"
#include "gmain.h"
#include "cmd.h"
#include "boxes.h"
#include "panelize.h"
#include "gcmd.h"
#include "dialog.h"
#include "layout.h"
#include "../vfs/vfs.h"

static enum {
	SORT_NAME,
	SORT_EXTENSION,
	SORT_ACCESS,
	SORT_MODIFY,
	SORT_CHANGE,
	SORT_SIZE
} SortOrderCode;

void
gnome_listing_cmd (GtkWidget *widget, WPanel *panel)
{
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
	char *p;
	
	if (!(p = gnome_is_program_in_path ("gnome-terminal")))
		if (!(p = gnome_is_program_in_path ("dtterm")))
			if (!(p = gnome_is_program_in_path ("nxterm")))
				if (!(p = gnome_is_program_in_path ("color-xterm")))
					if (!(p = gnome_is_program_in_path ("rxvt")))
						p = gnome_is_program_in_path ("xterm");

	if (p)
		my_system (EXECUTE_AS_SHELL, shell, p);
	else
		message (1, MSG_ERROR, " Could not start a terminal ");
}

void
gnome_about_cmd (void)
{
        GtkWidget *about;
        const gchar *authors[] = {
		"The Midnight Commander Team",
		"http://www.gnome.org/mc/",
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
	int q = 0;

	if (!confirm_exit)
		q = 1;
	else if (query_dialog (_(" The Midnight Commander "),
			       _(" Do you really want to quit the Midnight Commander? "),
			       0, 2, _("&Yes"), _("&No")) == 0)
		q = 1;
	
	if (q == 1)
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

	if (panel->timer_id){
		gtk_timeout_remove (panel->timer_id);
		panel->timer_id = -1;
	}
	
	/* Remove the widgets from the dialog head */
	remove_widget (h, panel->current_dir);
	remove_widget (h, panel->filter_w);
	remove_widget (h, panel);

	/* Kill them */
	destroy_widget (panel->current_dir);
	destroy_widget (panel->filter_w);
	destroy_widget ((void *)panel);

	layout_panel_gone (panel);

	mc_chdir ("/");
	return TRUE;
}

void
gnome_icon_view_cmd (GtkWidget *widget, WPanel *panel)
{
	if (panel->list_type == list_icons)
		return;
	panel->list_type = list_icons;
	set_panel_formats (panel);
	paint_panel (panel);
	do_refresh ();
}
void
gnome_partial_view_cmd (GtkWidget *widget, WPanel *panel)
{
	if (panel->list_type == list_brief)
		return;
	panel->list_type = list_brief;
	set_panel_formats (panel);
	paint_panel (panel);
	do_refresh ();
}
void
gnome_full_view_cmd (GtkWidget *widget, WPanel *panel)
{
	if (panel->list_type == list_full)
		return;
	panel->list_type = list_full;
	set_panel_formats (panel);
	paint_panel (panel);
	do_refresh ();
}
void
gnome_custom_view_cmd (GtkWidget *widget, WPanel *panel)
{
	if (panel->list_type == list_user)
		return;
	panel->list_type = list_user;
	set_panel_formats (panel);
	paint_panel (panel);
	do_refresh ();
}
static void
option_menu_sort_callback(GtkWidget *widget, gpointer data)
{
	g_print ("FIXME: implement this function;option_menu_sort_callback\n");
	switch ((gint) data) {
	case SORT_NAME:

		break;
	case SORT_EXTENSION:
		
		break;
	case SORT_ACCESS:
		
		break;
	case SORT_MODIFY:
		
		break;
	case SORT_CHANGE:
		
		break;
	case SORT_SIZE:
		
		break;
	}
}
void
gnome_sort_cmd (GtkWidget *widget, WPanel *panel)
/* Helps you determine the order in which the files exist. */
{
	GtkWidget *sort_box;
	GtkWidget *hbox;
        GtkWidget *omenu;
        GtkWidget *menu;
        GtkWidget *menu_item;
	GtkWidget *cbox;

	sort_box = gnome_dialog_new ("Sort By", GNOME_STOCK_BUTTON_OK, 
				     GNOME_STOCK_BUTTON_CANCEL, NULL);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (sort_box)->vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new ("Sort files by "), FALSE, FALSE, 0);

	omenu = gtk_option_menu_new ();
        gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, FALSE, 0);
        menu = gtk_menu_new ();
        menu_item = gtk_menu_item_new_with_label ( _("Name"));
        gtk_menu_append (GTK_MENU (menu), menu_item);
        gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                            GTK_SIGNAL_FUNC (option_menu_sort_callback), (gpointer) SORT_NAME);
        menu_item = gtk_menu_item_new_with_label ( _("File Type"));
        gtk_menu_append (GTK_MENU (menu), menu_item);
        gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                            GTK_SIGNAL_FUNC (option_menu_sort_callback), (gpointer) SORT_EXTENSION);
	menu_item = gtk_menu_item_new_with_label ( _("Size"));
        gtk_menu_append (GTK_MENU (menu), menu_item);
        gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                            GTK_SIGNAL_FUNC (option_menu_sort_callback), (gpointer) SORT_SIZE);
        menu_item = gtk_menu_item_new_with_label ( _("Time Last Accessed"));
        gtk_menu_append (GTK_MENU (menu), menu_item);
        gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                            GTK_SIGNAL_FUNC (option_menu_sort_callback), (gpointer) SORT_ACCESS);
        menu_item = gtk_menu_item_new_with_label ( _("Time Last Modified"));
        gtk_menu_append (GTK_MENU (menu), menu_item);
        gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                            GTK_SIGNAL_FUNC (option_menu_sort_callback), (gpointer) SORT_MODIFY);
        menu_item = gtk_menu_item_new_with_label ( _("Time Last Changed"));
        gtk_menu_append (GTK_MENU (menu), menu_item);
        gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
                            GTK_SIGNAL_FUNC (option_menu_sort_callback), (gpointer) SORT_CHANGE);
	gtk_widget_show_all (menu);
        gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);

	
	cbox = gtk_check_button_new_with_label (N_("Reverse the order."));
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (sort_box)->vbox),
			    cbox, FALSE, FALSE, 0);
	cbox = gtk_check_button_new_with_label (N_("Ignore case sensitivity."));
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (sort_box)->vbox),
			    cbox, FALSE, FALSE, 0);
	/* off to the races */
	gtk_widget_show_all (GNOME_DIALOG (sort_box)->vbox);
	gnome_dialog_run (GNOME_DIALOG (sort_box));

	/* Reverse order */
	/* case sensitive */
}
