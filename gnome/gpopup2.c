/* Popup menus for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 * 	    Jonathan Blandford <jrb@redhat.com>
 */

/* These rules really need to be duplicated elsewhere.
 * They should exist for the menu bar too...
 */

#include <config.h>
#include "global.h"
#include <gnome.h>
#include "panel.h"
#include "cmd.h"
#include "dialog.h"
#include "ext.h"
#include "main.h"
#include "../vfs/vfs.h"
#include "gpageprop.h"
#include "gpopup.h"
#include "gnome-file-property-dialog.h"
#include "gnome-open-dialog.h"
#include "gmain.h"
#include "gmount.h"
#define CLIST_FROM_SW(panel_list) GTK_CLIST (GTK_BIN (panel_list)->child)


/* Flags for the popup menu entries.  They specify to which kinds of files an
 * entry is valid for.
 */
enum {
	F_ALL 		= 1 << 0,	/* Applies to all files */
	F_REGULAR	= 1 << 1,	/* Applies only to regular files */
	F_SYMLINK	= 1 << 2,	/* Applies only to symlinks */
	F_SINGLE	= 1 << 3,	/* Applies only to a single file, not to multiple files */
	F_NOTDIR	= 1 << 4,	/* Applies to non-directories */
	F_NOTDEV	= 1 << 5,	/* Applies to non-devices only (ie. reg, lnk, dir) */
	F_ADVANCED	= 1 << 6,	/* Only appears in advanced mode */
	F_MIME_ACTIONS	= 1 << 7	/* Special marker for the position of MIME actions */
};

/* An entry in the actions menu */

typedef gboolean (*menu_func) (WPanel *panel, DesktopIconInfo *dii);

struct action {
	char *text;		/* Menu item text */
	int flags;		/* Flags from the above enum */
	gpointer callback;	/* Callback for menu item */
	menu_func func;         /* NULL if item is always present; a predicate otherwise */
};


/* Multiple File commands */
static void handle_open (GtkWidget *widget, WPanel *panel);
static void handle_mount (GtkWidget *widget, WPanel *panel);
static void handle_unmount (GtkWidget *widget, WPanel *panel);
static void handle_eject (GtkWidget *widget, WPanel *panel);
static void handle_view (GtkWidget *widget, WPanel *panel);
static void handle_view_unfiltered (GtkWidget *widget, WPanel *panel);
static void handle_edit (GtkWidget *widget, WPanel *panel);
static void handle_copy (GtkWidget *widget, WPanel *panel);
static void handle_delete (GtkWidget *widget, WPanel *panel);
static void handle_move (GtkWidget *widget, WPanel *panel);

/* F_SINGLE file commands */
static void handle_properties (GtkWidget *widget, WPanel *panel);
static void handle_open_with (GtkWidget *widget, WPanel *panel);
static void handle_symlink (GtkWidget *widget, WPanel *panel);
static void handle_hard_link (GtkWidget *widget, WPanel *panel);
static void handle_edit_symlink (GtkWidget *widget, WPanel *panel);

/* Helper funcs and testing funcs. */
static gboolean check_mount_func (WPanel *panel, DesktopIconInfo *dii);
static gboolean check_unmount_func (WPanel *panel, DesktopIconInfo *dii);
static gboolean check_eject_func (WPanel *panel, DesktopIconInfo *dii);
static gboolean check_device_func (WPanel *panel, DesktopIconInfo *dii);

static gchar * get_full_filename (WPanel *panel);

/* Now, the actual code */
static gchar *
get_full_filename (WPanel *panel)
{
	if (is_a_desktop_panel (panel)) {
		gint i;
		for (i = 0; i < panel->count; i++) 
			if (panel->dir.list [i].f.marked) {
				return concat_dir_and_file (panel->cwd,
							    panel->dir.list [i].fname);
			}
		g_return_val_if_fail (FALSE, NULL);
	} else
		return concat_dir_and_file (panel->cwd, selection (panel)->fname);

}

