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
panel_action_properties (GtkWidget *widget, WPanel *panel)
{
	file_entry *fe = &panel->dir.list [panel->selected];
	char *full_name = concat_dir_and_file (panel->cwd, fe->fname);

	if (item_properties (GTK_WIDGET (CLIST_FROM_SW (panel->list)), full_name, NULL) != 0)
		reread_cmd ();

	free (full_name);
}

static void
dicon_move (GtkWidget *widget, char *filename)
{
	g_warning ("Implement this function!");
}

static void
dicon_copy (GtkWidget *widget, char *filename)
{
	g_warning ("Implement this function!");
}

static void
dicon_delete (GtkWidget *widget, char *filename)
{
	g_warning ("Implement this function!");
}


/* The context menu: text displayed, condition that must be met and the routine that gets invoked
 * upon activation.
 */
static struct action file_actions[] = {
	{ N_("Properties"),      F_SINGLE | F_PANEL,   	  	 (GtkSignalFunc) panel_action_properties },
#if 0
	{ N_("Properties"),      F_SINGLE | F_DICON,  	  	 (GtkSignalFunc) desktop_icon_properties },
#endif
	{ "",                    F_SINGLE,   	    	  	 NULL },
	{ N_("Open"),            F_PANEL | F_ALL,      	  	 (GtkSignalFunc) panel_action_open },
#if 0
	{ N_("Open"),            F_DICON | F_ALL, 	  	 (GtkSignalFunc) desktop_icon_execute },
#endif
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
	GNOMEUIINFO_ITEM_NONE (N_("Move/rename..."), NULL, dicon_move),
	GNOMEUIINFO_ITEM_NONE (N_("Copy..."), NULL, dicon_copy),
	GNOMEUIINFO_ITEM_NONE (N_("Delete"), NULL, dicon_delete),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_END
};


/* Creates the menu items for the standard actions.  Returns the position at which additional menu
 * items should be inserted.
 */
