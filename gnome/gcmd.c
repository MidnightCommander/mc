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
sort_callback (GtkWidget *menu_item, GtkWidget *cbox1)
{
	if (gtk_object_get_data (GTK_OBJECT (menu_item), "SORT_ORDER_CODE") == SORT_NAME)
		gtk_widget_set_sensitive (cbox1, TRUE);
	else
		gtk_widget_set_sensitive (cbox1, FALSE);
}
void
gnome_sort_cmd (GtkWidget *widget, WPanel *panel)
{
	GtkWidget *sort_box;
	GtkWidget *hbox;
        GtkWidget *omenu;
        GtkWidget *menu;
        GtkWidget *menu_item;
	GtkWidget *cbox1, *cbox2;
	sortfn *sfn;

	sort_box = gnome_dialog_new (_("Sort By"), GNOME_STOCK_BUTTON_OK, 
				     GNOME_STOCK_BUTTON_CANCEL, NULL);
	/* we define this up here so we can pass it in to our callback */
	cbox1 = gtk_check_button_new_with_label (N_("Ignore case sensitivity."));
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (sort_box)->vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new ("Sort files by "), FALSE, FALSE, 0);

	omenu = gtk_option_menu_new ();
        gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, FALSE, 0);
        menu = gtk_menu_new ();
        menu_item = gtk_menu_item_new_with_label ( _("Name"));
	/* FIXME: we want to set the option menu to be the correct ordering. */
        gtk_menu_append (GTK_MENU (menu), menu_item);
	gtk_object_set_data (GTK_OBJECT (menu_item), "SORT_ORDER_CODE", (gpointer) SORT_NAME);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
				   GTK_SIGNAL_FUNC (sort_callback), cbox1);
	
        menu_item = gtk_menu_item_new_with_label ( _("File Type"));
        gtk_menu_append (GTK_MENU (menu), menu_item);
	gtk_object_set_data (GTK_OBJECT (menu_item), "SORT_ORDER_CODE", (gpointer) SORT_EXTENSION);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
				   GTK_SIGNAL_FUNC (sort_callback), cbox1);

	menu_item = gtk_menu_item_new_with_label ( _("Size"));
        gtk_menu_append (GTK_MENU (menu), menu_item);
	gtk_object_set_data (GTK_OBJECT (menu_item), "SORT_ORDER_CODE", (gpointer) SORT_SIZE);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
				   GTK_SIGNAL_FUNC (sort_callback), cbox1);

        menu_item = gtk_menu_item_new_with_label ( _("Time Last Accessed"));
        gtk_menu_append (GTK_MENU (menu), menu_item);
	gtk_object_set_data (GTK_OBJECT (menu_item), "SORT_ORDER_CODE", (gpointer) SORT_ACCESS);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
				   GTK_SIGNAL_FUNC (sort_callback), cbox1);

        menu_item = gtk_menu_item_new_with_label ( _("Time Last Modified"));
        gtk_menu_append (GTK_MENU (menu), menu_item);
	gtk_object_set_data (GTK_OBJECT (menu_item), "SORT_ORDER_CODE", (gpointer) SORT_MODIFY);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
				   GTK_SIGNAL_FUNC (sort_callback), cbox1);

        menu_item = gtk_menu_item_new_with_label ( _("Time Last Changed"));
        gtk_menu_append (GTK_MENU (menu), menu_item);
	gtk_object_set_data (GTK_OBJECT (menu_item), "SORT_ORDER_CODE", (gpointer) SORT_CHANGE);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
				   GTK_SIGNAL_FUNC (sort_callback), cbox1);

	gtk_widget_show_all (menu);
        gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);

	
	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (cbox1), panel->case_sensitive);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (sort_box)->vbox),
			    cbox1, FALSE, FALSE, 0);

	cbox2 = gtk_check_button_new_with_label (N_("Reverse the order."));
	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (cbox2), panel->reverse);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (sort_box)->vbox),
			    cbox2, FALSE, FALSE, 0);
	/* off to the races */
	gtk_widget_show_all (GNOME_DIALOG (sort_box)->vbox);
	switch (gnome_dialog_run (GNOME_DIALOG (sort_box))) {
	case 0:
		switch( (gint) gtk_object_get_data (GTK_OBJECT
						    (GTK_OPTION_MENU
						     (omenu)->menu_item),
						    "SORT_ORDER_CODE") ) {
		case SORT_NAME:
			sfn = sort_name;
			break;
		case SORT_EXTENSION:
			sfn = sort_ext;
			break;
		case SORT_ACCESS:
			sfn = sort_atime;
			break;
		case SORT_MODIFY:
			sfn = sort_time;
			break;
		case SORT_CHANGE:
			sfn = sort_ctime;
			break;
		case SORT_SIZE:
			sfn = sort_size;
			break;
		}
		/* case sensitive */
		panel->case_sensitive = GTK_TOGGLE_BUTTON (cbox1)->active;
		/* Reverse order */
		panel->reverse = GTK_TOGGLE_BUTTON (cbox2)->active;

		panel_set_sort_order (panel, sfn);
		break;
	case 1:
	default:
		break;
	}
	gtk_widget_destroy (sort_box);
}
void
gnome_external_panelize (GtkWidget *widget, WPanel *panel)
{
	GtkWidget *ep_dlg;
	GtkWidget *frame;
	GtkWidget *clist;
	GtkWidget *sw;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *entry;

	ep_dlg = gnome_dialog_new (_("Run Command"), GNOME_STOCK_BUTTON_OK, 
				   GNOME_STOCK_BUTTON_CANCEL, NULL);

				/* Frame 1 */
	frame = gtk_frame_new (_("Preset Commands"));
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ep_dlg)->vbox),
			    frame, FALSE, FALSE, 0);
	clist = gtk_clist_new (1);
	gtk_clist_set_column_auto_resize (GTK_CLIST (clist), 1, TRUE);
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	sw = gtk_scrolled_window_new (GTK_CLIST (clist)->hadjustment, GTK_CLIST (clist)->vadjustment);
	gtk_widget_set_usize (sw, 300, 100);
	gtk_container_add (GTK_CONTAINER (sw), clist);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	button = gtk_button_new_with_label (_("Add"));
	gtk_widget_set_usize (button, 75, 25);
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	button = gtk_button_new_with_label (_("Remove"));
	gtk_widget_set_usize (button, 75, 25);
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

				/* Frame 2 */
	frame = gtk_frame_new (_("Run this Command"));
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ep_dlg)->vbox),
			    frame, FALSE, FALSE, 0);
	entry = gtk_entry_new ();
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("Command: ")), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (frame), hbox);
	gtk_widget_show_all (GNOME_DIALOG (ep_dlg)->vbox);
	gnome_dialog_run_and_close (GNOME_DIALOG (ep_dlg));

}
void
gnome_select_all_cmd (GtkWidget *widget, WPanel *panel)
{
	gint i;
	for (i = 0; i < panel->count; i++){
		if (!strcmp (panel->dir.list [i].fname, ".."))
			continue;
		do_file_mark (panel, i, 1);
	}
	paint_panel (panel);
	do_refresh ();
}