static gboolean
check_mount_umount (DesktopIconInfo *dii, int mount)
{
	char *full_name;
	file_entry *fe;
	int v;
	int is_mounted;

	full_name = g_concat_dir_and_file (desktop_directory, dii->filename);
	fe = file_entry_from_file (full_name);
	if (!fe) {
		g_free (full_name);
		return FALSE;
	}

	v = is_mountable (full_name, fe, &is_mounted, NULL);
	file_entry_free (fe);
	g_free (full_name);

	if (!v)
		return FALSE;

	if (is_mounted && mount)
		return FALSE;

	if (!is_mounted && !mount)
		return FALSE;

	return TRUE;
}

static gboolean
check_mount_func (WPanel *panel, DesktopIconInfo *dii)
{
	if (!dii)
		return FALSE;

	return check_mount_umount (dii, TRUE);
}

static gboolean
check_unmount_func (WPanel *panel, DesktopIconInfo *dii)
{
	if (!dii)
		return FALSE;

	return check_mount_umount (dii, FALSE);
}

static gboolean
check_eject_func (WPanel *panel, DesktopIconInfo *dii)
{
	char *full_name;
	file_entry *fe;
	int v;
	int is_mounted;
	int retval;

	if (!dii)
		return FALSE;

	full_name = g_concat_dir_and_file (desktop_directory, dii->filename);
	fe = file_entry_from_file (full_name);
	if (!fe) {
		g_free (full_name);
		return FALSE;
	}

	v = is_mountable (full_name, fe, &is_mounted, NULL);
	file_entry_free (fe);

	if (!v)
		retval = FALSE;
	else if (!is_ejectable (full_name))
		retval = FALSE;
	else
		retval = TRUE;

	g_free (full_name);
	return retval;
}
static gboolean
check_device_func (WPanel *panel, DesktopIconInfo *dii)
{
	return (check_mount_func (panel, dii) ||
		check_unmount_func (panel, dii) ||
		check_eject_func (panel, dii));
}

/* global vars */
extern int we_can_afford_the_speed;

static struct action file_actions[] = {
	{ N_("Open"),			F_NOTDEV | F_SINGLE,			handle_open,		NULL },
	{ "",				F_NOTDEV | F_SINGLE,			NULL, 			NULL },
	{ N_("Mount device"),		F_ALL | F_SINGLE,			handle_mount,		check_mount_func },
	{ N_("Unmount device"),		F_ALL | F_SINGLE,			handle_unmount,		check_unmount_func },
	{ N_("Eject device"),		F_ALL | F_SINGLE,			handle_eject,		check_eject_func },
	/* Custom actions go here */
	{ "",				F_MIME_ACTIONS | F_SINGLE,		NULL, 			check_device_func },
	{ N_("Open with..."),		F_REGULAR | F_SINGLE, 			handle_open_with, 	NULL },
	{ N_("View"),			F_REGULAR | F_SINGLE, 			handle_view, 		NULL },
	{ N_("View Unfiltered"),	F_REGULAR | F_ADVANCED | F_SINGLE,	handle_view_unfiltered, NULL },
	{ N_("Edit"),			F_REGULAR | F_SINGLE, 			handle_edit,  		NULL },
	{ "",				F_REGULAR | F_SINGLE, 			NULL, 			NULL },
	{ N_("Copy..."),		F_ALL, 					handle_copy, 		NULL },
	{ N_("Delete"),			F_ALL, 					handle_delete, 		NULL },
	{ N_("Move..."),		F_ALL, 					handle_move, 		NULL },
	{ N_("Hard Link..."),		F_ADVANCED | F_SINGLE, 			handle_hard_link,  	NULL },
	{ N_("Symlink..."),		F_SINGLE, 				handle_symlink, 	NULL },
	{ N_("Edit Symlink..."),	F_SYMLINK | F_SINGLE, 			handle_edit_symlink,  	NULL },
	{ "",				F_SINGLE | F_ALL, 			NULL, 			NULL },
	{ N_("Properties..."),		F_SINGLE | F_ALL, 			handle_properties, 	NULL },
	{ NULL, 0, NULL, NULL }
};

