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
#include "profile.h"
#include "setup.h"
#include "panelize.h"
#include "gcmd.h"
#include "dialog.h"
#include "layout.h"
#include "gdesktop.h"
#include "gmain.h"
#include "../vfs/vfs.h"

static char *panelize_section = "Panelize";

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
gnome_open_terminal_with_cmd (const char *command)
{
	char *p;
	int quote_all = 0;

	if (!(p = gnome_is_program_in_path ("gnome-terminal"))){
		if (!(p = gnome_is_program_in_path ("dtterm")))
			if (!(p = gnome_is_program_in_path ("nxterm")))
				if (!(p = gnome_is_program_in_path ("color-xterm")))
					if (!(p = gnome_is_program_in_path ("rxvt")))
						p = gnome_is_program_in_path ("xterm");
	} else
		quote_all = 1;

	if (p){
		if (command){
			char *q;

			if (quote_all)
				q = g_strconcat (p, " -e '", command, "'", NULL);
			else
				q = g_strconcat (p, " -e ", command, NULL);
			my_system (EXECUTE_AS_SHELL, shell, q);
			g_free (q);
		} else
			my_system (EXECUTE_AS_SHELL, shell, p);

		g_free (p);
	} else
		message (1, MSG_ERROR, " Could not start a terminal ");
}

void
gnome_open_terminal (void)
{
	gnome_open_terminal_with_cmd (NULL);
}

void
gnome_about_cmd (void)
{
        GtkWidget *about;
	static int translated;
        static const gchar *authors[] = {
		N_("The Midnight Commander Team"),
		"http://www.gnome.org/mc/",
		N_("bug reports: http://bugs.gnome.org, or use gnome-bug"),
		NULL
	};

	if (!translated){
		int i;

		for (i = 0; authors [i]; i++)
			authors [i] = _(authors [i]);
		translated = TRUE;
	}

        about = gnome_about_new (_("GNU Midnight Commander"), VERSION,
				 "Copyright 1994-1999 the Free Software Foundation",
				 authors,
				 _("The GNOME edition of the Midnight Commander file manager."),
				 NULL);
        gtk_widget_show (about);
}

void
gnome_open_panel (GtkWidget *widget, WPanel *panel)
{
	new_panel_at (panel->cwd);
}

void
gnome_close_panel (GtkWidget *widget, WPanel *panel)
{
	Dlg_head *h = panel->widget.parent;
	if (panel->timer_id){
		gtk_timeout_remove (panel->timer_id);
		panel->timer_id = -1;
	}

	/* Remove the widgets from the dialog head */
	remove_widget (h, panel->current_dir);
	remove_widget (h, panel);

	/* Free our own internal stuff */
	g_free (panel->view_menu_items);
	g_free (panel->view_toolbar_items);

	/* Kill the widgets */
	destroy_widget (panel->current_dir);
	destroy_widget ((Widget *)panel);

	layout_panel_gone (panel);
	mc_chdir ("/");
}

static void
set_list_type (GtkWidget *widget, WPanel *panel, enum list_types type)
{
	int i;

	/* This is kind of a hack to see whether we need to do something or not.
	 * This function (or at least the callback that calls it) can refer to a
	 * radio menu item or a radio button, so we need to see if it is active.
	 */

	if (GTK_OBJECT_TYPE (widget) == gtk_radio_menu_item_get_type ()) {
		if (!GTK_CHECK_MENU_ITEM (widget)->active)
			return;
	} else if (GTK_OBJECT_TYPE (widget) == gtk_radio_button_get_type ()) {
		if (!GTK_TOGGLE_BUTTON (widget)->active)
			return;
	} else
		g_assert_not_reached ();

	/* Worth the effort? */

	if (panel->list_type == type)
		return;

        /* Set the list type */

	panel->list_type = type;
	set_panel_formats (panel);
	paint_panel (panel);
	do_refresh ();

	/* Synchronize the widgets */

	for (i = 0; panel->view_menu_items[i] && panel->view_toolbar_items[i]; i++)
		if (widget == panel->view_menu_items[i]) {
			gtk_signal_handler_block_by_data (
				GTK_OBJECT (panel->view_toolbar_items[i]), panel);
			gtk_toggle_button_set_active (
				GTK_TOGGLE_BUTTON (panel->view_toolbar_items[i]), TRUE);
			gtk_signal_handler_unblock_by_data (
				GTK_OBJECT (panel->view_toolbar_items[i]), panel);
			return;
		} else if (widget == panel->view_toolbar_items[i]) {
			gtk_signal_handler_block_by_data (
				GTK_OBJECT (panel->view_menu_items[i]), panel);
			gtk_check_menu_item_set_active (
				GTK_CHECK_MENU_ITEM (panel->view_menu_items[i]), TRUE);
			gtk_signal_handler_unblock_by_data (
				GTK_OBJECT (panel->view_menu_items[i]), panel);
			return;
		}

	g_assert_not_reached ();
}

