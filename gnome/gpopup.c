/* Popup menus for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */


#include <config.h>
#include "util.h"
#include <gnome.h>
#include "panel.h"
#include "cmd.h"
#include "dialog.h"
#include "ext.h"
#include "gpageprop.h"
#include "gpopup.h"
#include "main.h"

#define CLIST_FROM_SW(panel_list) GTK_CLIST (GTK_BIN (panel_list)->child)


/* Flags for the popup menu entries.  They specify to which kinds of files an entry is valid for. */
enum {
	F_ALL 		= 1 << 0,	/* Applies to all files */
	F_REGULAR	= 1 << 1,	/* Applies only to regular files */
	F_SYMLINK	= 1 << 2,	/* Applies only to symlinks */
	F_SINGLE	= 1 << 3,	/* Applies only to a single file, not to a multiple selection */
	F_NOTDIR	= 1 << 4,	/* Applies to non-directories */
	F_DICON		= 1 << 5,	/* Applies to desktop icons */
	F_PANEL		= 1 << 6	/* Applies to files from a panel window */
};

struct action {
	char *text;		/* Menu item text */
	int flags;		/* Flags from the above enum */
	GtkSignalFunc callback;	/* Callback for menu item */
};


static void
panel_action_open_with (GtkWidget *widget, WPanel *panel)
{
	char *command;
	
	command = input_expand_dialog (_(" Open with..."),
				       _("Enter extra arguments:"), panel->dir.list [panel->selected].fname);
	if (!command)
		return;
	execute (command);
	free (command);
}

static void
panel_action_open (GtkWidget *widget, WPanel *panel)
{
	if (do_enter (panel))
		return;
	panel_action_open_with (widget, panel);
}

static void
panel_action_view (GtkWidget *widget, WPanel *panel)
{
	view_cmd (panel);
}

static void
panel_action_view_unfiltered (GtkWidget *widget, WPanel *panel)
{
	view_simple_cmd (panel);
}

static void
panel_action_edit (GtkWidget *widget, WPanel *panel)
{
	edit_cmd (panel);
}

static void
desktop_icon_view(GtkWidget *widget, desktop_icon_info *dii)
{
	g_warning ("Not yet implemented\n");
}

/* Pops up the icon properties pages */
void
desktop_icon_properties (GtkWidget *widget, desktop_icon_info *dii)
{
	int retval;
	char *path;
	
	path = g_copy_strings (getenv("HOME"), "/desktop/", dii->filename, NULL);
	retval = item_properties (dii->dicon, path, dii);
	g_free(path);
	if (retval)
		reread_cmd ();
}

void
desktop_icon_execute (GtkWidget *ignored, desktop_icon_info *dii)
{
	desktop_icon_open (dii);
}

static void
panel_action_properties (GtkWidget *widget, WPanel *panel)
{
	file_entry *fe = &panel->dir.list [panel->selected];
	char *full_name = concat_dir_and_file (panel->cwd, fe->fname);

	if (item_properties (GTK_WIDGET (CLIST_FROM_SW (panel->list)), full_name, NULL) != 0)
		reread_cmd ();

	free (full_name);
}

static void
dicon_move (GtkWidget *widget, desktop_icon_info *dii)
{
	g_warning ("Implement this function!");
}

static void
dicon_copy (GtkWidget *widget, desktop_icon_info *dii)
{
	g_warning ("Implement this function!");
}

static void
dicon_delete (GtkWidget *widget, desktop_icon_info *dii)
{
	desktop_icon_delete (dii);
}

/* This is our custom signal connection function for popup menu items -- see below for the
 * marshaller information.  We pass the original callback function as the data pointer for the
 * marshaller (uiinfo->moreinfo).
 */
static void
popup_connect_func (GnomeUIInfo *uiinfo, gchar *signal_name, GnomeUIBuilderData *uibdata)
{
	g_assert (uibdata->is_interp);

	if (uiinfo->moreinfo) {
		gtk_object_set_data (GTK_OBJECT (uiinfo->widget), "popup_user_data", uiinfo->user_data);
		gtk_signal_connect_full (GTK_OBJECT (uiinfo->widget), signal_name,
					 NULL,
					 uibdata->relay_func,
					 uiinfo->moreinfo,
					 uibdata->destroy_func,
					 FALSE,
					 FALSE);
	}
}