/* This is our custom signal connection function for popup menu items -- see below for the
 * marshaller information.  We pass the original callback function as the data pointer for the
 * marshaller (uiinfo->moreinfo).
 */
static void
popup_connect_func (GnomeUIInfo *uiinfo, gchar *signal_name, GnomeUIBuilderData *uibdata)
{
	g_assert (uibdata->is_interp);

	if (uiinfo->moreinfo) {
		gtk_object_set_data (GTK_OBJECT (uiinfo->widget), "popup_user_data",
				     uiinfo->user_data);
		gtk_signal_connect_full (GTK_OBJECT (uiinfo->widget), signal_name,
					 NULL,
					 uibdata->relay_func,
					 uiinfo->moreinfo,
					 uibdata->destroy_func,
					 FALSE,
					 FALSE);
	}
}

/* Our custom marshaller for menu items.  We need it so that it can extract the
 * per-attachment user_data pointer from the parent menu shell and pass it to
 * the callback.  This overrides the user-specified data from the GnomeUIInfo
 * structures.
 */

typedef void (* ActivateFunc) (GtkObject *object, WPanel *panel);

static void
popup_marshal_func (GtkObject *object, gpointer data, guint n_args, GtkArg *args)
{
	ActivateFunc func;
	gpointer user_data;

	func = (ActivateFunc) data;
	user_data = gtk_object_get_data (object, "popup_user_data");

	gtk_object_set_data (GTK_OBJECT (GTK_WIDGET (object)->parent), "popup_active_item", object);
	(* func) (object, user_data);
}

/* Fills the menu with the specified uiinfo at the specified position, using our
 * magic marshallers to be able to fetch the active item.  The code is
 * shamelessly ripped from gnome-popup-menu.
 */
static void
fill_menu (GtkMenuShell *menu_shell, GnomeUIInfo *uiinfo, int pos)
{
	GnomeUIBuilderData uibdata;

	/* We use our own callback marshaller so that it can fetch the popup
	 * user data from the popup menu and pass it on to the user-defined
	 * callbacks.
	 */

	uibdata.connect_func = popup_connect_func;
	uibdata.data = NULL;
	uibdata.is_interp = TRUE;
	uibdata.relay_func = popup_marshal_func;
	uibdata.destroy_func = NULL;

	gnome_app_fill_menu_custom (menu_shell, uiinfo, &uibdata, NULL, FALSE, pos);
}

/* Convenience function to free something when an object is destroyed */
static void
free_on_destroy (GtkObject *object, gpointer data)
{
	g_free (data);
}

/* Callback for MIME-based actions */
static void
mime_action_callback (GtkWidget *widget, gpointer data)
{
	char *filename;
	char *key, *buf;
	const char *mime_type;
	const char *value;
	int needs_terminal = 0;
	int size;

	filename = data;
	key = gtk_object_get_user_data (GTK_OBJECT (widget));

	g_assert (filename != NULL);
	g_assert (key != NULL);

	mime_type = gnome_mime_type_or_default (filename, NULL);
	g_assert (mime_type != NULL);

	/*
	 * Find out if we need to run this in a terminal
	 */
	if (gnome_metadata_get (filename, "flags", &size, &buf) == 0){
		needs_terminal = strstr (buf, "needsterminal") != 0;
		g_free (buf);
	} else {
		char *flag_key;
		const char *flags;
		
		flag_key = g_strconcat ("flags.", key, "flags", NULL);
		flags = gnome_mime_get_value (filename, flag_key);
		g_free (flag_key);
		if (flags)
			needs_terminal = strstr (flags, "needsterminal") != 0;
	}
	
	value = gnome_mime_get_value (mime_type, key);
	exec_extension (filename, value, NULL, NULL, 0, needs_terminal);
}

/* Escapes the underlines in the specified string for use by GtkLabel */
static char *
escape_underlines (char *str)
{
	char *buf;
	char *p;

	buf = g_new (char, 2 * strlen (str) + 1);
	
	for (p = buf; *str; str++) {
		if (*str == '_')
			*p++ = '_';

		*p++ = *str;
	}

	*p = '\0';

	return buf;
}