static int
create_actions (GtkWidget *menu, WPanel *panel, int panel_row, char *filename)
{
	gpointer closure;
	struct action *action;
	int pos;
	GnomeUIInfo uiinfo[] = {
		{ 0 },
		GNOMEUIINFO_END
	};

	closure = panel ? (gpointer) panel : (gpointer) filename;

	pos = 0;
	
	for (action = file_actions; action->text; action++) {
		/* First, try F_PANEL and F_DICON flags */

		if (panel && !(action->flags & F_PANEL))
			continue;

		if (!panel && (action->flags & F_DICON))
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

		gnome_app_fill_menu (GTK_MENU_SHELL (menu), uiinfo, NULL, FALSE, pos);
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
regex_command_from_panel (GtkMenuItem *item, WPanel *panel)
{
	char *filename;
	char *action;
	int movedir;

	filename = panel->dir.list[panel->selected].fname;
	action = get_label_text (item);

	regex_command (filename, action, NULL, &movedir);
}

static void
regex_command_from_desktop_icon (GtkMenuItem *item, char *filename)
{
	char *action;
	int movedir;

	action = get_label_text (item);

	regex_command (filename, action, NULL, &movedir);
}

/* Create the menu items common to files from panel window and desktop icons, and also the items for
 * regexp_command() actions.
 */
static void
create_regexp_actions (GtkWidget *menu, WPanel *panel, int panel_row, char *filename, int insert_pos)
{
	gpointer closure, regex_closure;
	GnomeUIInfo *a_uiinfo;
	int i;
	GtkSignalFunc regex_callback;
	char *p, *q;
	int c;
	GnomeUIInfo uiinfo[] = {
		{ 0 },
		GNOMEUIINFO_END
	};

	if (panel) {
		a_uiinfo = panel_actions;
		closure = panel;
		regex_callback = regex_command_from_panel;
		regex_closure = panel;
	} else {
		a_uiinfo = desktop_icon_actions;
		closure = filename;
		regex_callback = regex_command_from_desktop_icon;
		regex_closure = filename;
	}

	/* Fill in the common part of the menus */

	for (i = 0; i < 5; i++)
		a_uiinfo[i].user_data = closure;

	gnome_app_fill_menu (GTK_MENU_SHELL (menu), a_uiinfo, NULL, FALSE, insert_pos);
	insert_pos += 5; /* the number of items from the common menus */

	/* Fill in the regex command part */

	p = regex_command (filename, NULL, NULL, NULL);
	if (!p)
		return;

	for (;;) {
		/* Strip off an entry */

		while (*p == ' ' || *p == '\t')
			p++;

		if (!*p)
			break;

		q = p;
		while (*q && *q != '=' && *q != '\t')
			q++;

		c = *q;
		*q = 0;

		/* Create the item for that entry */

		uiinfo[0].type = GNOME_APP_UI_ITEM;
		uiinfo[0].label = p;
		uiinfo[0].hint = NULL;
		uiinfo[0].moreinfo = regex_callback;
		uiinfo[0].user_data = regex_closure;
		uiinfo[0].unused_data = NULL;
		uiinfo[0].pixmap_type = GNOME_APP_PIXMAP_NONE;
		uiinfo[0].pixmap_info = NULL;
		uiinfo[0].accelerator_key = 0;
		uiinfo[0].ac_mods = 0;
		uiinfo[0].widget = NULL;

		gnome_app_fill_menu (GTK_MENU_SHELL (menu), uiinfo, NULL, FALSE, insert_pos++);

		/* Next! */

		if (!c)
			break;

		p = q + 1;
	}
}

/*
 * Create a context menu
 * It can take either a WPanel or a GnomeDesktopEntry.  One of them should
 * be set to NULL.
 */
int
gpopup_do_popup (GdkEventButton *event, WPanel *from_panel, int panel_row, char *filename)
{
	GtkWidget *menu;
	int pos;

	g_return_val_if_fail (event != NULL, -1);
	g_return_val_if_fail ((from_panel != NULL) ^ (filename != NULL), -1);

	pos = create_actions (menu, from_panel, panel_row, filename);
	create_regexp_actions (menu, from_panel, panel_row, filename, pos);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 3, event->time);
	return 0; /* FIXME: return the index of the selected item */
}









#if 0

#include <config.h>
#include <sys/stat.h>
#include "util.h"
#include <gnome.h>
#include "gpopup.h"
#define WANT_WIDGETS /* yuck */
#include "main.h"
#include "../vfs/vfs.h"


static void popup_open (GtkWidget *widget, gpointer data);
static void popup_open_new_window (GtkWidget *widget, gpointer data);
static void popup_open_with_program (GtkWidget *widget, gpointer data);
static void popup_open_with_arguments (GtkWidget *widget, gpointer data);
static void popup_view (GtkWidget *widget, gpointer data);
static void popup_edit (GtkWidget *widget, gpointer data);
static void popup_move (GtkWidget *widget, gpointer data);
static void popup_copy (GtkWidget *widget, gpointer data);
static void popup_link (GtkWidget *widget, gpointer data);
static void popup_delete (GtkWidget *widget, gpointer data);
static void popup_properties (GtkWidget *widget, gpointer data);


/* Keep this in sync with the popup_info array defined below, as these values are used to figure out
 * which items to enable/disable as appropriate.
 */
enum {
	POPUP_OPEN			= 0,
	POPUP_OPEN_IN_NEW_WINDOW	= 1,
	POPUP_OPEN_WITH_PROGRAM		= 2,
	POPUP_OPEN_WITH_ARGUMENTS	= 3,
	POPUP_VIEW			= 5,
	POPUP_EDIT			= 6,
	POPUP_VIEW_EDIT_SEPARATOR	= 7
};

/* The generic popup menu */
static GnomeUIInfo popup_info[] = {
	GNOMEUIINFO_ITEM_NONE (N_("_Open"), NULL, popup_open),
	GNOMEUIINFO_ITEM_NONE (N_("Open in _new window"), NULL, popup_open_new_window),
	GNOMEUIINFO_ITEM_NONE (N_("Open with pro_gram..."), NULL, popup_open_with_program),
	GNOMEUIINFO_ITEM_NONE (N_("Open with _arguments..."), NULL, popup_open_with_arguments),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_NONE (N_("_View"), NULL, popup_view),
	GNOMEUIINFO_ITEM_NONE (N_("_Edit"), NULL, popup_edit),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_NONE (N_("_Move/rename..."), NULL, popup_move),
	GNOMEUIINFO_ITEM_NONE (N_("_Copy..."), NULL, popup_copy),
	GNOMEUIINFO_ITEM_NONE (N_("_Link..."), NULL, popup_link),
	GNOMEUIINFO_ITEM_NONE (N_("_Delete"), NULL, popup_delete),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_NONE (N_("_Properties..."), NULL, popup_properties),
	GNOMEUIINFO_END
};

struct popup_file_info {
	char *filename;
	WPanel *panel;
};

int
gpopup_do_popup (char *filename, WPanel *from_panel, GdkEventButton *event)
{
	GtkWidget *popup;
	struct stat s;
	int result;
	struct popup_file_info pfi;

	if (mc_stat (filename, &s) != 0) {
		g_warning ("Could not stat %s, no popup menu will be run", filename);
		return -1;
	}

	popup = gnome_popup_menu_new (popup_info);

	/* Hide the menu items that are not appropriate */

	if (S_ISDIR (s.st_mode)) {
		if (!from_panel)
			gtk_widget_hide (popup_info[POPUP_OPEN].widget); /* Only allow "open in new window" */

		gtk_widget_hide (popup_info[POPUP_OPEN_WITH_ARGUMENTS].widget);
		gtk_widget_hide (popup_info[POPUP_VIEW].widget);
		gtk_widget_hide (popup_info[POPUP_EDIT].widget);
		gtk_widget_hide (popup_info[POPUP_VIEW_EDIT_SEPARATOR].widget);
	} else {
		gtk_widget_hide (popup_info[POPUP_OPEN_IN_NEW_WINDOW].widget);

		if (is_exe (s.st_mode))
			gtk_widget_hide (popup_info[POPUP_OPEN_WITH_PROGRAM].widget);
		else
			gtk_widget_hide (popup_info[POPUP_OPEN_WITH_ARGUMENTS].widget);
	}

	/* Go! */

	pfi.filename = filename;
	pfi.panel = from_panel;

	result = gnome_popup_menu_do_popup_modal (popup, NULL, NULL, event, &pfi);

	gtk_widget_destroy (popup);
	return result;
}

static void
popup_open (GtkWidget *widget, gpointer data)
{
	struct popup_file_info *pfi;
	struct stat s;

	pfi = data;

	if (mc_stat (pfi->filename, &s) != 0) {
		g_warning ("Could not stat %s", pfi->filename);
		return;
	}

	do_enter (pfi->panel);

#if 0
	if (S_ISDIR (s.st_mode)) {
		/* Open the directory in the panel the menu was activated from */

		g_assert (pfi->panel != NULL);

		do_panel_cd (pfi->panel, pfi->filename, cd_exact);
	} else if (is_exe (s.st_mode)) {
		/* FIXME: execute */
	} else {
		/* FIXME: get default program and launch it with this file */
	}
#endif
}

static void
popup_open_new_window (GtkWidget *widget, gpointer data)
{
	/* FIXME */
	g_warning ("Implement this function!");
}

static void
popup_open_with_program (GtkWidget *widget, gpointer data)
{
	/* FIXME */
	g_warning ("Implement this function!");
}

static void
popup_open_with_arguments (GtkWidget *widget, gpointer data)
{
	/* FIXME */
	g_warning ("Implement this function!");
}

static void
popup_view (GtkWidget *widget, gpointer data)
{
	/* FIXME */
	g_warning ("Implement this function!");
}

static void
popup_edit (GtkWidget *widget, gpointer data)
{
	/* FIXME */
	g_warning ("Implement this function!");
}

static void
popup_move (GtkWidget *widget, gpointer data)
{
	/* FIXME */
	g_warning ("Implement this function!");
}

static void
popup_copy (GtkWidget *widget, gpointer data)
{
	/* FIXME */
	g_warning ("Implement this function!");
}

static void
popup_link (GtkWidget *widget, gpointer data)
{
	/* FIXME */
	g_warning ("Implement this function!");
}

static void
popup_delete (GtkWidget *widget, gpointer data)
{
	/* FIXME */
	g_warning ("Implement this function!");
}

static void
popup_properties (GtkWidget *widget, gpointer data)
{
	/* FIXME */
	g_warning ("Implement this function!");
}

#endif