void
gnome_icon_view_cmd (GtkWidget *widget, WPanel *panel)
{
	set_list_type (widget, panel, list_icons);
}

void
gnome_brief_view_cmd (GtkWidget *widget, WPanel *panel)
{
	set_list_type (widget, panel, list_brief);
}

void
gnome_detailed_view_cmd (GtkWidget *widget, WPanel *panel)
{
	set_list_type (widget, panel, list_full);
}

void
gnome_custom_view_cmd (GtkWidget *widget, WPanel *panel)
{
	set_list_type (widget, panel, list_user);
}

static void
sort_callback (GtkWidget *menu_item, GtkWidget *cbox1)
{
	if (gtk_object_get_data (GTK_OBJECT (menu_item), "SORT_ORDER_CODE") == SORT_NAME)
		gtk_widget_set_sensitive (cbox1, TRUE);
	else
		gtk_widget_set_sensitive (cbox1, FALSE);
}

/* Returns a sort function based on its type */
sortfn *
sort_get_func_from_type (SortType type)
{
	sortfn *sfn = NULL;

	switch (type) {
	case SORT_NAME:
		sfn = (sortfn *) sort_name;
		break;

	case SORT_EXTENSION:
		sfn = (sortfn *) sort_ext;
		break;

	case SORT_ACCESS:
		sfn = (sortfn *) sort_atime;
		break;

	case SORT_MODIFY:
		sfn = (sortfn *) sort_time;
		break;

	case SORT_CHANGE:
		sfn = (sortfn *) sort_ctime;
		break;

	case SORT_SIZE:
		sfn = (sortfn *) sort_size;
		break;

	default:
		g_assert_not_reached ();
	}

	return sfn;
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
	sortfn *sfn = NULL;

	sort_box = gnome_dialog_new (_("Sort By"), GNOME_STOCK_BUTTON_OK,
				     GNOME_STOCK_BUTTON_CANCEL, NULL);
	gmc_window_setup_from_panel (GNOME_DIALOG (sort_box), panel);

	/* we define this up here so we can pass it in to our callback */
	cbox1 = gtk_check_button_new_with_label (_("Ignore case sensitivity."));
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (sort_box)->vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("Sort files by ")), FALSE, FALSE, 0);

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

	cbox2 = gtk_check_button_new_with_label (_("Reverse the order."));
	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (cbox2), panel->reverse);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (sort_box)->vbox),
			    cbox2, FALSE, FALSE, 0);
	/* off to the races */
	gtk_widget_show_all (GNOME_DIALOG (sort_box)->vbox);
	switch (gnome_dialog_run (GNOME_DIALOG (sort_box))) {
	case 0:
		sfn = sort_get_func_from_type(
				GPOINTER_TO_UINT(gtk_object_get_data (GTK_OBJECT
						    (GTK_OPTION_MENU
						     (omenu)->menu_item),
						    "SORT_ORDER_CODE")));
		/* case sensitive */
		panel->case_sensitive = GTK_TOGGLE_BUTTON (cbox1)->active;
		/* Reverse order */
		panel->reverse = GTK_TOGGLE_BUTTON (cbox2)->active;

		panel_set_sort_order (panel, sfn);
		break;
	case 1:
		break;
	default:
		return;
	}
	gtk_widget_destroy (sort_box);
}