/* Creates the menu items for actions based on the MIME type of the selected
 * file in the panel.
 */
static int
create_mime_actions (GtkWidget *menu, WPanel *panel, int pos, DesktopIconInfo *dii)
{
	char *full_name;
	const char *mime_type;
	GList *keys, *l;
	gint pos_init = pos;
	GnomeUIInfo uiinfo[] = {
		{ 0 },
		GNOMEUIINFO_END
	};

	if (is_a_desktop_panel (panel)) {
		g_assert (dii != NULL);
		full_name = g_concat_dir_and_file (panel->cwd, dii->filename);
	} else
		full_name = g_concat_dir_and_file (panel->cwd,
						   panel->dir.list[panel->selected].fname);
	mime_type = gnome_mime_type_or_default (full_name, NULL);

	if (!mime_type) {
		g_free (full_name);
		return pos;
	}

	keys = gnome_mime_get_keys (mime_type);
	for (l = keys; l; l = l->next) {
		char *key;
		char *str;

		str = key = l->data;

		if (strncmp (key, "open.", 5) != 0)
			continue;

		str += 5;
		while (*str && *str != '.')
			str++;

		if (*str)
			str++;

		if (!*str)
			continue;

		/* Create the item for that entry */

		str = escape_underlines (str);

		uiinfo[0].type = GNOME_APP_UI_ITEM;
		uiinfo[0].label = str;
		uiinfo[0].hint = NULL;
		uiinfo[0].moreinfo = mime_action_callback;
		uiinfo[0].user_data = full_name;
		uiinfo[0].unused_data = NULL;
		uiinfo[0].pixmap_type = GNOME_APP_PIXMAP_NONE;
		uiinfo[0].pixmap_info = NULL;
		uiinfo[0].accelerator_key = 0;
		uiinfo[0].ac_mods = 0;
		uiinfo[0].widget = NULL;

		fill_menu (GTK_MENU_SHELL (menu), uiinfo, pos++);
		g_free (str);

		gtk_object_set_user_data (GTK_OBJECT (uiinfo[0].widget), key);

	}

	/* Remember to free this memory */
	gtk_signal_connect (GTK_OBJECT (menu), "destroy",
			    (GtkSignalFunc) free_on_destroy,
			    full_name);
	
	if (pos_init != pos) {
		uiinfo[0].type = GNOME_APP_UI_SEPARATOR;
		fill_menu (GTK_MENU_SHELL (menu), uiinfo, pos++);
		g_list_free (keys);
	}

	return pos;
}

/* Creates the menu items for the standard actions.  Returns the position at
 * which additional menu items should be inserted.
 */
static void
create_actions (GtkWidget *menu, gint flags, WPanel *panel, DesktopIconInfo *dii)
{
	struct action *action;
	int pos;
	GnomeUIInfo uiinfo[] = {
		{ 0 },
		GNOMEUIINFO_END
	};

	pos = 0;

	for (action = file_actions; action->text; action++) {
		/* Insert the MIME actions if appropriate */
		if ((action->flags & F_MIME_ACTIONS) && (flags & F_SINGLE)) {
			int curpos = pos;
			pos = create_mime_actions (menu, panel, pos, dii);
			/* Why do we do this?  If the mime actions are there, and it's mountable,
			 * we can count on the separator at the end of the mime_actions
			 * menu.  However, if the mime_actions aren't there, we need a separator */
			if (pos == curpos && (action->func)(panel, dii)) {
				uiinfo[0].type = GNOME_APP_UI_SEPARATOR;
				fill_menu (GTK_MENU_SHELL (menu), uiinfo, pos++);
			}
			continue;
		}

		/* Filter the actions that are not appropriate */
		if ((action->flags & flags) != action->flags)
			continue;

		if (action->func && !((action->func)(panel, dii)))
			continue;

		/* Create the menu item for this action */
		if (action->text[0]) {
			uiinfo[0].type = GNOME_APP_UI_ITEM;
			uiinfo[0].label = _(action->text);
			uiinfo[0].hint = NULL;
			uiinfo[0].moreinfo = action->callback;
			uiinfo[0].user_data = (gpointer) panel;
			uiinfo[0].unused_data = NULL;
			uiinfo[0].pixmap_type = GNOME_APP_PIXMAP_NONE;
			uiinfo[0].pixmap_info = NULL;
			uiinfo[0].accelerator_key = 0;
			uiinfo[0].ac_mods = 0;
			uiinfo[0].widget = NULL;
		} else
			uiinfo[0].type = GNOME_APP_UI_SEPARATOR;

		fill_menu (GTK_MENU_SHELL (menu), uiinfo, pos++);
	}
}