/* Our custom marshaller for menu items.  We need it so that it can extract the per-attachment
 * user_data pointer from the parent menu shell and pass it to the callback.  This overrides the
 * user-specified data from the GnomeUIInfo structures.
 */

typedef void (* ActivateFunc) (GtkObject *object, gpointer data);

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

/* Fills the menu with the specified uiinfo at the specified position, using our magic marshallers
 * to be able to fetch the active item.  The code is shamelessly ripped from gnome-popup-menu.
 */
static void
fill_menu (GtkMenuShell *menu_shell, GnomeUIInfo *uiinfo, int pos)
{
	GnomeUIBuilderData uibdata;

	/* We use our own callback marshaller so that it can fetch the popup user data
	 * from the popup menu and pass it on to the user-defined callbacks.
	 */

	uibdata.connect_func = popup_connect_func;
	uibdata.data = NULL;
	uibdata.is_interp = TRUE;
	uibdata.relay_func = popup_marshal_func;
	uibdata.destroy_func = NULL;

	gnome_app_fill_menu_custom (menu_shell, uiinfo, &uibdata, NULL, FALSE, pos);
}


/* The context menu: text displayed, condition that must be met and the routine that gets invoked
 * upon activation.
 */
static struct action file_actions[] = {
	{ N_("Properties"),      F_SINGLE | F_PANEL,   	  	 (GtkSignalFunc) panel_action_properties },
	{ N_("Properties"),      F_SINGLE | F_DICON,  	  	 (GtkSignalFunc) desktop_icon_properties },
	{ "",                    F_SINGLE,   	    	  	 NULL },
	{ N_("Open"),            F_PANEL | F_ALL,      	  	 (GtkSignalFunc) panel_action_open },
	{ N_("Open"),            F_DICON | F_ALL, 	  	 (GtkSignalFunc) desktop_icon_execute },
	{ N_("Open with"),       F_PANEL | F_ALL,      	  	 (GtkSignalFunc) panel_action_open_with },
	{ N_("View"),            F_PANEL | F_NOTDIR,      	 (GtkSignalFunc) panel_action_view },
	{ N_("View unfiltered"), F_PANEL | F_NOTDIR,      	 (GtkSignalFunc) panel_action_view_unfiltered },  
	{ N_("Edit"),            F_PANEL | F_NOTDIR,             (GtkSignalFunc) panel_action_edit },
	{ "",                    0,          	                 NULL },
	{ N_("Link..."),         F_PANEL | F_REGULAR | F_SINGLE, (GtkSignalFunc) link_cmd },
	{ N_("Symlink..."),      F_PANEL | F_SINGLE,             (GtkSignalFunc) symlink_cmd },
	{ N_("Edit symlink..."), F_PANEL | F_SYMLINK,            (GtkSignalFunc) edit_symlink_cmd },
	{ NULL, 0, NULL },
};

/* Menu entries for files from a panel window */
static GnomeUIInfo panel_actions[] = {
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("Move/rename..."), NULL, ren_cmd),
	GNOMEUIINFO_ITEM_NONE (N_("Copy..."), NULL, copy_cmd),
	GNOMEUIINFO_ITEM_NONE (N_("Delete"), NULL, delete_cmd),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_END
};

/* Menu entries for files from desktop icons */
static GnomeUIInfo desktop_icon_actions[] = {
	GNOMEUIINFO_SEPARATOR,
#if 0
	GNOMEUIINFO_ITEM_NONE (N_("Move/rename..."), NULL, dicon_move),
	GNOMEUIINFO_ITEM_NONE (N_("Copy..."), NULL, dicon_copy),
#endif
	GNOMEUIINFO_ITEM_NONE (N_("Delete"), NULL, dicon_delete),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_END
};


