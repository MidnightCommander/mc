/* Drag and Drop functionality for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#include <config.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "file.h"
#include "main.h"
#include "panel.h"
#include "gscreen.h"
#include "../vfs/vfs.h"
#include <gdk/gdkprivate.h>
#include "gdnd.h"


/* The menu of DnD actions */
static GnomeUIInfo actions[] = {
	GNOMEUIINFO_ITEM_NONE (N_("Move here"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("Copy here"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("Link here"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("Cancel drag"), NULL, NULL),
	GNOMEUIINFO_END
};

/* Pops up a menu of actions to perform on dropped files */
static GdkDragAction
get_action (void)
{
	GtkWidget *menu;
	int a;
	GdkDragAction action;

	menu = gnome_popup_menu_new (actions);
	a = gnome_popup_menu_do_popup_modal (menu, NULL, NULL, NULL, NULL);

	switch (a) {
	case 0:
		action = GDK_ACTION_MOVE;
		break;

	case 1:
		action = GDK_ACTION_COPY;
		break;

	case 2:
		action = GDK_ACTION_LINK;
		break;

	default:
		action = GDK_ACTION_ASK; /* Magic value to indicate cancellation */
	}

	gtk_widget_destroy (menu);

	return action;
}

/*
 * Looks for a panel that has the specified window for its list
 * display.  It is used to figure out if we are receiving a drop from
 * a panel on this MC process.  If no panel is found, it returns NULL.
 */
static WPanel *
find_panel_owning_window (GdkDragContext *context)
{
	GList *list;
	WPanel *panel;
	GtkWidget *source_widget, *toplevel_widget;

	source_widget = gtk_drag_get_source_widget (context);
	if (!source_widget)
		return NULL;

	/*
	 * We will scan the list of existing WPanels.  We
	 * uniformize the thing by pulling the toplevel
	 * widget for each WPanel and compare this to the
	 * toplevel source_widget
	 */
	toplevel_widget = gtk_widget_get_toplevel (source_widget);

	for (list = containers; list; list = list->next) {
		GtkWidget *panel_toplevel_widget;
		
		panel = ((PanelContainer *) list->data)->panel;

		panel_toplevel_widget = panel->xwindow;

		if (panel->xwindow == toplevel_widget){

			/*
			 * Now a WPanel actually contains a number of
			 * drag sources.  If the drag source is the
			 * Tree, we must report that it was not the
			 * contents of the WPanel
			 */

			if (source_widget == panel->tree)
				return NULL;
			
			return panel;
		}
	}

	return NULL;
}

/*
 * Performs a drop action on the specified panel.  Only supports copy
 * and move operations.  The files are moved or copied to the
 * specified destination directory.
 */
static void
perform_action_on_panel (WPanel *source_panel, GdkDragAction action, char *destdir, int ask)
{
	switch (action) {
	case GDK_ACTION_COPY:
		if (ask)
			panel_operate (source_panel, OP_COPY, destdir);
		else
			panel_operate_def (source_panel, OP_COPY, destdir);
		break;

	case GDK_ACTION_MOVE:
		if (ask)
			panel_operate (source_panel, OP_MOVE, destdir);
		else
			panel_operate_def (source_panel, OP_MOVE, destdir);
		break;

	default:
		g_assert_not_reached ();
	}

	/* Finish up */

	update_one_panel_widget (source_panel, FALSE, UP_KEEPSEL);

	if (action == GDK_ACTION_MOVE)
		panel_update_contents (source_panel);
}

/*
 * Performs handling of symlinks via drag and drop.  This should go
 * away when operation windows support links.
 */
static void
perform_links (GList *names, char *destdir)
{
	char *name;
	char *dest_name;

	for (; names; names = names->next) {
		name = names->data;
		if (strncmp (name, "file:", 5) == 0)
			name += 5;

		dest_name = g_concat_dir_and_file (destdir, x_basename (name));
		mc_symlink (name, dest_name);
		g_free (dest_name);
	}
}

/*
 * Performs a drop action manually, by going through the list of files
 * to operate on.  The files are copied or moved to the specified
 * directory.  This should also encompass symlinking when the file
 * operations window supports links.
 */
static void
perform_action (GList *names, GdkDragAction action, char *destdir)
{
	struct stat s;
	char *name;
	char *dest_name;
	int result;

	switch (action) {
	case GDK_ACTION_COPY:
		create_op_win (OP_COPY, FALSE);
		break;

	case GDK_ACTION_MOVE:
		create_op_win (OP_MOVE, FALSE);
		break;

	default:
		g_assert_not_reached ();
	}

	file_mask_defaults ();

	for (; names; names = names->next) {
		name = names->data;
		if (strncmp (name, "file:", 5) == 0)
			name += 5;

		dest_name = g_concat_dir_and_file (destdir, x_basename (name));

		do {
			result = mc_stat (name, &s);

			if (result != 0) {
				/* FIXME: this error message sucks */
				if (file_error (_("Could not stat %s\n%s"), dest_name) != FILE_RETRY)
					result = 0;
			} else {
				long count = 0;
				double bytes = 0;
					
				if (S_ISDIR (s.st_mode)) {
					if (action == GDK_ACTION_COPY)
						copy_dir_dir (
							name, dest_name,
							TRUE, FALSE,
							FALSE, FALSE,
							&count, &bytes);
					else
						move_dir_dir (
							name, dest_name,
							&count, &bytes);
				} else {
					if (action == GDK_ACTION_COPY)
						copy_file_file (
							name, dest_name,
							TRUE,
							&count, &bytes, 1);
					else
						move_file_file (
							name, dest_name,
							&count, &bytes);
				}
			}
		} while (result != 0);

		g_free (dest_name);
		break;
	}

	destroy_op_win ();
}

/**
 * gdnd_drop_on_directory:
 * @context: The drag context received from the drag_data_received callback
 * @selection_data: The selection data from the drag_data_received callback
 * @dirname: The name of the directory to drop onto
 * 
 * Extracts an URI list from the selection data and drops all the files in the
 * specified directory.
 *
 * Return Value: TRUE if the drop was sucessful, FALSE if it was not.
 **/
int
gdnd_drop_on_directory (GdkDragContext *context, GtkSelectionData *selection_data, char *destdir)
{
	GdkDragAction action;
	WPanel *source_panel;
	GList *names;

	if (context->suggested_action == GDK_ACTION_ASK) {
		action = get_action ();

		if (action == GDK_ACTION_ASK)
			return FALSE;
		
	} else
		action = context->suggested_action;

	/* If we are dragging from a file panel, we can display a nicer status display */
	source_panel = find_panel_owning_window (context);

	printf ("Panel found for this source: %p\n", source_panel);
	/* Symlinks do not use file.c */

	if (source_panel && action != GDK_ACTION_LINK)
		perform_action_on_panel (source_panel, action, destdir, context->suggested_action == GDK_ACTION_ASK);
	else {
		names = gnome_uri_list_extract_uris (selection_data->data);

		if (action == GDK_ACTION_LINK)
			perform_links (names, destdir);
		else
			perform_action (names, action, destdir);

		gnome_uri_list_free_strings (names);
	}

	return TRUE;
}
