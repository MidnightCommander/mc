/*
 * Layout routines for the GNOME edition of the GNU Midnight Commander
 *
 * (C) 1998 the Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@kernel.org)
 */
#include <config.h>
#include "x.h"
#include <stdio.h>
#include <sys/stat.h>
#include "dir.h"
#include "panel.h"
#include "gscreen.h"
#include "main.h"
#include "gmain.h"
#include "cmd.h"
#include "boxes.h"
#include "panelize.h"
#include "gcmd.h"
#include "gcliplabel.h"
#include "gdesktop.h"
#include "setup.h"
#include "../vfs/vfs.h"

#define UNDEFINED_INDEX -1

GList *containers = 0;

int output_lines = 0;
int command_prompt = 1;
int keybar_visible = 1;
int message_visible = 1;
int xterm_hintbar = 0;

PanelContainer *current_panel_ptr, *other_panel_ptr;

WPanel *
get_current_panel (void)
{
	if (current_panel_ptr)
		return current_panel_ptr->panel;
	else
		return NULL;
}

WPanel *
get_other_panel (void)
{
	if (other_panel_ptr)
		return other_panel_ptr->panel;
	else
		return NULL;
}

/* FIXME: we probably want to get rid of this code */
int
get_current_index (void)
{
	GList *p;
	int i;
	
	for (i = 0, p = containers; p; p = p->next, i++){
		if (p->data == current_panel_ptr)
			return i;
	}
	printf ("FATAL: current panel is not in the list\n");
	g_assert_not_reached ();
	return -1; /* keep -Wall happy */
}

int
get_other_index (void)
{
	GList *p;
	int i;
	
	for (i = 0, p = containers; p; p = p->next, i++){
		if (p->data == other_panel_ptr)
			return i;
	}
	return UNDEFINED_INDEX;
}

void
set_current_panel (int index)
{
	GList *p;
	
	for (p = containers; index; p = p->next)
		index--;
	current_panel_ptr = p->data; 
}

void
set_new_current_panel (WPanel *panel)
{
	GList *p;

	if (g_list_length (containers) > 1)
		other_panel_ptr = current_panel_ptr;
	
	for (p = containers; p; p = p->next){
		if (((PanelContainer *)p->data)->panel == panel){
			current_panel_ptr = p->data;
		}
	}
}

/*
 * Tries to assign other_panel (ie, if there is anything to assign to
 */
static void
assign_other (void)
{
	GList *p;

	other_panel_ptr = NULL;
	for (p = containers; p; p = p->next)
		if (p->data != current_panel_ptr){
			other_panel_ptr = p->data;
			printf ("PANEL: Found another other\n");
			break;
		}
}

/*
 * This keeps track of the current_panel_ptr and other_panel_ptr as
 * well as the list of active containers
 */
void
layout_panel_gone (WPanel *panel)
{
	PanelContainer *pc_holder = 0;
	int len = g_list_length (containers);
	GList *p;
	
	for (p = containers; p; p = p->next){
		PanelContainer *pc = p->data;

		if (pc->panel == panel){
			pc_holder = pc;
			break;
		}
	}

	if (len > 1){
		containers = g_list_remove (containers, pc_holder);
	}

	/* Check if this is not the current panel */
	if (current_panel_ptr->panel == panel){
		if (other_panel_ptr){
			current_panel_ptr = other_panel_ptr;
			assign_other ();
		} else {
			current_panel_ptr = NULL;
		}
	} else if (other_panel_ptr->panel == panel){
	        /* Check if it was the other panel */
		if (len == 1){
			other_panel_ptr = 0;
		} else 
			assign_other ();
	} else {
	}

	if (len == 1){
		g_free (containers->data);
		g_list_free (containers);
		containers = NULL;
	} else
		g_free (pc_holder);
}

void
set_hintbar (char *str)
{
  /*gtk_label_set (GTK_LABEL (current_panel_ptr->panel->status), str);*/
	x_flush_events ();
}

void
print_vfs_message (char *msg, ...)
{
	va_list ap;
	char str [256];
	
	va_start(ap, msg);
	vsprintf(str, msg, ap);
	va_end(ap);
	if (midnight_shutdown)
		return;
	
	set_hintbar(str);
}

void
rotate_dash (void)
{
}

int
get_current_type (void)
{
	return view_listing;
}


int
get_other_type (void)
{
	return other_panel_ptr ? view_listing : view_nothing;
}

int
get_display_type (int index)
{
	GList *p;

	if (index == UNDEFINED_INDEX)
		return -1;
	
	p = g_list_nth (containers, index);
	if (p)
		return ((PanelContainer *)p->data)->panel->list_type;
	else
		return -1;
}

