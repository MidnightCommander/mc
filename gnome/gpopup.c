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


/*
 * Flags for the context-sensitive popup menus
 */
#define F_ALL         1
#define F_REGULAR     2
#define F_SYMLINK     4
#define F_SINGLE      8
#define F_NOTDIR     16
#define F_DICON      32		/* Only applies to desktop_icon_t */
#define F_PANEL      64         /* Only applies to WPanel */


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

void
panel_action_view (GtkWidget *widget, WPanel *panel)
{
	view_cmd (panel);
}

void
panel_action_view_unfiltered (GtkWidget *widget, WPanel *panel)
{
	view_simple_cmd (panel);
}

void
panel_action_edit (GtkWidget *widget, WPanel *panel)
{
	edit_cmd (panel);
}

void
panel_action_properties (GtkWidget *widget, WPanel *panel)
{
	file_entry *fe = &panel->dir.list [panel->selected];
	char *full_name = concat_dir_and_file (panel->cwd, fe->fname);

	if (item_properties (GTK_WIDGET (CLIST_FROM_SW (panel->list)), full_name, NULL) != 0)
		reread_cmd ();

	free (full_name);
}

typedef void (*context_menu_callback)(GtkWidget *, void *);

/*
 * The context menu: text displayed, condition that must be met and
 * the routine that gets invoked upon activation.
 */
static struct {
	char *text;
	int  flags;
	context_menu_callback callback;
} file_actions [] = {
	{ N_("Properties"),      F_SINGLE | F_PANEL,   	  	 (context_menu_callback) panel_action_properties },
#if 0
	{ N_("Properties"),      F_SINGLE | F_DICON,  	  	 (context_menu_callback) desktop_icon_properties },
#endif
	{ "",                    F_SINGLE,   	    	  	 NULL },
	{ N_("Open"),            F_PANEL | F_ALL,      	  	 (context_menu_callback) panel_action_open },
#if 0
	{ N_("Open"),            F_DICON | F_ALL, 	  	 (context_menu_callback) desktop_icon_execute },
#endif
	{ N_("Open with"),       F_PANEL | F_ALL,      	  	 (context_menu_callback) panel_action_open_with },
	{ N_("View"),            F_PANEL | F_NOTDIR,      	 (context_menu_callback) panel_action_view },
	{ N_("View unfiltered"), F_PANEL | F_NOTDIR,      	 (context_menu_callback) panel_action_view_unfiltered },  
	{ N_("Edit"),            F_PANEL | F_NOTDIR,             (context_menu_callback) panel_action_edit },
	{ "",                    0,          	                 NULL },
	{ N_("Link..."),         F_PANEL | F_REGULAR | F_SINGLE, (context_menu_callback) link_cmd },
	{ N_("Symlink..."),      F_PANEL | F_SINGLE,             (context_menu_callback) symlink_cmd },
	{ N_("Edit symlink..."), F_PANEL | F_SYMLINK,            (context_menu_callback) edit_symlink_cmd },
	{ NULL, 0, NULL },
};

typedef struct {
	char *text;
	context_menu_callback callback;
} common_menu_t;
	
/*
 * context menu, constant entries
 */
common_menu_t common_panel_actions [] = {
	{ N_("Copy..."),         (context_menu_callback) copy_cmd },
	{ N_("Rename/move..."),  (context_menu_callback) ren_cmd },
	{ N_("Delete..."),       (context_menu_callback) delete_cmd },
	{ NULL, NULL }
};

common_menu_t common_dicon_actions [] = {
#if 0
	{ N_("Delete"),          (context_menu_callback) desktop_icon_delete },
#endif
	{ NULL, NULL }
};
      
