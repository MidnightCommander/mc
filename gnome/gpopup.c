/* Popup menus for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#include <config.h>
#include <sys/stat.h>
#include "util.h"
#include <gnome.h>
#include "gpopup.h"
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
	POPUP_EDIT			= 6
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

int
gpopup_do_popup (char *filename, int from_panel, GdkEventButton *event)
{
	GtkWidget *popup;
	struct stat s;
	int result;

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
	} else {
		gtk_widget_hide (popup_info[POPUP_OPEN_IN_NEW_WINDOW].widget);

		if (is_exe (s.st_mode))
			gtk_widget_hide (popup_info[POPUP_OPEN_WITH_PROGRAM].widget);
		else
			gtk_widget_hide (popup_info[POPUP_OPEN_WITH_ARGUMENTS].widget);
	}

	/* Go! */

	result = gnome_popup_menu_do_popup_modal (popup, NULL, NULL, event, filename);

	gtk_widget_destroy (popup);
	return result;
}

static void
popup_open (GtkWidget *widget, gpointer data)
{
	/* FIXME */
}

static void
popup_open_new_window (GtkWidget *widget, gpointer data)
{
	/* FIXME */
}

static void
popup_open_with_program (GtkWidget *widget, gpointer data)
{
	/* FIXME */
}

static void
popup_open_with_arguments (GtkWidget *widget, gpointer data)
{
	/* FIXME */
}

static void
popup_view (GtkWidget *widget, gpointer data)
{
	/* FIXME */
}

static void
popup_edit (GtkWidget *widget, gpointer data)
{
	/* FIXME */
}

static void
popup_move (GtkWidget *widget, gpointer data)
{
	/* FIXME */
}

static void
popup_copy (GtkWidget *widget, gpointer data)
{
	/* FIXME */
}

static void
popup_link (GtkWidget *widget, gpointer data)
{
	/* FIXME */
}

static void
popup_delete (GtkWidget *widget, gpointer data)
{
	/* FIXME */
}

static void
popup_properties (GtkWidget *widget, gpointer data)
{
	/* FIXME */
}