void
use_dash (int ignore)
{
	/* we dont care in the gnome edition */
}

Widget *
get_panel_widget (int index)
{
	GList *p;
	
	for (p = containers; index; p = p->next)
		index--;
	return (Widget *) ((PanelContainer *)p->data)->panel;
}

/* FIXME: This routine is wrong.  It should follow what the original save_panel_types
 * does.  I can not remember which problem the original routine was trying to address
 * when I did the "New {Left|Rigth} Panel" sections.
 */
void
save_panel_types (void)
{
	GList *p;
	
	for (p = containers; p; p = p->next){
		PanelContainer *pc = p->data;

		panel_save_setup (pc->panel, pc->panel->panel_name);
	}
}

void configure_box (void);

GtkCheckMenuItem *gnome_toggle_snap (void);

GnomeUIInfo gnome_panel_file_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("_New window"),        N_("Opens a new window"), gnome_open_panel },
	/* We want to make a new menu entry here... */
	/* For example: */
	/* New-> */
	/*  Command Prompt */
	/*  Gimp Image */
	/*  Gnumeric Spreadsheet */
	/*  Text Document */
	/*  etc... */
	{ GNOME_APP_UI_SEPARATOR },
	{ GNOME_APP_UI_ITEM, N_("_Open"),              N_("Open selected files"), NULL, NULL,
	  NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN },
	{ GNOME_APP_UI_ITEM, N_("_Copy..."),           N_("Copy files"), copy_cmd, NULL},
	{ GNOME_APP_UI_ITEM, N_("_Move..."),           N_("Rename or move files"), ren_cmd },
	{ GNOME_APP_UI_ITEM, N_("_Delete..."),         N_("Delete files from disk"), delete_cmd },
	{ GNOME_APP_UI_SEPARATOR },
	{ GNOME_APP_UI_ITEM, N_("C_lose"),             N_("Close this panel"), gnome_close_panel, NULL,
	  NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CLOSE },
	{ GNOME_APP_UI_ENDOFINFO, 0, 0 }
};

GnomeUIInfo gnome_panel_edit_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("_Cut"),               N_("Cuts the selected files into the cut buffer."),  NULL, NULL,
	  NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CUT },
	{ GNOME_APP_UI_ITEM, N_("C_opy"),              N_("Copies the selected files into the cut buffer."),  NULL, NULL,
	  NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_COPY },
	{ GNOME_APP_UI_ITEM, N_("_Paste"),             N_("Pastes files from the cut buffer into the current directory"),  NULL, NULL,
	  NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PASTE },
	{ GNOME_APP_UI_SEPARATOR },
	{ GNOME_APP_UI_ITEM, N_("_Select Files..."),   N_("Select a group of files"), select_cmd },
	{ GNOME_APP_UI_ITEM, N_("_Invert Selection"),  N_("Reverses the list of tagged files"), reverse_selection_cmd },
	{ GNOME_APP_UI_ITEM, N_("_Rescan Directory"),  N_("Rescan the directory contents"), reread_cmd },
	{ GNOME_APP_UI_SEPARATOR },
	{ GNOME_APP_UI_ITEM, N_("Preferences..."),     N_("Configure the GNOME Midnight Commander"), configure_box, NULL,
	  NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PROP},
	{ GNOME_APP_UI_ENDOFINFO, 0, 0 }
};

GnomeUIInfo gnome_panel_view_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("Icon View"), NULL, gnome_icon_view_cmd, NULL},
	{ GNOME_APP_UI_ITEM, N_("Partial View"), NULL, gnome_partial_view_cmd, NULL},
	{ GNOME_APP_UI_ITEM, N_("Full View"), NULL, gnome_full_view_cmd, NULL},
	{ GNOME_APP_UI_ITEM, N_("Custom View"), NULL, gnome_custom_view_cmd, NULL},
	{GNOME_APP_UI_ENDOFINFO, NULL, NULL, NULL, NULL, NULL,
	 GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL}
};

GnomeUIInfo gnome_panel_layout_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("_Sort By..."),     N_("Confirmation settings"), gnome_sort_cmd },
	{ GNOME_APP_UI_ITEM, N_("_Filter View..."),    N_("Global option settings"), filter_cmd },
	{ GNOME_APP_UI_SEPARATOR },
	{ GNOME_APP_UI_RADIOITEMS, NULL , NULL, gnome_panel_view_menu},
	{ GNOME_APP_UI_ENDOFINFO, 0, 0 }
};