static GtkWidget *
create_popup_submenu (WPanel *panel, desktop_icon_t *di, int row, char *filename)
{
	static int submenu_translated;
	GtkWidget *menu;
	int i;
	void *closure;

	closure = (panel != 0 ? (void *) panel : (void *)di);
	
	if (!submenu_translated){
		/* FIXME translate it */
		submenu_translated = 1;
	}
	
	menu = gtk_menu_new ();
	for (i = 0; file_actions [i].text; i++){
		GtkWidget *item;

		/* First, try F_PANEL and F_DICON flags */
		if (di && (file_actions [i].flags & F_PANEL))
			continue;

		if (panel && (file_actions [i].flags & F_DICON))
			continue;
		
		/* Items with F_ALL bypass any other condition */
		if (!(file_actions [i].flags & F_ALL)){

			/* Items with F_SINGLE require that ONLY ONE marked files exist */
			if (panel && file_actions [i].flags & F_SINGLE){
				if (panel->marked > 1)
					continue;
			}

			/* Items with F_NOTDIR requiere that the selection is not a directory */
			if (panel && file_actions [i].flags & F_NOTDIR){
				struct stat *s = &panel->dir.list [row].buf;

				if (panel->dir.list [row].f.link_to_dir)
					continue;
				
				if (S_ISDIR (s->st_mode))
					continue;
			}
			
			/* Items with F_REGULAR do not accept any strange file types */
			if (panel && file_actions [i].flags & F_REGULAR){
				struct stat *s = &panel->dir.list [row].buf;
				
				if (S_ISLNK (panel->dir.list [row].f.link_to_dir))
					continue;
				if (S_ISSOCK (s->st_mode) || S_ISCHR (s->st_mode) ||
				    S_ISFIFO (s->st_mode) || S_ISBLK (s->st_mode))
					continue;
			}

			/* Items with F_SYMLINK only operate on symbolic links */
			if (panel && file_actions [i].flags & F_SYMLINK){
				if (!S_ISLNK (panel->dir.list [row].buf.st_mode))
					continue;
			}
		}
		if (*file_actions [i].text)
			item = gtk_menu_item_new_with_label (_(file_actions [i].text));
		else
			item = gtk_menu_item_new ();

		gtk_widget_show (item);
		if (file_actions [i].callback){
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    GTK_SIGNAL_FUNC(file_actions [i].callback), closure);
		}

		gtk_menu_append (GTK_MENU (menu), item);
	}
	return menu;
}

/*
 * Ok, this activates a menu popup action for a filename
 * it is kind of hackish, it gets the desired action from the
 * item, so it has to peek inside the item to retrieve the label
 */
static void
popup_activate_by_string (GtkMenuItem *item, WPanel *panel)
{
	char *filename = panel->dir.list [panel->selected].fname;
	char *action;
	int movedir;
	
	g_return_if_fail (GTK_IS_MENU_ITEM (item));
	g_return_if_fail (GTK_IS_LABEL (GTK_BIN (item)->child));

	action = GTK_LABEL (GTK_BIN (item)->child)->label;

	regex_command (filename, action, NULL, &movedir);
}

static void
popup_activate_desktop_icon (GtkMenuItem *item, char *filename)
{
	char *action;
	int movedir;
	
	action = GTK_LABEL (GTK_BIN (item)->child)->label;

	regex_command (filename, action, NULL, &movedir);
}

static void
file_popup_add_context (GtkMenu *menu, WPanel *panel, desktop_icon_t *di, char *filename)
{
	GtkWidget *item;
	char *p, *q;
	int c, i;
	void *closure, *regex_closure;
	common_menu_t *menu_p;
	GtkSignalFunc regex_func;
	
	if (panel){
		menu_p        = common_panel_actions;
		closure       = panel;
		regex_func    = GTK_SIGNAL_FUNC (popup_activate_by_string);
		regex_closure = panel;
	} else {
		menu_p        = common_dicon_actions;
		closure       = di;
		regex_func    = GTK_SIGNAL_FUNC (popup_activate_desktop_icon);
		regex_closure = di->dentry->exec [0];
	}
	
	for (i = 0; menu_p [i].text; i++){
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (_(menu_p [i].text));
		gtk_widget_show (item);
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC (menu_p [i].callback), closure);
		gtk_menu_append (GTK_MENU (menu), item);
	}
	
	p = regex_command (filename, NULL, NULL, NULL);
	if (!p)
		return;
	
	item = gtk_menu_item_new ();
	gtk_widget_show (item);
	gtk_menu_append (menu, item);
	
	for (;;){
		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p)
			break;
		q = p;
		while (*q && *q != '=' && *q != '\t')
			q++;
		c = *q;
		*q = 0;
		
		item = gtk_menu_item_new_with_label (p);
		gtk_widget_show (item);

		gtk_signal_connect (GTK_OBJECT(item), "activate", regex_func, regex_closure);
		gtk_menu_append (menu, item);
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
void
file_popup (GdkEventButton *event, void *WPanel_pointer, void *desktop_icon_t_pointer, int row, char *filename)
{
	GtkWidget *menu = gtk_menu_new ();
	GtkWidget *submenu;
	GtkWidget *item;
	WPanel      *panel = WPanel_pointer;
	desktop_icon_t *di = desktop_icon_t_pointer;
	char *str;
		
	g_return_if_fail (((panel != NULL) ^ (di != NULL)));

	if (panel)
		str = (panel->marked > 1) ? "..." : filename;
	else
		str = filename;
	
	if (panel){
		item = gtk_menu_item_new_with_label (str);
		gtk_widget_show (item);
		gtk_menu_append (GTK_MENU (menu), item);

		submenu = create_popup_submenu (panel, di, row, filename);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
	} else 
		menu = create_popup_submenu (panel, di, row, filename);

	file_popup_add_context (GTK_MENU (menu), panel, di, filename);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 3, event->time);
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