typedef struct ep_dlg_data {
	GtkWidget *ep_dlg;
	GtkWidget *clist;
	GtkWidget *entry;
	GtkWidget *add_button;
	GtkWidget *remove_button;
	gboolean setting_text;
	gint selected; /* if this is -1 then nothing is selected, otherwise, it's the row selected */
} ep_dlg_data;

static gchar *
get_nickname (gchar *text)
{
	GtkWidget *dlg;
	GtkWidget *entry;
	GtkWidget *label;
	gchar *retval = NULL;
	int destroy;

	dlg = gnome_dialog_new (_("Enter name."), GNOME_STOCK_BUTTON_OK,
				GNOME_STOCK_BUTTON_CANCEL, NULL);
	gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_MOUSE);
	entry = gtk_entry_new ();
	if (text)
		gtk_entry_set_text (GTK_ENTRY (entry), text);
	label = gtk_label_new (_("Enter label for command:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dlg)->vbox),
			    label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dlg)->vbox),
			    entry, FALSE, FALSE, 0);
	gtk_widget_show_all (GNOME_DIALOG (dlg)->vbox);
	destroy = TRUE;
	switch (gnome_dialog_run (GNOME_DIALOG (dlg))) {
	case -1:
		destroy = FALSE;
		break;
	case 0:
		retval = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
		break;
	case 1:
	default:
	}
	if (destroy)
		gtk_widget_destroy (dlg);

	return retval;
}

static void
ep_add_callback (GtkWidget *widget, ep_dlg_data *data)
{
	gint i;
	gchar *insert_tab[1];

	insert_tab[0] = get_nickname (NULL);
	if (insert_tab[0] == NULL)
		return;
	i = gtk_clist_append (GTK_CLIST (data->clist), insert_tab);
	gtk_clist_set_row_data (GTK_CLIST (data->clist), i,
				g_strdup (gtk_entry_get_text (GTK_ENTRY (data->entry))));
	g_free (insert_tab [0]);
	data->selected = -1;
	gtk_widget_set_sensitive (data->add_button, FALSE);
	gtk_entry_set_text (GTK_ENTRY (data->entry), "");
}

static void
ep_remove_callback (GtkWidget *widget, ep_dlg_data *data)
{
	if (data->selected > -1) {
		g_free (gtk_clist_get_row_data (GTK_CLIST (data->clist), data->selected));
		gtk_clist_remove (GTK_CLIST (data->clist), data->selected);
		data->selected = -1;
		gtk_entry_set_text (GTK_ENTRY (data->entry), "");
	}
	gtk_widget_set_sensitive (data->remove_button, FALSE);
}

static void
ep_select_callback (GtkWidget *widget,
		    gint row,
		    gint column,
		    GdkEventButton *event,
		    ep_dlg_data *data)
{
	if (event && event->type == GDK_2BUTTON_PRESS) {
		gchar *nick;

		gtk_clist_get_text (GTK_CLIST (widget), row, 0, &nick);
		/* ugly but correct... (: */
		nick = get_nickname (nick);
		gtk_clist_set_text (GTK_CLIST (data->clist), row, 0, nick);
		gtk_clist_select_row (GTK_CLIST (data->clist), row, 0);
	} else {
		data->setting_text = TRUE;
		gtk_entry_set_text (GTK_ENTRY (data->entry),
				    (gchar *) gtk_clist_get_row_data (GTK_CLIST (widget), row));
		data->setting_text = FALSE;
		data->selected = row;
		gtk_widget_set_sensitive (data->remove_button, TRUE);
		gtk_widget_set_sensitive (data->add_button, FALSE);
	}
}

static void
ep_text_changed_callback (GtkWidget *widget, ep_dlg_data *data)
{
	if (data->setting_text)
		/* we don't want to deselect text if we just clicked on something */
		return;
	if (data->selected > -1) {
		gtk_clist_unselect_row (GTK_CLIST (data->clist), data->selected, 0);
		data->selected = -1;
	}
	gtk_widget_set_sensitive (data->remove_button, FALSE);
	gtk_widget_set_sensitive (data->add_button, TRUE);
}