/* Convenience callback to exit the main loop of a modal popup menu when it is deactivated*/
static void
menu_shell_deactivated (GtkMenuShell *menu_shell, WPanel *panel)
{
	gtk_main_quit ();
}

/* Returns the index of the active item in the specified menu, or -1 if none is active */
static int
get_active_index (GtkMenu *menu)
{
	GList *l;
	GtkWidget *active;
	int i;

	active = gtk_object_get_data (GTK_OBJECT (menu), "popup_active_item");

	for (i = 0, l = GTK_MENU_SHELL (menu)->children; l; l = l->next, i++)
		if (active == l->data)
			return i;

	return -1;
}

#define REMOVE(x,f) x &= ~f

int
gpopup_do_popup2 (GdkEventButton *event, WPanel *panel, DesktopIconInfo *dii)
{
	GtkWidget *menu;
	gint flags = F_ALL | F_REGULAR | F_SYMLINK | F_SINGLE | F_NOTDEV | F_NOTDIR;
	struct stat s;
	guint id;
	int i;
	int marked;

	g_return_val_if_fail (event != NULL, -1);
	g_return_val_if_fail (panel != NULL, -1);

	menu = gtk_menu_new ();

	/* Connect to the deactivation signal to be able to quit our modal main
	 * loop.
	 */
	id = gtk_signal_connect (GTK_OBJECT (menu), "deactivate",
				 (GtkSignalFunc) menu_shell_deactivated,
				 NULL);

	marked = 0;

	for (i = 0; i < panel->count; i++) {
		if (!strcmp (panel->dir.list [i].fname, "..") || !panel->dir.list[i].f.marked)
			continue;

		marked++;

		s = panel->dir.list[i].buf;

		if (S_ISLNK (s.st_mode))
			mc_stat (panel->dir.list [i].fname, &s);
		else
			REMOVE (flags, F_SYMLINK);

		if (S_ISDIR (s.st_mode))
			REMOVE (flags, F_NOTDIR);

		if (!S_ISREG (s.st_mode))
			REMOVE (flags, F_REGULAR);

		if (S_ISCHR (s.st_mode) || S_ISBLK (s.st_mode))
			REMOVE (flags, F_NOTDEV);
	}

	if (marked == 0)
		return 1;

	if (marked > 1)
		REMOVE (flags, F_SINGLE);

	/* Fill the menu */
	create_actions (menu, flags, panel, dii);

	/* Run it */
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);
	gtk_grab_add (menu);
	gtk_main ();
	gtk_grab_remove (menu);

	gtk_signal_disconnect (GTK_OBJECT (menu), id);

	i = get_active_index (GTK_MENU (menu));
	gtk_widget_unref (menu);
	return 1;
}

static void
handle_open (GtkWidget *widget, WPanel *panel)
{


	if (is_a_desktop_panel (panel)) {
		gchar *full_name;
		DesktopIconInfo *dii;
		
		full_name = get_full_filename (panel);
		dii = desktop_icon_info_get_by_filename (x_basename (full_name));
		g_assert (dii != NULL);

		desktop_icon_info_open (dii);
		g_free (full_name);
		return;
	}
	do_enter (panel);
}