/* Creates the menu items for the standard actions.  Returns the position at which additional menu
 * items should be inserted.
 */
static int
create_actions (GtkWidget *menu, WPanel *panel,
		desktop_icon_info *dii,
		int panel_row, char *filename)
{
	gpointer closure;
	struct action *action;
	int pos;
	GnomeUIInfo uiinfo[] = {
		{ 0 },
		GNOMEUIINFO_END
	};

	closure = panel ? (gpointer) panel : (gpointer) dii;

	pos = 0;
	
	for (action = file_actions; action->text; action++) {
		/* First, try F_PANEL and F_DICON flags */

		if (!panel && (action->flags & F_PANEL))
			continue;

		if (!dii && (action->flags & F_DICON))
			continue;

		/* Items with F_ALL bypass any other condition */

		if (!(action->flags & F_ALL)) {
			/* Items with F_SINGLE require that ONLY ONE marked files exist */

			if (panel && (action->flags & F_SINGLE))
				if (panel->marked > 1)
					continue;

			/* Items with F_NOTDIR requiere that the selection is not a directory */

			if (panel && (action->flags & F_NOTDIR)) {
				struct stat *s = &panel->dir.list[panel_row].buf;

				if (panel->dir.list [panel_row].f.link_to_dir)
					continue;

				if (S_ISDIR (s->st_mode))
					continue;
			}

			/* Items with F_REGULAR do not accept any strange file types */

			if (panel && (action->flags & F_REGULAR)) {
				struct stat *s = &panel->dir.list[panel_row].buf;

				if (S_ISLNK (s->st_mode) && panel->dir.list[panel_row].f.link_to_dir)
					continue;

				if (S_ISSOCK (s->st_mode) || S_ISCHR (s->st_mode) ||
				    S_ISFIFO (s->st_mode) || S_ISBLK (s->st_mode))
					continue;
			}

			/* Items with F_SYMLINK only operate on symbolic links */

			if (panel && action->flags & F_SYMLINK) {
				struct stat *s = &panel->dir.list[panel_row].buf;

				if (!S_ISLNK (s->st_mode))
					continue;
			}
		}

		/* Create the appropriate GnomeUIInfo structure for the menu item */

		if (action->text[0]) {
			uiinfo[0].type = GNOME_APP_UI_ITEM;
			uiinfo[0].label = _(action->text);
			uiinfo[0].hint = NULL;
			uiinfo[0].moreinfo = action->callback;
			uiinfo[0].user_data = closure;
			uiinfo[0].unused_data = NULL;
			uiinfo[0].pixmap_type = GNOME_APP_PIXMAP_NONE;
			uiinfo[0].pixmap_info = NULL;
			uiinfo[0].accelerator_key = 0;
			uiinfo[0].ac_mods = 0;
			uiinfo[0].widget = NULL;
		} else
			uiinfo[0].type = GNOME_APP_UI_SEPARATOR;

		fill_menu (GTK_MENU_SHELL (menu), uiinfo, pos);
		pos++;
	}

	return pos;
}

/* These functions activate a menu popup action for a filename in a hackish way, as they have to
 * fetch the action from the menu item's label.
 */

static char *
get_label_text (GtkMenuItem *item)
{
	GtkWidget *label;

	/* THIS DEPENDS ON THE LATEST GNOME-LIBS!  Keep this in sync with gnome-app-helper until we
	 * figure a way to do this in a non-hackish way.
	 */

	label = GTK_BIN (item)->child;
	g_assert (label != NULL);

	return GTK_LABEL (label)->label;
}

static void
mime_command_from_panel (GtkMenuItem *item, WPanel *panel)
{
	char *filename;
	char *action;
	int movedir;
	
	/*
	 * This is broken, but we dont mind.  Federico
	 * needs to explain me what was he intending here.
	 * panel->selected does not mean, it was the icon
	 * that got clicked.
	 */
}