static void
load_settings (GtkCList *clist)
{
	gchar *insert_tab[1];
	void *profile_keys;
	gchar *key, *value;
	gint i = 0;

	profile_keys = profile_init_iterator (panelize_section, profile_name);

	if (!profile_keys){
		insert_tab[0] = _("Find all core files");
		i = gtk_clist_insert (clist, i, insert_tab);
		gtk_clist_set_row_data (clist, i, g_strdup ("find / -name core"));
		insert_tab[0] = _("Find rejects after patching");
		i = gtk_clist_insert (clist, i, insert_tab);
		gtk_clist_set_row_data (clist, i, g_strdup ("find . -name \\*.rej -print"));
	} else {
		while (profile_keys) {
			profile_keys = profile_iterator_next (profile_keys, &key, &value);
			insert_tab[0] = key;
			i = gtk_clist_insert (clist, i, insert_tab);
			gtk_clist_set_row_data (clist, i, g_strdup (value));
		}
	}
}

static void
save_settings (GtkCList *clist)
{
	gint i;
	gchar *text;

	profile_clean_section (panelize_section, profile_name);
	for (i = 0; i < GTK_CLIST (clist)->rows; i++) {
		gtk_clist_get_text (GTK_CLIST (clist), i, 0, &text);
		WritePrivateProfileString (panelize_section,
					   text,
					   (gchar *) gtk_clist_get_row_data (GTK_CLIST (clist), i),
					   profile_name);
	}
	sync_profiles ();
}

void
gnome_external_panelize (GtkWidget *widget, WPanel *panel)
{
	ep_dlg_data *data;
	GtkWidget *frame;
	GtkWidget *sw;
	GtkWidget *vbox;
	GtkWidget *hbox;
	gint i;
	gchar *row_data;
	int destroy;

	data = g_new0 (ep_dlg_data, 1);
	data->setting_text = FALSE;
	data->selected = -1;
	data->ep_dlg = gnome_dialog_new (_("Run Command"), GNOME_STOCK_BUTTON_OK,
				   GNOME_STOCK_BUTTON_CANCEL, NULL);
	gtk_window_set_position (GTK_WINDOW (data->ep_dlg), GTK_WIN_POS_MOUSE);

				/* Frame 1 */
	frame = gtk_frame_new (_("Preset Commands"));
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (data->ep_dlg)->vbox),
			    frame, FALSE, FALSE, 0);
	data->clist = gtk_clist_new (1);
	load_settings (GTK_CLIST (data->clist));
	gtk_signal_connect (GTK_OBJECT (data->clist), "select_row", GTK_SIGNAL_FUNC (ep_select_callback), (gpointer) data);
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_clist_columns_autosize (GTK_CLIST (data->clist));
	gtk_clist_set_auto_sort (GTK_CLIST (data->clist), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	sw = gtk_scrolled_window_new (GTK_CLIST (data->clist)->hadjustment, GTK_CLIST (data->clist)->vadjustment);
	gtk_widget_set_usize (sw, 300, 100);
	gtk_container_add (GTK_CONTAINER (sw), data->clist);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	data->add_button = gtk_button_new_with_label (_("Add"));
	gtk_signal_connect (GTK_OBJECT (data->add_button), "clicked", GTK_SIGNAL_FUNC (ep_add_callback), (gpointer) data);
	gtk_widget_set_usize (data->add_button, 75, 25);
	gtk_box_pack_end (GTK_BOX (hbox), data->add_button, FALSE, FALSE, 0);
	data->remove_button = gtk_button_new_with_label (_("Remove"));
	gtk_widget_set_sensitive (data->remove_button, FALSE);
	gtk_signal_connect (GTK_OBJECT (data->remove_button), "clicked", GTK_SIGNAL_FUNC (ep_remove_callback), (gpointer) data);
	gtk_widget_set_usize (data->remove_button, 75, 25);
	gtk_box_pack_end (GTK_BOX (hbox), data->remove_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

				/* Frame 2 */
	frame = gtk_frame_new (_("Run this Command"));
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (data->ep_dlg)->vbox),
			    frame, FALSE, FALSE, 0);
	data->entry = gtk_entry_new ();
	gtk_signal_connect (GTK_OBJECT (data->entry), "changed", GTK_SIGNAL_FUNC (ep_text_changed_callback), (gpointer) data);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("Command: ")), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), data->entry, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (frame), hbox);
	gtk_widget_show_all (GNOME_DIALOG (data->ep_dlg)->vbox);

	destroy = TRUE;

	switch (gnome_dialog_run (GNOME_DIALOG (data->ep_dlg))) {
	case 0:
		gtk_widget_hide (data->ep_dlg);
		while (gtk_events_pending () )
			gtk_main_iteration ();

		do_external_panelize (gtk_entry_get_text (GTK_ENTRY (data->entry)));
		save_settings (GTK_CLIST (data->clist));
		break;
	case 1:
		break;
	case -1:
		destroy = FALSE;
		break;
	default:
	}
	for (i = 0; i < GTK_CLIST (data->clist)->rows; i++) {
		row_data = gtk_clist_get_row_data (GTK_CLIST (data->clist), i);
		if (row_data)
			g_free (row_data);
	}

	if (destroy)
		gtk_widget_destroy (GTK_WIDGET (data->ep_dlg));
	g_free (data);
}