static void
perform_mount_unmount (WPanel *panel, int mount)
{
	char *full_name;
	DesktopIconInfo *dii;

	g_assert (is_a_desktop_panel (panel));

	full_name = get_full_filename (panel);
	dii = desktop_icon_info_get_by_filename (x_basename (full_name));
	g_assert (dii != NULL);

	desktop_icon_set_busy (dii, TRUE);
	do_mount_umount (full_name, mount);
	desktop_icon_set_busy (dii, FALSE);
	g_free (full_name);
}

static void
handle_mount (GtkWidget *widget, WPanel *panel)
{
	perform_mount_unmount (panel, TRUE);
	update_panels (UP_RELOAD, UP_KEEPSEL);
}

static void
handle_unmount (GtkWidget *widget, WPanel *panel)
{
	perform_mount_unmount (panel, FALSE);
	update_panels (UP_RELOAD, UP_KEEPSEL);
}

static void
handle_eject (GtkWidget *widget, WPanel *panel)
{
	char *full_name;
	char *lname;
	DesktopIconInfo *dii;
	
	g_assert (is_a_desktop_panel (panel));

	full_name = get_full_filename (panel);
	dii = desktop_icon_info_get_by_filename (x_basename (full_name));
	g_assert (dii != NULL);

	desktop_icon_set_busy (dii, TRUE);

	lname = g_readlink (full_name);
	if (!lname){
		g_free (full_name);
		desktop_icon_set_busy (dii, FALSE);
		return;
	}

	if (is_block_device_mounted (lname))
		do_mount_umount (full_name, FALSE);
	
	do_eject (lname);
	
	desktop_icon_set_busy (dii, FALSE);
	update_panels (UP_RELOAD, UP_KEEPSEL);
	g_free (lname);
	g_free (full_name);
}

static void
handle_view (GtkWidget *widget, WPanel *panel)
{
	gchar *full_name;

	full_name = get_full_filename (panel);
	gmc_view (full_name, 0);
	g_free (full_name);
}

static void
handle_view_unfiltered (GtkWidget *widget, WPanel *panel)
{
	/* We need it to do the right thing later. */
	/*view_simple_cmd (panel);*/
	return;
}

static void
handle_edit (GtkWidget *widget, WPanel *panel)
{
	gchar *full_name;

	full_name = get_full_filename (panel);
	gmc_edit (full_name);
	g_free (full_name);
}

static void
handle_copy (GtkWidget *widget, WPanel *panel)
{
	copy_cmd ();
}

static void
handle_delete (GtkWidget *widget, WPanel *panel)
{
	delete_cmd ();
}

static void
handle_move (GtkWidget *widget, WPanel *panel)
{
	ren_cmd ();
}

/* F_SINGLE file commands */
static void
handle_properties (GtkWidget *widget, WPanel *panel)
{
	gint retval = 0;
	GtkWidget *dialog;
	gchar *full_name = NULL;
	int run;
	
	full_name = get_full_filename (panel);
	dialog = gnome_file_property_dialog_new (full_name,
						 (is_a_desktop_panel (panel)
						  ? TRUE
						  : we_can_afford_the_speed));

	if (!is_a_desktop_panel (panel))
		gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (panel->xwindow));

	run = gnome_dialog_run (GNOME_DIALOG (dialog));
	if (run == 0)
		retval = gnome_file_property_dialog_make_changes (
			GNOME_FILE_PROPERTY_DIALOG (dialog));

	if (run != -1)
		gtk_widget_destroy (dialog);

	g_free (full_name);
	if (retval && !is_a_desktop_panel (panel))
		reread_cmd ();
}

static void
handle_open_with (GtkWidget *widget, WPanel *panel)
{
	gchar *full_name;
	full_name = get_full_filename (panel);
	gmc_open_with (full_name);
	g_free (full_name);
}

static void
handle_hard_link (GtkWidget *widget, WPanel *panel)
{
	/* yeah right d:  -jrb */
	link_cmd ();
}

static void
handle_symlink (GtkWidget *widget, WPanel *panel)
{
	symlink_cmd ();
}

static void
handle_edit_symlink (GtkWidget *widget, WPanel *panel)
{
	edit_symlink_cmd ();
}

