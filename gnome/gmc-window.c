/* Toplevel file window for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

/* #include <config.h> */
#include <gnome.h>
#include "gdesktop.h"
#include "gmc-window.h"


/* Magic numbers */

#define ICON_LIST_SEPARATORS " /-_."
#define ICON_LIST_ROW_SPACING 2
#define ICON_LIST_COL_SPACING 2
#define ICON_LIST_ICON_BORDER 2
#define ICON_LIST_TEXT_SPACING 2


static void gmc_window_init (GmcWindow *gmc);


/**
 * gmc_window_get_type:
 *
 * Returns the unique Gtk type assigned to the GmcWindow widget.
 *
 * Return Value: the type ID of the GmcWindow widget.
 **/
GtkType
gmc_window_get_type (void)
{
	static GtkType gmc_window_type = 0;

	if (!gmc_window_type) {
		GtkTypeInfo gmc_window_info = {
			"GmcWindow",
			sizeof (GmcWindow),
			sizeof (GmcWindowClass),
			(GtkClassInitFunc) NULL,
			(GtkObjectInitFunc) gmc_window_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		gmc_window_type = gtk_type_unique (gnome_app_get_type (), &gmc_window_info);
	}

	return gmc_window_type;
}

/* Displays GMC's About dialog */
static void
about_dialog (GtkWidget *widget, gpointer data)
{
	GtkWidget *about;
	const gchar *authors[] = {
		"The Midnight Commander Team",
		"http://www.gnome.org/mc",
		"Bug reports:  mc-bugs@nuclecu.unam.mx",
		NULL
	};

	about = gnome_about_new (_("GNU Midnight Commander"), VERSION,
				 _("Copyright (C) 1998 The Free Software Foundation"),
				 authors,
				 _("The GNOME edition of the Midnight Commander file manager."),
				 NULL);
	gtk_window_set_modal(GTK_WINDOW(about),TRUE);
	gnome_dialog_run (GNOME_DIALOG (about));
}

/* FIXME: put in the callbacks */

/* File menu */
static GnomeUIInfo file_menu[] = {
	{ GNOME_APP_UI_ITEM, N_("Open _new window"), NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 'n', GDK_CONTROL_MASK, NULL },

	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM, N_("_Close this window"), NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CLOSE, 'w', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_ITEM, N_("E_xit"), NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_EXIT, 'q', GDK_CONTROL_MASK, NULL },
	GNOMEUIINFO_END
};

/* View types radioitem list */
static GnomeUIInfo view_list_types_radioitems[] = {
	GNOMEUIINFO_ITEM_NONE (N_("_Listing view"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("_Icon view"), NULL, NULL),
	GNOMEUIINFO_END
};

/* View menu */
static GnomeUIInfo view_menu[] = {
	GNOMEUIINFO_TOGGLEITEM (N_("Display _tree view"), NULL, NULL, NULL),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_RADIOLIST (view_list_types_radioitems),
	GNOMEUIINFO_END
};

/* Help menu */
static GnomeUIInfo help_menu[] = {
	{ GNOME_APP_UI_ITEM, N_("_About the Midnight Commander..."), NULL, about_dialog, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ABOUT, 0, 0, NULL },
	GNOMEUIINFO_END
};

/* Main menu */
static GnomeUIInfo main_menu[] = {
	GNOMEUIINFO_SUBTREE (N_("_File"), file_menu),
	GNOMEUIINFO_SUBTREE (N_("_View"), view_menu),
	GNOMEUIINFO_SUBTREE (N_("_Help"), help_menu),
	GNOMEUIINFO_END
};

/* Sets up the menu bar for a gmc window */
static void
setup_menus (GmcWindow *gmc)
{
	gnome_app_create_menus_with_data (GNOME_APP (gmc), main_menu, gmc);
}

/* Sets up the toolbar for a gmc window */
static void
setup_toolbar (GmcWindow *gmc)
{
	/* FIXME */
}

/* Sets up the contents for a gmc window */
static void
setup_contents (GmcWindow *gmc)
{
	/* Paned container */

	gmc->paned = gtk_hpaned_new ();
	gnome_app_set_contents (GNOME_APP (gmc), gmc->paned);
	gtk_widget_show (gmc->paned);

	/* Tree view */

	gmc->tree = gtk_button_new_with_label ("Look at me!\nI am a nice tree!");
	gtk_paned_add1 (GTK_PANED (gmc->paned), gmc->tree);
	gtk_widget_show (gmc->tree);

	/* Notebook */

	gmc->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (gmc->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (gmc->notebook), FALSE);
	gtk_paned_add2 (GTK_PANED (gmc->paned), gmc->notebook);
	gtk_widget_show (gmc->notebook);

	/* List view */

	gmc->clist_sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (gmc->clist_sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_notebook_append_page (GTK_NOTEBOOK (gmc->notebook), gmc->clist_sw, NULL);
	gtk_widget_show (gmc->clist_sw);

	gmc->clist = gtk_clist_new (1); /* FIXME: how many columns? */
	gtk_container_add (GTK_CONTAINER (gmc->clist_sw), gmc->clist);
	gtk_widget_show (gmc->clist);

	/* Icon view */

	gmc->ilist_sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (gmc->ilist_sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_notebook_append_page (GTK_NOTEBOOK (gmc->notebook), gmc->ilist_sw, NULL);
	gtk_widget_show (gmc->ilist_sw);

	gmc->ilist = gnome_icon_list_new (DESKTOP_SNAP_X, NULL, TRUE);
	gnome_icon_list_set_separators (GNOME_ICON_LIST (gmc->ilist), ICON_LIST_SEPARATORS);
	gnome_icon_list_set_row_spacing (GNOME_ICON_LIST (gmc->ilist), ICON_LIST_ROW_SPACING);
	gnome_icon_list_set_col_spacing (GNOME_ICON_LIST (gmc->ilist), ICON_LIST_COL_SPACING);
	gnome_icon_list_set_icon_border (GNOME_ICON_LIST (gmc->ilist), ICON_LIST_ICON_BORDER);
	gnome_icon_list_set_text_spacing (GNOME_ICON_LIST (gmc->ilist), ICON_LIST_TEXT_SPACING);
	gnome_icon_list_set_selection_mode (GNOME_ICON_LIST (gmc->ilist), GTK_SELECTION_MULTIPLE);
	GTK_WIDGET_SET_FLAGS (gmc->ilist, GTK_CAN_FOCUS);

	gtk_container_add (GTK_CONTAINER (gmc->ilist_sw), gmc->ilist);
	gtk_widget_show (gmc->ilist);

	gtk_notebook_set_page (GTK_NOTEBOOK (gmc->notebook), gmc->list_type);

	/* FIXME: connect the clist/ilist signals, setup DnD, etc. */
}

/* Initializes the gmc window by creating all its contents */
static void
gmc_window_init (GmcWindow *gmc)
{
	gmc->list_type = FILE_LIST_ICONS; /* FIXME: load this from the configuration */

	setup_menus (gmc);
	setup_toolbar (gmc);
	setup_contents (gmc);
}

/**
 * gmc_window_new:
 *
 * Creates a new GMC toplevel file window.
 * 
 * Return Value: the newly-created window.
 **/
GtkWidget *
gmc_window_new (void)
{
	return gtk_type_new (gmc_window_get_type ());
}