void
gnome_select_all_cmd (GtkWidget *widget, WPanel *panel)
{
	gint i;
	for (i = 0; i < panel->count; i++){
		if (!strcmp (panel->dir.list [i].fname, "..")) {
			continue;
		}
		do_file_mark (panel, i, 1);
	}
	paint_panel (panel);
	do_refresh ();
}

void
gnome_start_search (GtkWidget *widget, WPanel *panel)
{
	start_search (panel);
}

void
gnome_reverse_selection_cmd_panel (GtkWidget *widget, WPanel *panel)
{
	file_entry *file;
	int i;

	for (i = 0; i < panel->count; i++) {
		if (!strcmp (panel->dir.list [i].fname, ".."))
			continue;

		file = &panel->dir.list [i];
		do_file_mark (panel, i, !file->f.marked);
	}

	paint_panel (panel);
}

void
gnome_filter_cmd (GtkWidget *widget, WPanel *panel)
{
	GtkWidget *filter_dlg;
	GtkWidget *entry;
	GtkWidget *label;
	gchar *text1, *text2, *text3;

	filter_dlg = gnome_dialog_new (_("Set Filter"), GNOME_STOCK_BUTTON_OK,
				       GNOME_STOCK_BUTTON_CANCEL, NULL);
	gtk_window_set_position (GTK_WINDOW (filter_dlg), GTK_WIN_POS_MOUSE);
	if (easy_patterns) {
		text1 = "mc_filter_globs";
		text3 = _("Show all files");
		if (panel->filter && (strcmp (panel->filter, "*")))
			text2 = panel->filter;
		else
			text2 = NULL;
	} else {
		text1 = ("mc_filter_regexps");
		text3 = _(".");
		if (!panel->filter)
			text2 = NULL;
		else
			text2 = panel->filter;
	}
	entry = gnome_entry_new (text1);
	gnome_entry_load_history (GNOME_ENTRY (entry));

	if (text2) {
		gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (entry))), text2);
		gnome_entry_prepend_history (GNOME_ENTRY (entry), FALSE, text3);
	} else
		gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (entry))), text3);

	if (easy_patterns)
		label = gtk_label_new (_("Enter a filter here for files in the panel view.\n\nFor example:\n*.png will show just png images"));
	else
		label = gtk_label_new (_("Enter a Regular Expression to filter files in the panel view."));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (filter_dlg)->vbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (filter_dlg)->vbox), entry, FALSE, FALSE, 0);
	gtk_widget_show_all (GNOME_DIALOG (filter_dlg)->vbox);
	switch (gnome_dialog_run (GNOME_DIALOG (filter_dlg))) {
	case 0:
		gtk_widget_hide (filter_dlg);
		if (panel->filter) {
			g_free (panel->filter);
			panel->filter = NULL;
		}
		panel->filter = g_strdup (gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (entry)))));
		if (!strcmp (_("Show all files"), panel->filter)) {
			g_free (panel->filter);
			panel->filter = NULL;
			gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (entry))),
					    "");
			gtk_label_set_text (GTK_LABEL (panel->status), _("Show all files"));
		} else if (!strcmp ("*", panel->filter)) {
			g_free (panel->filter);
			panel->filter = NULL;
			gtk_label_set_text (GTK_LABEL (panel->status), _("Show all files"));
		} else
			gtk_label_set_text (GTK_LABEL (panel->status), panel->filter);

		gnome_entry_save_history (GNOME_ENTRY (entry));
		x_filter_changed (panel);
		reread_cmd ();
		break;

	case -1:
		return;
	}
	gtk_widget_destroy (filter_dlg);
}

