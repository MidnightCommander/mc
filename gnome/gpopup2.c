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
#include <gnome.h>
#include "../vfs/vfs.h"
#include "global.h"
#include "panel.h"
#include "cmd.h"
#include "dialog.h"
#include "ext.h"
#include "gpageprop.h"
#include "gpopup.h"
#include "main.h"
#include "gnome-file-property-dialog.h"
#define CLIST_FROM_SW(panel_list) GTK_CLIST (GTK_BIN (panel_list)->child)

/* Flags for the popup menu entries.  They specify to which kinds of files an entry is valid for. */
enum {
	F_ALL 		= 1 << 0,	/* Applies to all files */
	F_REGULAR	= 1 << 1,	/* Applies only to regular files */
	F_SYMLINK	= 1 << 2,	/* Applies only to symlink */
	F_SINGLE	= 1 << 3,	/* Applies only to a single file, not to a multiple selection */
	F_NOTDIR	= 1 << 4,	/* Applies to non-directories */
	F_DICON		= 1 << 5,	/* Applies only to desktop icons */
	F_NOTDEV        = 1 << 6,	/* Applies to non-devices only (ie. reg, lnk, dir) */
	F_ADVANCED      = 1 << 7        /* Only appears in (non-existent) Advanced mode */
};

/* typedefs */
typedef struct action {
	char *text;		/* Menu item text */
	int flags;		/* Flags from the above enum */
	GtkSignalFunc callback;	/* Callback for menu item */
} action;

/* Prototypes */
/* Multiple File commands */
static void panel_action_open_with (GtkWidget *widget, WPanel *panel);
static void popup_handle_open (GtkWidget *widget, WPanel *panel);
static void popup_handle_view (GtkWidget *widget, WPanel *panel);
static void popup_handle_view_unfiltered (GtkWidget *widget, WPanel *panel);
static void popup_handle_edit (GtkWidget *widget, WPanel *panel);
static void popup_handle_copy (GtkWidget *widget, WPanel *panel);
static void popup_handle_delete (GtkWidget *widget, WPanel *panel);
static void popup_handle_move (GtkWidget *widget, WPanel *panel);

/* F_SINGLE file commands */
static void popup_handle_properties (GtkWidget *widget, WPanel *panel);
static void popup_handle_open_with (GtkWidget *widget, WPanel *panel);
static void popup_handle_hard_link (GtkWidget *widget, WPanel *panel);
static void popup_handle_symlink (GtkWidget *widget, WPanel *panel);
static void popup_handle_edit_symlink (GtkWidget *widget, WPanel *panel);

/* Generic Options */
static void popup_handle_display_properties (GtkWidget *widget, WPanel *panel);
static void popup_handle_refresh (GtkWidget *widget, WPanel *panel);
static void popup_handle_arrange_icons (GtkWidget *widget, WPanel *panel);
static void popup_handle_logout (GtkWidget *widget, WPanel *panel);

/* global vars */
extern int we_can_afford_the_speed;

static action file_actions[] = {
	{ N_("Properties"),      F_SINGLE | F_ALL,   	  	 (GtkSignalFunc) popup_handle_properties },
	{ "",                    F_SINGLE | F_ALL,  	  	 NULL },
	{ N_("Open"),            F_NOTDEV,		  	 (GtkSignalFunc) popup_handle_open },
	{ "",                    F_NOTDEV,   	    	  	 NULL },
	{ N_("Open with..."),    F_REGULAR | F_SINGLE, 	  	 (GtkSignalFunc) popup_handle_open_with },
	{ N_("View"),            F_REGULAR | F_SINGLE,		 (GtkSignalFunc) popup_handle_view },
	{ N_("View unfiltered"), F_REGULAR | F_ADVANCED | F_SINGLE,      	 (GtkSignalFunc) popup_handle_view_unfiltered },  
	{ N_("Edit"),            F_REGULAR | F_SINGLE,     	 (GtkSignalFunc) popup_handle_edit },
	{ "",                    F_REGULAR | F_SINGLE,         	 NULL },
	{ N_("Copy..."),         F_ALL,             		 (GtkSignalFunc) popup_handle_copy },
	{ N_("Delete"),          F_ALL,             		 (GtkSignalFunc) popup_handle_delete },
	{ N_("Move..."),         F_ALL,             		 (GtkSignalFunc) popup_handle_move },
	{ "", 			 F_SINGLE, 			 NULL }, 
	{ N_("Hard Link..."),    F_ADVANCED | F_SINGLE,		 (GtkSignalFunc) popup_handle_hard_link }, /* UGH.  I can't believe this exists. */
	{ N_("Link..."),    F_SINGLE, 	                 (GtkSignalFunc) popup_handle_symlink },
	{ N_("Edit symlink..."), F_SYMLINK | F_SINGLE,           (GtkSignalFunc) popup_handle_edit_symlink },
	{ NULL, 0, NULL }
};

