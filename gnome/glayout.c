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
	gtk_label_set (GTK_LABEL (current_panel_ptr->panel->status), str);
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
	{ GNOME_APP_UI_ITEM, N_("Open Terminal"),     N_("Opens a terminal"), gnome_open_terminal },
	{ GNOME_APP_UI_SEPARATOR },	
	{ GNOME_APP_UI_ITEM, N_("Copy..."),           N_("Copy files"),       copy_cmd },
	{ GNOME_APP_UI_ITEM, N_("Rename/Move..."),    N_("Rename or move files"), ren_cmd },
	{ GNOME_APP_UI_ITEM, N_("New directory..."),  N_("Creates a new folder"), mkdir_cmd },
	{ GNOME_APP_UI_ITEM, N_("Delete..."),         N_("Delete files from disk"), delete_cmd },
	{ GNOME_APP_UI_SEPARATOR },	
	{ GNOME_APP_UI_ITEM, N_("View"),              N_("View file"), panel_action_view },
	{ GNOME_APP_UI_ITEM, N_("View raw"),          N_("View the file without further processing"),panel_action_view_unfiltered},
	{ GNOME_APP_UI_SEPARATOR },	
	{ GNOME_APP_UI_ITEM, N_("Select group by pattern..."),   N_("Selects a group of files"), select_cmd },
	{ GNOME_APP_UI_ITEM, N_("Unselect group by pattern..."), N_("Un-selects a group of marked files"), unselect_cmd },
	{ GNOME_APP_UI_ITEM, N_("Reverse selection"), N_("Reverses the list of tagged files"), reverse_selection_cmd },
	{ GNOME_APP_UI_SEPARATOR },
	{ GNOME_APP_UI_ITEM, N_("Close"),             N_("Close this panel"), gnome_close_panel },
	{ GNOME_APP_UI_ITEM, N_("Exit"),              N_("Exit program"), gnome_quit_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_EXIT },
	{ GNOME_APP_UI_ENDOFINFO, 0, 0 }
};

GnomeUIInfo gnome_panel_panel_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("New window"),        N_("Opens a new window"), gnome_open_panel },
	{ GNOME_APP_UI_ITEM, N_("Display mode..."),   N_("Set the display mode for the panel"),  gnome_listing_cmd },
	{ GNOME_APP_UI_ITEM, N_("Sort order..."),     N_("Changes the sort order of the files"), sort_cmd },
	{ GNOME_APP_UI_ITEM, N_("Filter..."),         N_("Set a filter for the files"), filter_cmd },
	{ GNOME_APP_UI_ITEM, N_("Rescan"),            N_("Rescan the directory contents"), reread_cmd },
	{ GNOME_APP_UI_SEPARATOR },
#ifdef USE_NETCODE
	{ GNOME_APP_UI_ITEM, N_("Network link..."),   N_("Connect to a remote machine"), netlink_cmd },
	{ GNOME_APP_UI_ITEM, N_("FTP link..."),       N_("Connect to a remote machine with FTP"), ftplink_cmd },
#endif
	{ GNOME_APP_UI_ENDOFINFO, 0, 0 }
};

GnomeUIInfo gnome_panel_options_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("Confirmation..."),   N_("Confirmation settings"), confirm_box },
	{ GNOME_APP_UI_ITEM, N_("Options..."),        N_("Global option settings"), configure_box, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PROP },
#ifdef USE_VFS
	{ GNOME_APP_UI_ITEM, N_("Virtual FS..."),     N_("Virtual File System settings"), configure_vfs },
#endif
	{ GNOME_APP_UI_SEPARATOR },
	{ GNOME_APP_UI_ITEM, N_("Save setup"),        NULL, save_setup_cmd },
	{ GNOME_APP_UI_ENDOFINFO, 0, 0 }
};

GnomeUIInfo gnome_panel_commands_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("Find..."),           N_("Locate files on disk"),   find_cmd },
	{ GNOME_APP_UI_ITEM, N_("Hotlist..."),        N_("List of favorite sites"), quick_chdir_cmd },
	{ GNOME_APP_UI_ITEM, N_("Compare panels..."), N_("Compare panel contents"), gnome_compare_panels },
	{ GNOME_APP_UI_ITEM, N_("External panelize..."), NULL, external_panelize },
#ifdef USE_VFS					  
	{ GNOME_APP_UI_ITEM, N_("Active VFS list..."),   N_("List of active virtual file systems"), reselect_vfs },
#endif						  
#ifdef USE_EXT2FSLIB
	{ GNOME_APP_UI_ITEM, N_("Undelete files (ext2fs only)..."), N_("Recover deleted files"), undelete_cmd },
#endif
#ifdef WITH_BACKGROUND
	{ GNOME_APP_UI_ITEM, N_("Background jobs..."),   N_("List of background operations"), jobs_cmd },
#endif
	{ GNOME_APP_UI_ENDOFINFO, 0, 0 }
};

GnomeUIInfo gnome_panel_desktop_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("Arrange icons"),     N_("Arranges the icons on the desktop"), gnome_arrange_icons },
/*	{ GNOME_APP_UI_TOGGLEITEM, N_("Desktop grid"), N_("Use a grid for laying out icons"), gnome_toggle_snap }, */
	{ GNOME_APP_UI_ENDOFINFO, 0, 0 }
};

GnomeUIInfo gnome_panel_about_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("About"),              N_("Information on this program"), gnome_about_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ABOUT },
	GNOMEUIINFO_HELP ("midnight-commander"),
	GNOMEUIINFO_END
};

GnomeUIInfo gnome_panel_menu [] = {
	{ GNOME_APP_UI_SUBTREE, N_("File"),     NULL, &gnome_panel_file_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Window"),    NULL, &gnome_panel_panel_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Commands"), NULL, &gnome_panel_commands_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Options"),  NULL, &gnome_panel_options_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Desktop"),  NULL, &gnome_panel_desktop_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Help"),     NULL, &gnome_panel_about_menu },
	{ GNOME_APP_UI_ENDOFINFO, 0, 0 }
};

GtkCheckMenuItem *
gnome_toggle_snap (void)
{
	return GTK_CHECK_MENU_ITEM (gnome_panel_desktop_menu [1].widget);
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

WPanel *
create_container (Dlg_head *h, char *name)
{
	PanelContainer *container = g_new (PanelContainer, 1);
	WPanel     *panel;
	GtkWidget  *app, *vbox;

	container->splitted = 0;
	app = gnome_app_new ("gmc", name);
	gtk_window_set_wmclass (GTK_WINDOW (app), "gmc", "gmc");
	gtk_widget_set_usize (GTK_WIDGET (app), 500, 360);
	panel = panel_new (name);

	vbox = gtk_vbox_new (0, 0);
	gnome_app_set_contents (GNOME_APP (app), vbox);
	gnome_app_create_menus_with_data (GNOME_APP (app), gnome_panel_menu, panel);

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
	if (!containers){
		containers = g_list_alloc ();
		containers->data = container;
	} else  
		containers = g_list_append (containers, container);

	if (!current_panel_ptr){
		current_panel_ptr = container;
	} else if (!other_panel_ptr)
		other_panel_ptr = container;

	bind_gtk_keys (GTK_WIDGET (app), h);
	return panel;
}

void
new_panel_at (char *dir)
{
	WPanel *panel;
	
	mc_chdir (dir);
	panel = create_container (desktop_dlg, dir);
	add_widget (desktop_dlg, panel);

	set_new_current_panel (panel);
}

void
setup_panels (void)
{
	load_hint ();
}