void
gnome_open_files (GtkWidget *widget, WPanel *panel)
{
	GList *later = NULL;
#if 0
	GList *now;
#endif
	gint i;

	/* FIXME: this is the easy way to do things.  We want the
	 * hard way sometime. */
	for (i = 0; i < panel->count; i++) {
		if (panel->dir.list [i].f.marked)
			if (!do_enter_on_file_entry ((panel->dir.list) + i))
				later = g_list_prepend (later, panel->dir.list + i);
	}
#if 0
	/* This is sorta ugly.  Should we just skip these? There should be a better way. */
	for (now = later; now; now = now->next) {
		gchar *command;
		command = input_expand_dialog (_(" Open with..."),
					       _("Enter extra arguments:"), (WPanel *) now->data);
		if (!command)
			/* we break out. */
			break;
		execute (command);
		 g_free (command);
	}
#endif
	g_list_free (later);

}

void
gnome_run_new (GtkWidget *widget, GnomeDesktopEntry *gde)
{
	gnome_desktop_entry_launch (gde);
}

void
gnome_mkdir_cmd (GtkWidget *widget, WPanel *panel)
{
	mkdir_cmd (panel);
}

void
gnome_newfile_cmd (GtkWidget *widget, WPanel *panel)
{
	mc_chdir (panel->cwd);
	gmc_edit ("");
}

static void
dentry_apply_callback(GtkWidget *widget, int page, gpointer data)
{
	GnomeDesktopEntry *dentry;
	gchar *desktop_directory;

	if (page != -1)
		return;

	g_return_if_fail(data!=NULL);
	g_return_if_fail(GNOME_IS_DENTRY_EDIT(data));
	dentry = gnome_dentry_get_dentry(GNOME_DENTRY_EDIT(data));

	if (getenv ("GNOME_DESKTOP_DIR") != NULL)
		desktop_directory = g_strconcat (getenv ("GNOME_DESKTOP_DIR"),
						 "/",
						 dentry->name, ".desktop",
						 NULL);
	else
		desktop_directory = g_strconcat (gnome_user_home_dir, "/",
						 DESKTOP_DIR_NAME, "/",
						 dentry->name, ".desktop",
						 NULL);

	dentry->location = desktop_directory;
	gnome_desktop_entry_save(dentry);
	gnome_desktop_entry_free(dentry);
	gnome_config_sync ();
	desktop_reload_icons (FALSE, 0, 0);

}