static void
mime_command_from_desktop_icon (GtkMenuItem *item, char *filename)
{
	char *action;
	int movedir;
	char *key, *mime_type, *val;
	action = get_label_text (item);

	key = gtk_object_get_user_data (GTK_OBJECT (item));
	mime_type = gnome_mime_type_or_default (filename, NULL);
	if (!mime_type)
		return;
	
	val = gnome_mime_get_value (mime_type, key);
	exec_extension (filename, val, NULL, NULL, 0);
}

/* Create the menu items common to files from panel window and desktop icons, and also the items for
 * regexp_command() actions.
 */
static void
create_regexp_actions (GtkWidget *menu, WPanel *panel,
		       desktop_icon_info *dii,
		       int panel_row, char *filename, int insert_pos)
{
	gpointer closure, regex_closure;
	GnomeUIInfo *a_uiinfo;
	int i;
	GtkSignalFunc regex_callback;
	char *mime_type;
	GList *keys, *l;
	GnomeUIInfo uiinfo[] = {
		{ 0 },
		GNOMEUIINFO_END
	};

	if (panel) {
		a_uiinfo = panel_actions;
		closure = panel;
		regex_callback = mime_command_from_desktop_icon;
		regex_closure = filename;
	} else {
		a_uiinfo = desktop_icon_actions;
		closure = dii;
		regex_callback = mime_command_from_desktop_icon;
		regex_closure = filename;
	}

	/* Fill in the common part of the menus */

	for (i = 0; i < 5; i++)
		a_uiinfo[i].user_data = closure;

	fill_menu (GTK_MENU_SHELL (menu), a_uiinfo, insert_pos);
	insert_pos += 5; /* the number of items from the common menus */

	/* Fill in the regex command part */

	mime_type = gnome_mime_type_or_default (filename, NULL);
	if (!mime_type)
		return;
	
	keys = gnome_mime_get_keys (mime_type);

	for (l = keys; l; l = l->next) {
		char *key = l->data;
		char *str;
		
		str = key;
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

		uiinfo[0].type = GNOME_APP_UI_ITEM;
		uiinfo[0].label = str;
		uiinfo[0].hint = NULL;
		uiinfo[0].moreinfo = regex_callback;
		uiinfo[0].user_data = regex_closure;
		uiinfo[0].unused_data = NULL;
		uiinfo[0].pixmap_type = GNOME_APP_PIXMAP_NONE;
		uiinfo[0].pixmap_info = NULL;
		uiinfo[0].accelerator_key = 0;
		uiinfo[0].ac_mods = 0;
		uiinfo[0].widget = NULL;

		fill_menu (GTK_MENU_SHELL (menu), uiinfo, insert_pos++);
		gtk_object_set_user_data (GTK_OBJECT (uiinfo [0].widget), key);
	}
	g_list_free (keys);
}

/* Convenience callback to exit the main loop of a modal popup menu when it is deactivated*/
static void
menu_shell_deactivated (GtkMenuShell *menu_shell, gpointer data)
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

/*
 * Create a context menu
 * It can take either a WPanel or a GnomeDesktopEntry.  One of them should
 * be set to NULL.
 */
int
gpopup_do_popup (GdkEventButton *event,
		 WPanel *from_panel,
		 struct desktop_icon_info *dii,
		 int panel_row,
		 char *filename)
{
	GtkWidget *menu;
	int pos;
	guint id;

	g_return_val_if_fail (event != NULL, -1);
	g_return_val_if_fail (from_panel != NULL || dii != NULL || filename != NULL, -1);

	menu = gtk_menu_new ();

	/* Connect to the deactivation signal to be able to quit our
           modal main loop */

	id = gtk_signal_connect (GTK_OBJECT (menu), "deactivate",
				 (GtkSignalFunc) menu_shell_deactivated,
				 NULL);

	/* Fill the menu */

	pos = create_actions (menu, from_panel, dii,
			      panel_row, filename);
	create_regexp_actions (menu, from_panel, dii,
			       panel_row, filename, pos);

	/* Run it */

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);
	gtk_grab_add (menu);
	gtk_main ();
	gtk_grab_remove (menu);

	gtk_signal_disconnect (GTK_OBJECT (menu), id);

	return get_active_index (GTK_MENU (menu));
}