GnomeUIInfo gnome_panel_commands_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("_Find File..."),      N_("Locate files on disk"),   find_cmd, NULL,
	  NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_JUMP_TO},
	  
	{ GNOME_APP_UI_ITEM, N_("_Compare panels..."), N_("Compare two panel contents"), gnome_compare_panels },
	{ GNOME_APP_UI_ITEM, N_("_Run Command..."),    N_("Run a command and put the results in a panel"), external_panelize },
#ifdef USE_VFS					  
	{ GNOME_APP_UI_ITEM, N_("_Active VFS list..."),N_("List of active virtual file systems"), reselect_vfs },
#endif
#ifdef USE_EXT2FSLIB
	/*does this do anything?*/
/*	{ GNOME_APP_UI_ITEM, N_("_Undelete files (ext2fs only)..."), N_("Recover deleted files"), undelete_cmd },*/
#endif
#ifdef WITH_BACKGROUND
	{ GNOME_APP_UI_ITEM, N_("_Background jobs..."),   N_("List of background operations"), jobs_cmd },
#endif
	{ GNOME_APP_UI_ENDOFINFO, 0, 0 }
};


GnomeUIInfo gnome_panel_about_menu [] = {
/*	GNOMEUIINFO_HELP ("midnight-commander"), */
	{ GNOME_APP_UI_ITEM, N_("_About"),              N_("Information on this program"), gnome_about_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ABOUT },
	GNOMEUIINFO_END
};

GnomeUIInfo gnome_panel_menu [] = {
	{ GNOME_APP_UI_SUBTREE, N_("_File"),     NULL, &gnome_panel_file_menu },
	{ GNOME_APP_UI_SUBTREE, N_("_Edit"),     NULL, &gnome_panel_edit_menu },
	{ GNOME_APP_UI_SUBTREE, N_("_Layout"),   NULL, &gnome_panel_layout_menu },
	{ GNOME_APP_UI_SUBTREE, N_("_Commands"), NULL, &gnome_panel_commands_menu },
	{ GNOME_APP_UI_SUBTREE, N_("_Help"),     NULL, &gnome_panel_about_menu },
	{ GNOME_APP_UI_ENDOFINFO, 0, 0 }
};

GtkCheckMenuItem *
gnome_toggle_snap (void)
{
	return NULL; /*GTK_CHECK_MENU_ITEM (gnome_panel_desktop_menu [1].widget);*/
}

void
gnome_init_panels (void)
{
	current_panel_ptr = NULL;
	other_panel_ptr = NULL;
}

static void
gnome_close_panel_event (GtkWidget *widget, GdkEvent *event, WPanel *panel)
{
	gnome_close_panel (widget, panel);
}

static void
panel_enter_event (GtkWidget *widget, GdkEvent *event, WPanel *panel)
{
	/* Avoid unnecessary code execution */
	if (get_current_panel () == panel)
		return;
	
	set_new_current_panel (panel);
	dlg_select_widget (panel->widget.parent, panel);
	send_message (panel->widget.parent, (Widget *) panel, WIDGET_FOCUS, 0);
}

struct _TbItems {
	char *key, *text, *tooltip, *icon;
	void (*cb) (GtkWidget *, void *);
	GtkWidget *widget; /* will be filled in */
};
typedef struct _TbItems TbItems;

void user_menu_cmd (void);

static TbItems tb_items[] =
{
/* 1Help   2Menu   3View   4Edit   5Copy   6RenMov 7Mkdir  8Delete 9PullDn 10Quit */
    {"F1", "Help", "Interactive help browser", GNOME_STOCK_MENU_BLANK, (void (*) (GtkWidget *, void *)) help_cmd, 0},
    {"F2", "Menu", "User actions", GNOME_STOCK_MENU_BLANK, (void (*) (GtkWidget *, void *)) user_menu_cmd, 0},
    {"F3", "View", "View file", GNOME_STOCK_MENU_BLANK, (void (*) (GtkWidget *, void *)) view_panel_cmd, 0},
    {"F4", "Edit", "Edit file", GNOME_STOCK_MENU_BLANK, (void (*) (GtkWidget *, void *)) edit_panel_cmd, 0},
    {"F5", "Copy", "Copy file or directory", GNOME_STOCK_MENU_COPY, (void (*) (GtkWidget *, void *)) copy_cmd, 0},
    {"F6", "Move", "Rename or move a file or directory", GNOME_STOCK_MENU_BLANK, (void (*) (GtkWidget *, void *)) ren_cmd, 0},
    {"F7", "Mkdir", "Create directory", GNOME_STOCK_MENU_BLANK, (void (*) (GtkWidget *, void *)) mkdir_panel_cmd, 0},
    {"F8", "Dlete", "Delete file or directory", GNOME_STOCK_MENU_BLANK, (void (*) (GtkWidget *, void *)) delete_cmd, 0},
    {"F9", "Menu", "Pull down menu", GNOME_STOCK_MENU_BLANK, (void (*) (GtkWidget *, void *)) 0, 0},
    {"F10", "Quit", "Close and exit", GNOME_STOCK_MENU_QUIT, (void (*) (GtkWidget *, void *)) 0, 0},
    {0, 0, 0, 0, 0, 0}
};