void
gnome_new_launcher (GtkWidget *widget, WPanel *panel)
{
	GtkWidget *dialog;
	GtkObject *dentry;

	dialog = gnome_property_box_new ();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Desktop entry properties"));
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	dentry = gnome_dentry_edit_new_notebook(GTK_NOTEBOOK(GNOME_PROPERTY_BOX(dialog)->notebook));
	gtk_signal_connect_object(GTK_OBJECT(dentry), "changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(dialog));

	gtk_signal_connect(GTK_OBJECT(dialog), "apply",
			   GTK_SIGNAL_FUNC(dentry_apply_callback),
			   dentry);
	gtk_widget_show(dialog);


}
void
gnome_select (GtkWidget *widget, WPanel *panel)
{
    char *reg_exp, *reg_exp_t;
    int i;
    int c;
    int dirflag = 0;
    GtkWidget *select_dialog;
    GtkWidget *entry;
    GtkWidget *label;
    int run;

    select_dialog = gnome_dialog_new (_("Select File"), GNOME_STOCK_BUTTON_OK,
				      GNOME_STOCK_BUTTON_CANCEL, NULL);
    gtk_window_set_position (GTK_WINDOW (select_dialog), GTK_WIN_POS_MOUSE);
    entry = gnome_entry_new ("mc_select");
    gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (entry))), easy_patterns ? "*" : ".");
    gnome_entry_load_history (GNOME_ENTRY (entry));

    if (easy_patterns)
	    label = gtk_label_new (_("Enter a filter here to select files in the panel view with.\n\nFor example:\n*.png will select all png images"));
    else
	    label = gtk_label_new (_("Enter a regular expression here to select files in the panel view with."));

    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (select_dialog)->vbox), label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (select_dialog)->vbox), entry, FALSE, FALSE, 0);
    gtk_widget_show_all (GNOME_DIALOG (select_dialog)->vbox);
    reg_exp = NULL;
    run = gnome_dialog_run (GNOME_DIALOG (select_dialog));
    if (run == 0) {
	    gtk_widget_hide (select_dialog);
	    reg_exp = g_strdup (gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (entry)))));
    }
    if (run != -1)
	    gtk_widget_destroy (select_dialog);

    if ((reg_exp == NULL) || (*reg_exp == '\000')) {
	    g_free (reg_exp);
	    return;
    }
    reg_exp_t = reg_exp;

    /* Check if they specified a directory */
    if (*reg_exp_t == PATH_SEP){
        dirflag = 1;
        reg_exp_t++;
    }
    if (reg_exp_t [strlen(reg_exp_t) - 1] == PATH_SEP){
        dirflag = 1;
        reg_exp_t [strlen(reg_exp_t) - 1] = 0;
    }

    for (i = 0; i < panel->count; i++){
        if (!strcmp (panel->dir.list [i].fname, ".."))
            continue;
	if (S_ISDIR (panel->dir.list [i].buf.st_mode)){
	    if (!dirflag)
                continue;
        } else {
            if (dirflag)
                continue;
	}
	c = regexp_match (reg_exp_t, panel->dir.list [i].fname, match_file);
	if (c == -1){
	    message (1, MSG_ERROR, _("  Malformed regular expression  "));
	    g_free (reg_exp);
	    return;
	}
	if (c){
	    do_file_mark (panel, i, 1);
	}
    }
    paint_panel (panel);
    g_free (reg_exp);
}
void
set_cursor_busy (WPanel *panel)
{
	GdkCursor *cursor;

	if (is_a_desktop_panel (panel))
		return;

	cursor = gdk_cursor_new (GDK_WATCH);
	gdk_window_set_cursor (GTK_WIDGET (panel->xwindow)->window, cursor);
	gdk_cursor_destroy (cursor);
	gdk_flush ();

}
void
set_cursor_normal (WPanel *panel)
{
	GdkCursor *cursor;

	if (is_a_desktop_panel (panel))
		return;

	cursor = gdk_cursor_new (GDK_TOP_LEFT_ARROW);
	gdk_window_set_cursor (GTK_WIDGET (panel->xwindow)->window, cursor);
	gdk_cursor_destroy (cursor);
	gdk_flush ();
}

void
gnome_new_link (GtkWidget *widget, WPanel *panel)
{
	char *template;
	char *url;

	url = input_expand_dialog (_("Creating a desktop link"),
				   _("Enter the URL:"), "");
	if (!url)
		return;

	template = g_concat_dir_and_file (desktop_directory, "urlXXXXXX");

	if (mktemp (template)) {
		char *icon;

		icon = g_concat_dir_and_file (ICONDIR, "gnome-http-url.png");
		desktop_create_url (template, url, url, icon);
		g_free (icon);
	}
	g_free (template);
	g_free (url);
}
