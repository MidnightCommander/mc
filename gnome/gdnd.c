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
#include "global.h"
#include "file.h"
#include "fileopctx.h"
#include "main.h"
#include "panel.h"
#include "ext.h"
#include "gscreen.h"
#include "../vfs/vfs.h"
#include <gdk/gdkprivate.h>
#include "gdesktop.h"
#include "gdnd.h"


/* Atoms for the DnD target types */
GdkAtom dnd_target_atoms[TARGET_NTARGETS];


/**
 * gdnd_init:
 *
 * Initializes the dnd_target_atoms array by interning the DnD target atom names.
 **/
void
gdnd_init (void)
{
	dnd_target_atoms[TARGET_MC_DESKTOP_ICON] =
		gdk_atom_intern (TARGET_MC_DESKTOP_ICON_TYPE, FALSE);

	dnd_target_atoms[TARGET_URI_LIST] =
		gdk_atom_intern (TARGET_URI_LIST_TYPE, FALSE);

	dnd_target_atoms[TARGET_TEXT_PLAIN] =
		gdk_atom_intern (TARGET_TEXT_PLAIN_TYPE, FALSE);

	dnd_target_atoms[TARGET_URL] =
		gdk_atom_intern (TARGET_URL_TYPE, FALSE);
}

/* The menu of DnD actions */
static GnomeUIInfo actions[] = {
	GNOMEUIINFO_ITEM_NONE (N_("_Move here"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("_Copy here"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("_Link here"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("Cancel drag"), NULL, NULL),
	GNOMEUIINFO_END
};

/* Pops up a menu of actions to perform on dropped files */
static GdkDragAction
get_action (GdkDragContext *context)
{
	GtkWidget *menu;
	int a;
	GdkDragAction action;

	/* Create the menu and set the sensitivity of the items based on the
	 * allowed actions.
	 */

	menu = gnome_popup_menu_new (actions);

	gtk_widget_set_sensitive (actions[0].widget, (context->actions & GDK_ACTION_MOVE) != 0);
	gtk_widget_set_sensitive (actions[1].widget, (context->actions & GDK_ACTION_COPY) != 0);
	gtk_widget_set_sensitive (actions[2].widget, (context->actions & GDK_ACTION_LINK) != 0);

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

/* Performs a drop action on the specified panel.  Only supports copy and move
 * operations.  The files are moved or copied to the specified destination
 * directory.
 */
static void
perform_action_on_panel (WPanel *source_panel, GdkDragAction action, char *destdir)
{
	switch (action) {
	case GDK_ACTION_COPY:
		panel_operate (source_panel, OP_COPY, destdir, FALSE);
		break;

	case GDK_ACTION_MOVE:
		panel_operate (source_panel, OP_MOVE, destdir, FALSE);
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

/* Performs a drop action manually, by going through the list of files to
 * operate on.  The files are copied or moved to the specified directory.  This
 * should also encompass symlinking when the file operations window supports
 * links.
 */
static void
perform_action (GList *names, GdkDragAction action, char *destdir)
{
	struct stat s;
	char *name;
	char *dest_name;
	int result;
	FileOpContext *ctx;

	ctx = file_op_context_new ();

	switch (action) {
	case GDK_ACTION_COPY:
		file_op_context_create_ui (ctx, OP_COPY, FALSE);
		break;

	case GDK_ACTION_MOVE:
		file_op_context_create_ui (ctx, OP_MOVE, FALSE);
		break;

	default:
		g_assert_not_reached ();
	}

	for (; names; names = names->next) {
		name = names->data;
		if (strncmp (name, "file:", 5) == 0)
			name += 5;

		dest_name = g_concat_dir_and_file (destdir, x_basename (name));

		do {
			result = mc_lstat (name, &s);

			if (result != 0) {
				/* FIXME: this error message sucks */
				if (file_error (_("Could not stat %s\n%s"), dest_name) != FILE_RETRY)
					result = 0;
			} else {
				long count = 0;
				double bytes = 0;

				if (S_ISDIR (s.st_mode)) {
					if (action == GDK_ACTION_COPY)
						copy_dir_dir (ctx,
							      name, dest_name,
							      TRUE, FALSE,
							      FALSE, FALSE,
							      &count, &bytes);
					else
						move_dir_dir (ctx,
							      name, dest_name,
							      &count, &bytes);
				} else {
					if (action == GDK_ACTION_COPY)
						copy_file_file (ctx,
								name, dest_name,
								TRUE,
								&count, &bytes, 1);
					else
						move_file_file (ctx,
								name, dest_name,
								&count, &bytes);
				}
			}
		} while (result != 0);

		g_free (dest_name);
	}

	file_op_context_destroy (ctx);
}

/* Drop a URI list on a directory.  If the data comes from a panel, use the nice
 * MC progress display; otherwise `do it by hand'.
 */
static void
drop_uri_list_on_directory (GdkDragContext *context, GtkSelectionData *selection_data,
			    GdkDragAction action, char *destdir)
{
	WPanel *source_panel;
	GtkWidget *source_widget;
	GList *names;

	source_panel = gdnd_find_panel_by_drag_context (context, &source_widget);

	/* We cannot use file.c if we are going to symlink or if we are dragging
         * from a tree.
	 */
	if (source_panel && source_widget != source_panel->tree && action != GDK_ACTION_LINK)
		perform_action_on_panel (source_panel, action, destdir);
	else {
		names = gnome_uri_list_extract_uris (selection_data->data);

		if (action == GDK_ACTION_LINK)
			perform_links (names, destdir);
		else
			perform_action (names, action, destdir);

		gnome_uri_list_free_strings (names);
	}
}

/* Drop a Netscape URL in a directory */
static void
drop_url_on_directory (GdkDragContext *context, GtkSelectionData *selection_data, char *destdir)
{
	char *template;

	template = g_concat_dir_and_file (destdir, "urlXXXXXX");

	if (mktemp (template)) {
		char *icon;

		icon = g_concat_dir_and_file (ICONDIR, "gnome-http-url.png");
		desktop_create_url (
			template,
			selection_data->data,
			selection_data->data,
			icon);
		g_free (icon);
	}
	g_free (template);
}

/* Drop stuff on a directory */
static int
drop_on_directory (GdkDragContext *context, GtkSelectionData *selection_data,
		   GdkDragAction action, char *directory)
{
	int retval;

	retval = FALSE;

	if (gdnd_drag_context_has_target (context, TARGET_URI_LIST)) {
		drop_uri_list_on_directory (context, selection_data, action, directory);
		retval = TRUE;
	} else if (gdnd_drag_context_has_target (context, TARGET_URL)) {
		drop_url_on_directory (context, selection_data, directory);
		retval = TRUE;
	}

	return retval;
}

/* Returns whether a file has the drop-action metadata or MIME property defined
 * for it.
 */
static int
file_has_drop_action (char *filename)
{
	char *buf;
	int size;
	const char *mime_type;

	if (gnome_metadata_get (filename, "drop-action", &size, &buf) == 0) {
		g_free (buf);
		return TRUE;
	} else {
		mime_type = gnome_mime_type_or_default (filename, NULL);
		if (!mime_type)
			return FALSE;

		if (gnome_mime_get_value (mime_type, "drop-action") != NULL)
			return TRUE;
		else
			return FALSE;
	}
}

/* Drop stuff on a non-directory file.  This uses metadata and MIME as well. */
static int
drop_on_file (GdkDragContext *context, GtkSelectionData *selection_data,
	      char *dest_full_name, file_entry *dest_fe)
{
	int size;
	char *buf;
	const char *mime_type;
	int retval;
	GList *names, *l;
	int len, i;
	char **drops;

	retval = FALSE; /* assume we cannot drop */

	/* Convert the data list into an array of strings */

	names = gnome_uri_list_extract_uris (selection_data->data);
	len = g_list_length (names);
	drops = g_new (char *, len + 1);

	for (l = names, i = 0; i < len; i++, l = l->next) {
		char *text = l->data;

		if (strncmp (text, "file:", 5) == 0)
			text += 5;

		drops[i] = text;
	}
	drops[i] = NULL;

	/* 1. Try to use a metadata-based drop action */

	if (gnome_metadata_get (dest_full_name, "drop-action", &size, &buf) == 0) {
		exec_extension (dest_full_name, buf, drops, NULL, 0, 0);
		g_free (buf);
		retval = TRUE;
		goto out;
	}

	/* 2. Try a drop action from the MIME-type */

	mime_type = gnome_mime_type_or_default (dest_full_name, NULL);
	if (mime_type) {
		const char *action;

		action = gnome_mime_get_value (mime_type, "drop-action");
		if (action) {
			exec_extension (dest_full_name, action, drops, NULL, 0, 0);
			retval = TRUE;
			goto out;
		}
	}

	/* 3. If executable, try metadata keys for "open" */

	if (is_exe (dest_fe->buf.st_mode) && if_link_is_exe (dest_full_name, dest_fe)) {
		/* FIXME: handle the case for Netscape URLs */

		if (gnome_metadata_get (dest_full_name, "open", &size, &buf) == 0)
			exec_extension (dest_full_name, buf, drops, NULL, 0, 0);
		else
			exec_extension (dest_full_name, "%f %q", drops, NULL, 0, 0);

		g_free (buf);

		retval = TRUE;
		goto out;
	}

 out:
	g_free (drops);
	gnome_uri_list_free_strings (names);
	return retval;
}

/**
 * gdnd_perform_drop:
 * @context: Drag context for operation.
 * @selection_data: Selection data from drag_data_received.
 * @dest_full_name: Complete name of the destination file or directory.
 * @dest_fe: File entry for the destination file or directory.
 * 
 * Performs a drop operation on a directory or file.
 * 
 * Return value: TRUE if the drop is successful, FALSE otherwise.
 **/
int
gdnd_perform_drop (GdkDragContext *context, GtkSelectionData *selection_data,
		   char *dest_full_name, file_entry *dest_fe)
{
	GdkDragAction action;

	g_return_val_if_fail (context != NULL, FALSE);
	g_return_val_if_fail (selection_data != NULL, FALSE);
	g_return_val_if_fail (dest_full_name != NULL, FALSE);
	g_return_val_if_fail (dest_fe != NULL, FALSE);

	/* Get action */

	if (context->action == GDK_ACTION_ASK) {
		action = get_action (context);
		if (action == GDK_ACTION_ASK)
			return FALSE;
	} else
		action = context->action;

	if (S_ISDIR (dest_fe->buf.st_mode) || dest_fe->f.link_to_dir)
		return drop_on_directory (context, selection_data, action, dest_full_name);
	else
		return drop_on_file (context, selection_data, dest_full_name, dest_fe);
}

/**
 * gdnd_drag_context_has_target:
 * @context: The context to query for a target type
 * @type: The sought target type
 *
 * Tests whether the specified drag context has a target of the specified type.
 *
 * Return value: TRUE if the context has the specified target type, FALSE
 * otherwise.
 **/
int
gdnd_drag_context_has_target (GdkDragContext *context, TargetType type)
{
	GList *l;

	g_return_val_if_fail (context != NULL, FALSE);

	for (l = context->targets; l; l = l->next)
		if (dnd_target_atoms[type] == GPOINTER_TO_INT (l->data))
			return TRUE;

	return FALSE;
}

/**
 * gdnd_find_panel_by_drag_context:
 * @context: The context by which to find a panel.
 * @source_widget: The source widget is returned here.
 *
 * Looks in the list of panels for the one that corresponds to the specified
 * drag context.
 *
 * Return value: The sought panel, or NULL if no panel corresponds to the
 * context.
 **/
WPanel *
gdnd_find_panel_by_drag_context (GdkDragContext *context, GtkWidget **source_widget)
{
	GtkWidget *source;
	GtkWidget *toplevel;
	GList *l;
	WPanel *panel;

	g_return_val_if_fail (context != NULL, NULL);

	source = gtk_drag_get_source_widget (context);

	if (source_widget)
		*source_widget = source;

	if (!source)
		return NULL; /* different process */

	toplevel = gtk_widget_get_toplevel (source);

	for (l = containers; l; l = l->next) {
		panel = ((PanelContainer *) l->data)->panel;

		if (panel->xwindow == toplevel)
			return panel;
	}

	return NULL;
}

/**
 * gdnd_validate_action:
 * @context: The drag context for this drag operation.
 * @on_desktop: Whether we are dragging onto the desktop or a desktop icon.
 * @same_process: Whether the drag comes from the same process or not.
 * @same_source: If same_process, then whether the source and dest widgets are the same.
 * @dest_full_name: Complete name of the destination file or directory.
 * @dest_fe: File entry for the destination file, or NULL if directory.
 * @dest_selected: If dest is non-NULL, whether it is selected or not.
 *
 * Computes the final drag action based on the suggested action of the specified
 * context and conditions.
 *
 * Return value: The computed action, meant to be passed to gdk_drag_action().
 **/
GdkDragAction
gdnd_validate_action (GdkDragContext *context,
		      int on_desktop, int same_process, int same_source,
		      char *dest_full_name, file_entry *dest_fe, int dest_selected)
{
	int on_directory;
	int on_exe;

	g_return_val_if_fail (context != NULL, 0);
	g_return_val_if_fail (dest_full_name != NULL, 0);

	/* If we are dragging a desktop icon onto the desktop or onto a selected
	 * desktop icon, unconditionally specify MOVE.
	 */
	if (on_desktop
	    && gdnd_drag_context_has_target (context, TARGET_MC_DESKTOP_ICON)
	    && (!dest_fe || dest_selected))
		return GDK_ACTION_MOVE;

	/* See what kind of file the destination is, if any */

	if (dest_fe) {
		on_directory = S_ISDIR (dest_fe->buf.st_mode) || dest_fe->f.link_to_dir;
		on_exe = is_exe (dest_fe->buf.st_mode) && if_link_is_exe (dest_full_name, dest_fe);
	}

	if (gdnd_drag_context_has_target (context, TARGET_URI_LIST)) {
		if (dest_fe) {
			if (same_source && dest_selected)
				return 0;

			if (on_directory) {
				if ((same_source || same_process)
				    && (context->actions & GDK_ACTION_MOVE)
				    && context->suggested_action != GDK_ACTION_ASK)
					return GDK_ACTION_MOVE;
				else
					return context->suggested_action;
			} else if (on_exe) {
				if (context->actions & GDK_ACTION_COPY)
					return GDK_ACTION_COPY;
			} else if (file_has_drop_action (dest_full_name)) {
				if (context->actions & GDK_ACTION_COPY)
					return GDK_ACTION_COPY;
			} else if (same_source)
				return 0;
			else if (same_process
				 && (context->actions & GDK_ACTION_MOVE)
				 && context->suggested_action != GDK_ACTION_ASK)
				return GDK_ACTION_MOVE;
			else
				return context->suggested_action;
		} else {
			if (same_source)
				return 0;
			else if (same_process
				 && (context->actions & GDK_ACTION_MOVE)
				 && context->suggested_action != GDK_ACTION_ASK)
				return GDK_ACTION_MOVE;
			else
				return context->suggested_action;
		}
	}

	if (gdnd_drag_context_has_target (context, TARGET_URL)) {
		/* FIXME: right now we only allow linking to directories.  We
		 * should see if we can move or copy stuff instead (for ftp
		 * instead of http sites, for example).
		 */
		if (dest_fe) {
			if (on_directory) {
				if (context->actions & GDK_ACTION_LINK)
					return GDK_ACTION_LINK;
			} else if (context->actions & GDK_ACTION_COPY)
				return GDK_ACTION_COPY;
		} else if (context->actions & GDK_ACTION_LINK)
			return GDK_ACTION_LINK;
	}

	return 0;
}

/**
 * gdnd_can_drop_on_file:
 * @full_name: Complete name of the file.
 * @fe: File entry for the file.
 * 
 * Computes whether a non-directory file can take drops.
 * 
 * Return value: TRUE if the file can take drops, FALSE otherwise.
 **/
int
gdnd_can_drop_on_file (char *full_name, file_entry *fe)
{
	g_return_val_if_fail (full_name != NULL, FALSE);
	g_return_val_if_fail (fe != NULL, FALSE);

	if ((is_exe (fe->buf.st_mode) && if_link_is_exe (full_name, fe))
	    || file_has_drop_action (full_name))
		return TRUE;
	else
		return FALSE;
}