static GtkWidget *create_toolbar (GtkWidget * window, GtkWidget *widget)
{
    GtkWidget *toolbar;
    TbItems *t;
    toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
    for (t = &tb_items[0]; t->text; t++) {
	t->widget = gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
					     t->text,
					     t->tooltip,
					     0,
			     gnome_stock_pixmap_widget (window, t->icon),
					     t->cb,
					     t->cb ? widget : 0);
    }
    return toolbar;
}

WPanel *
create_container (Dlg_head *h, char *name, char *geometry)
{
	PanelContainer *container = g_new (PanelContainer, 1);
	WPanel     *panel;
	GtkWidget  *app, *vbox;
	int        xpos, ypos, width, height;

	gnome_parse_geometry (geometry, &xpos, &ypos, &width, &height);
	
	container->splitted = 0;
	app = gnome_app_new ("gmc", name);
	gtk_window_set_wmclass (GTK_WINDOW (app), "gmc", "gmc");

	/* Geometry configuration */
	if (width != -1 && height != -1)
		gtk_widget_set_usize (GTK_WIDGET (app), width, height);
	else
		gtk_widget_set_usize (GTK_WIDGET (app), 540, 360);
	if (xpos != -1 && ypos != -1)
		gtk_widget_set_uposition (GTK_WIDGET (app), xpos, ypos);

	panel = panel_new (name);
	vbox = gtk_vbox_new (0, 0);
	gnome_app_set_contents (GNOME_APP (app), vbox);
	gnome_app_create_menus_with_data (GNOME_APP (app), gnome_panel_menu, panel);

	/*
	 * I am trying to unclutter the screen, so this toolbar is gone now
	 */
/*	gnome_app_set_toolbar(GNOME_APP (app), GTK_TOOLBAR(create_toolbar(app, 0))); */

	gtk_signal_connect (GTK_OBJECT (app),
			    "enter_notify_event",
			    GTK_SIGNAL_FUNC (panel_enter_event),
			    panel);
	gtk_signal_connect (GTK_OBJECT (app),
			    "delete_event",
			    GTK_SIGNAL_FUNC (gnome_close_panel_event),
			    panel);
	/* Ultra nasty hack follows:
	 * I am setting the panel->widget.wdata value here before the
	 * panel X stuff gets created in the INIT message section of the
	 * widget.  There I put a pointer to the vbox where the panel
	 * should pack itself
	 */
	panel->widget.wdata = (widget_data) vbox;
	container->panel = panel;
	
	containers = g_list_append (containers, container);

	if (!current_panel_ptr){
		current_panel_ptr = container;
	} else if (!other_panel_ptr)
		other_panel_ptr = container;

	bind_gtk_keys (GTK_WIDGET (app), h);
	return panel;
}

WPanel *
new_panel_with_geometry_at (char *dir, char *geometry)
{
	WPanel *panel;

	mc_chdir (dir);
	panel = create_container (desktop_dlg, dir, geometry);
	add_widget (desktop_dlg, panel);
	set_new_current_panel (panel);
	x_flush_events ();

	return panel;
}

WPanel *
new_panel_at (char *dir)
{
	return new_panel_with_geometry_at (dir, NULL);
}

void
setup_panels (void)
{
	load_hint ();
}

/*
 * GNOME's implementation of the update_panels routine
 */
void
update_panels (int force_update, char *current_file)
{
	int reload_others = !(force_update & UP_ONLY_CURRENT);
	GList *p;

	/* Test if there are panels open */
	if (!cpanel)
		return;
	
	update_one_panel_widget (cpanel, force_update, current_file);

	if (reload_others){
		for (p = containers; p; p = p->next){
			PanelContainer *pc = p->data;
			
			if (p->data == current_panel_ptr)
				continue;
			
			update_one_panel_widget (pc->panel, force_update, UP_KEEPSEL);
		}
	}
	mc_chdir (cpanel->cwd);
}