static action generic_actions[] = {
	{ N_("NEW(FIXME)"),      F_ALL,	 	  	  	 NULL },
	{ "",                    F_ALL,   	    	  	 NULL },
	{ N_("Change Background"),F_DICON,		      	 (GtkSignalFunc) popup_handle_display_properties },
	{ N_("Refresh"),         F_ALL,		      	  	 (GtkSignalFunc) popup_handle_refresh },
	{ N_("Arrange Icons"),   F_DICON,			 (GtkSignalFunc) popup_handle_arrange_icons },
	{ N_(""),                F_DICON,             		 NULL },
	{ N_("Logout"),          F_DICON,             		 (GtkSignalFunc) popup_handle_logout },
	{ NULL, 0, NULL }
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


/* Creates the menu items for the standard actions.  Returns the position at which additional menu
 * items should be inserted.
 */
static int
create_actions (GtkWidget *menu, gint flags, gboolean on_selected, WPanel *panel)
{
	struct action *action;
	gint pos = 0;
	gint error_correction;
	GnomeUIInfo uiinfo[] = {
		{ 0 },
		GNOMEUIINFO_END
	};

	if (on_selected)
		action = file_actions;
	else
		action = generic_actions;


	for (;action->text; action++) {
		error_correction = 0;
		/* Let's see if we want this particular entry */
		/* we need to special-case F_SINGLE */
		if (action->flags & F_SINGLE) {
			if  (!(flags & F_SINGLE))
				continue;
			else if (action->flags != F_SINGLE)
				error_correction |= F_SINGLE;
		}
		/* same with advanced */
		if (action->flags & F_ADVANCED) {
			if  (!(flags & F_ADVANCED))
				continue;
			else
				error_correction |= F_ADVANCED;
		}

		if ((flags & (action->flags - error_correction)) == 0)
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

		fill_menu (GTK_MENU_SHELL (menu), uiinfo, pos);
		pos++;
	}

	return pos;
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

/*
 * Create a context menu
 * It can take either a WPanel or a GnomeDesktopEntry.  One of them should
 * be set to NULL.
 */
#define REMOVE(x,f) if (x&f)x-=f

int gpopup_do_popup2 (GdkEventButton *event,
		      WPanel *panel, GList *file_list,
		      gboolean on_selected)
{
	GtkWidget *menu;
	GList *list;
	gint flags = F_ALL | F_REGULAR | F_SYMLINK | F_SINGLE | F_NOTDEV | F_NOTDIR;
	struct stat s;
	guint id;
	gint i;
	gint count = 0;

	g_return_val_if_fail (event != NULL, -1);

	menu = gtk_menu_new ();

	/* Connect to the deactivation signal to be able to quit our
           modal main loop */
	id = gtk_signal_connect (GTK_OBJECT (menu), "deactivate",
				 (GtkSignalFunc) menu_shell_deactivated,
				 NULL);

	if (file_list == NULL) {
		/* make the file list */
		for (i = 0; i < panel->count; i++) {
			if (!strcmp (panel->dir.list [i].fname, "..")) {
				continue;
			}
			if (panel->dir.list [i].f.marked) {
				s.st_mode = panel->dir.list[i].buf.st_mode;
				/* do flag stuff */
				if (S_ISLNK (s.st_mode))
					mc_stat (panel->dir.list [i].fname, &s);
				else
					REMOVE (flags, F_SYMLINK);
				if (S_ISDIR (s.st_mode))
					REMOVE (flags, F_NOTDIR);
				if (!S_ISREG (s.st_mode))
					REMOVE (flags, F_REGULAR);
				if (count == 1)
					REMOVE (flags, F_SINGLE);
				if (!S_ISREG (s.st_mode) && !S_ISDIR (s.st_mode))
					REMOVE (flags, F_NOTDEV);
				count++;
			}
		}
	} else {
		/* we already have the file list, but we need to create the flags */
		for (list = file_list; list; list = list->next) {
			mc_lstat (list->data, &s);
			if (S_ISLNK (s.st_mode))
				mc_stat (list->data, &s);
			else
				REMOVE (flags, F_SYMLINK);
			     
			if (S_ISDIR (s.st_mode))
				REMOVE (flags, F_NOTDIR);
			if (!S_ISREG (s.st_mode))
				REMOVE (flags, F_REGULAR);
			if (count == 1)
				REMOVE (flags, F_SINGLE);
			if (!S_ISREG (s.st_mode) && !S_ISDIR (s.st_mode))
				REMOVE (flags, F_NOTDEV);
			count++;
		}
	}

	/* Fill the menu */
	create_actions (menu, flags, on_selected, panel);

	/* Run it */
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);
	gtk_grab_add (menu);
	gtk_main ();
	gtk_grab_remove (menu);

	gtk_signal_disconnect (GTK_OBJECT (menu), id);
	/* FIXME: FIXME: FIXME: free the stoopid memory */
	return get_active_index (GTK_MENU (menu));
}

static void
panel_action_open_with (GtkWidget *widget, WPanel *panel)
{
	char *command;
	
	command = input_expand_dialog (_(" Open with..."),
				       _("Enter extra arguments:"), panel->dir.list [panel->selected].fname);
	if (!command)
		return;
	execute (command);
	 g_free (command);
}
static void
popup_handle_open (GtkWidget *widget, WPanel *panel)
{
	if (do_enter (panel))
		return;
	panel_action_open_with (widget, panel);
}
static void
popup_handle_view (GtkWidget *widget, WPanel *panel)
{
	view_cmd (panel);
}
static void
popup_handle_view_unfiltered (GtkWidget *widget, WPanel *panel)
{
	view_simple_cmd (panel);
}
static void
popup_handle_edit (GtkWidget *widget, WPanel *panel)
{
	edit_cmd (panel);
}
static void
popup_handle_copy (GtkWidget *widget, WPanel *panel)
{
	copy_cmd ();
}
static void
popup_handle_delete (GtkWidget *widget, WPanel *panel)
{
	delete_cmd ();
}
static void
popup_handle_move (GtkWidget *widget, WPanel *panel)
{
	ren_cmd ();
}

/* F_SINGLE file commands */
static void popup_handle_properties (GtkWidget *widget, WPanel *panel)
{
	gint retval;
	file_entry *fe = &panel->dir.list [panel->selected];
	char *full_name = concat_dir_and_file (panel->cwd, fe->fname);
	GtkWidget *dlg;

/*	if (item_properties (GTK_WIDGET (CLIST_FROM_SW (panel->list)), full_name, NULL) != 0)
	reread_cmd ();*/
	dlg = gnome_file_property_dialog_new (full_name, we_can_afford_the_speed);
	gnome_dialog_set_parent (GNOME_DIALOG (dlg), GTK_WINDOW (gtk_widget_get_toplevel (panel->ministatus)));
	if (gnome_dialog_run (GNOME_DIALOG (dlg)) == 0)
		retval = gnome_file_property_dialog_make_changes (GNOME_FILE_PROPERTY_DIALOG (dlg));
	gtk_widget_destroy (dlg);
	 g_free (full_name);
	if (retval)
		reread_cmd ();
}
static void popup_handle_open_with (GtkWidget *widget, WPanel *panel)
{
	char *command;
	
	command = input_expand_dialog (_(" Open with..."),
				       _("Enter extra arguments:"), panel->dir.list [panel->selected].fname);
	if (!command)
		return;
	execute (command);
	g_free (command);
}
static void popup_handle_hard_link (GtkWidget *widget, WPanel *panel)
{
	/* yeah right d:  -jrb */
	link_cmd ();
}
static void popup_handle_symlink (GtkWidget *widget, WPanel *panel)
{
	symlink_cmd ();
}
static void popup_handle_edit_symlink (GtkWidget *widget, WPanel *panel)
{
	edit_symlink_cmd ();
}
static void popup_handle_display_properties (GtkWidget *widget, WPanel *panel)
{

}
static void popup_handle_refresh (GtkWidget *widget, WPanel *panel)
{

}
static void popup_handle_arrange_icons (GtkWidget *widget, WPanel *panel)
{

}
static void popup_handle_logout (GtkWidget *widget, WPanel *panel)
{

}
