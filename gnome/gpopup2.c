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
	F_DICON		= 1 << 5,	/* Applies only to desktop icons */
	F_NOTDEV	= 1 << 6,	/* Applies to non-devices only (ie. reg, lnk, dir) */
	F_ADVANCED	= 1 << 7        /* Only appears in advanced mode */
};

/* typedefs */
struct action {
	char *text;		/* Menu item text */
	int flags;		/* Flags from the above enum */
	gpointer callback;	/* Callback for menu item */
};

/* Multiple File commands */
static void panel_action_open_with (GtkWidget *widget, WPanel *panel);
static void handle_open (GtkWidget *widget, WPanel *panel);
static void handle_view (GtkWidget *widget, WPanel *panel);
static void handle_view_unfiltered (GtkWidget *widget, WPanel *panel);
static void handle_edit (GtkWidget *widget, WPanel *panel);
static void handle_copy (GtkWidget *widget, WPanel *panel);
static void handle_delete (GtkWidget *widget, WPanel *panel);
static void handle_move (GtkWidget *widget, WPanel *panel);

/* F_SINGLE file commands */
static void handle_properties (GtkWidget *widget, WPanel *panel);
static void handle_open_with (GtkWidget *widget, WPanel *panel);
static void handle_hard_link (GtkWidget *widget, WPanel *panel);
static void handle_symlink (GtkWidget *widget, WPanel *panel);
static void handle_edit_symlink (GtkWidget *widget, WPanel *panel);

/* Generic Options */
static void handle_display_properties (GtkWidget *widget, WPanel *panel);
static void handle_rescan (GtkWidget *widget, WPanel *panel);
static void handle_arrange_icons (GtkWidget *widget, WPanel *panel);
static void handle_logout (GtkWidget *widget, WPanel *panel);

/* global vars */
extern int we_can_afford_the_speed;

static struct action file_actions[] = {
	{ N_("Open"),			F_NOTDEV,				handle_open },
	{ "",				F_NOTDEV,				NULL },
	{ N_("Open with..."),		F_REGULAR | F_SINGLE, 			handle_open_with },
	{ N_("View"),			F_REGULAR | F_SINGLE, 			handle_view },
	{ N_("View Unfiltered"),	F_REGULAR | F_ADVANCED | F_SINGLE,	handle_view_unfiltered },  
	{ N_("Edit"),			F_REGULAR | F_SINGLE, 			handle_edit },
	{ "",				F_REGULAR | F_SINGLE, 			NULL },
	{ N_("Copy..."),		F_ALL, 					handle_copy },
	{ N_("Delete"),			F_ALL, 					handle_delete },
	{ N_("Move..."),		F_ALL, 					handle_move },
	{ N_("Hard Link..."),		F_ADVANCED | F_SINGLE, 			handle_hard_link },
	{ N_("Symlink..."),		F_SINGLE, 				handle_symlink },
	{ N_("Edit Symlink..."),	F_SYMLINK | F_SINGLE, 			handle_edit_symlink },
	{ "",				F_SINGLE | F_ALL, 			NULL },
	{ N_("Properties..."),		F_SINGLE | F_ALL, 			handle_properties },
	{ NULL, 0, NULL }
};

#if 0
static action generic_actions[] = {
	{ N_("NEW(FIXME)"),		F_ALL, 		NULL },
	{ "",				F_ALL, 		NULL },
	{ N_("Change Background"),	F_DICON, 	handle_display_properties },
	{ N_("Rescan Directory"),	F_ALL,		handle_rescan },
	{ N_("Arrange Icons"),		F_DICON, 	handle_arrange_icons },
	{ N_(""),			F_DICON, 	NULL },
	{ N_("Logout"),			F_DICON, 	handle_logout },
	{ NULL, 0, NULL }
};
#endif

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


/* The context menu: text displayed, condition that must be met and the routine
 * that gets invoked upon activation.
 */


/* Creates the menu items for the standard actions.  Returns the position at
 * which additional menu items should be inserted.
 */
static void
create_actions (GtkWidget *menu, gint flags, WPanel *panel)
{
	struct action *action;
	int pos;
	GnomeUIInfo uiinfo[] = {
		{ 0 },
		GNOMEUIINFO_END
	};

	pos = 0;

	for (action = file_actions; action->text; action++) {
		if ((action->flags & flags) != action->flags)
			continue;

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
gpopup_do_popup2 (GdkEventButton *event, WPanel *panel)
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

	g_assert (marked > 0);

	if (marked > 1)
		REMOVE (flags, F_SINGLE);

	/* Fill the menu */
	create_actions (menu, flags, panel);

	/* Run it */
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);
	gtk_grab_add (menu);
	gtk_main ();
	gtk_grab_remove (menu);

	gtk_signal_disconnect (GTK_OBJECT (menu), id);

	i = get_active_index (GTK_MENU (menu));
	gtk_widget_destroy (menu);
	return i;
}

static void
panel_action_open_with (GtkWidget *widget, WPanel *panel)
{
	char *command;
	
	command = input_expand_dialog (_(" Open with..."),
				       _("Enter extra arguments:"),
				       panel->dir.list [panel->selected].fname);
	if (!command)
		return;

	execute (command);
	g_free (command);
}

static void
handle_open (GtkWidget *widget, WPanel *panel)
{
	if (do_enter (panel))
		return;

	panel_action_open_with (widget, panel);
}

static void
handle_view (GtkWidget *widget, WPanel *panel)
{
	view_cmd (panel);
}

static void
handle_view_unfiltered (GtkWidget *widget, WPanel *panel)
{
	view_simple_cmd (panel);
}

static void
handle_edit (GtkWidget *widget, WPanel *panel)
{
	edit_cmd (panel);
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
	gint retval;
	file_entry *fe = &panel->dir.list [panel->selected];
	char *full_name = concat_dir_and_file (panel->cwd, fe->fname);
	GtkWidget *dlg;

#if 0
	if (item_properties (GTK_WIDGET (CLIST_FROM_SW (panel->list)), full_name, NULL) != 0)
		reread_cmd ();
#endif
	dlg = gnome_file_property_dialog_new (full_name, we_can_afford_the_speed);
	gnome_dialog_set_parent (GNOME_DIALOG (dlg),
				 GTK_WINDOW (gtk_widget_get_toplevel (panel->ministatus)));

	if (gnome_dialog_run (GNOME_DIALOG (dlg)) == 0)
		retval = gnome_file_property_dialog_make_changes (GNOME_FILE_PROPERTY_DIALOG (dlg));

	gtk_widget_destroy (dlg);
	g_free (full_name);
	if (retval)
		reread_cmd ();
}

static void
handle_open_with (GtkWidget *widget, WPanel *panel)
{
	char *command;

	command = input_expand_dialog (_(" Open with..."),
				       _("Enter extra arguments:"),
				       panel->dir.list [panel->selected].fname);
	if (!command)
		return;

	execute (command);
	g_free (command);
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

static void
handle_display_properties (GtkWidget *widget, WPanel *panel)
{
	/* FIXME */
	g_warning ("FIXME: implement popup_handle_display_properties()");
}

static void
handle_rescan (GtkWidget *widget, WPanel *panel)
{
	reread_cmd ();
}

static void
handle_arrange_icons (GtkWidget *widget, WPanel *panel)
{
	/* FIXME */
	g_warning ("FIXME: implement popup_handle_arrange_icons()");
}

static void
handle_logout (GtkWidget *widget, WPanel *panel)
{
	/* FIXME */
	g_warning ("FIXME: implement popup_handle_logout()");
}
