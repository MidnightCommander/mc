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
#include <unistd.h>
#include "global.h"
#include "dir.h"
#include "panel.h"
#include "gscreen.h"
#include "main.h"
#include "gmain.h"
#include "cmd.h"
#include "dialog.h"
#include "boxes.h"
#include "panelize.h"
#include "gcmd.h"
#include "gcliplabel.h"
#include "gdesktop.h"
#include "setup.h"
#include "../vfs/vfs.h"
#include "gprefs.h"
#include "gsession.h"
#include "listing-iconic.xpm"
#include "listing-brief-list.xpm"
#include "listing-list.xpm"
#include "listing-custom.xpm"


#define UNDEFINED_INDEX -1

/* Keep these two arrays in sync! */

GnomeUIInfo panel_view_menu_uiinfo[] = {
	GNOMEUIINFO_RADIOITEM (N_("_Icon View"),
			       N_("Switch view to an icon display"),
			       gnome_icon_view_cmd, NULL),
	GNOMEUIINFO_RADIOITEM (N_("_Brief View"),
			       N_("Switch view to show just file name and type"),
			       gnome_brief_view_cmd, NULL),
	GNOMEUIINFO_RADIOITEM (N_("_Detailed View"),
			       N_("Switch view to show detailed file statistics"),
			       gnome_detailed_view_cmd, NULL),
	GNOMEUIINFO_RADIOITEM (N_("_Custom View"),
			       N_("Switch view to show user-defined statistics"),
			       gnome_custom_view_cmd, NULL),
	GNOMEUIINFO_END
};

GnomeUIInfo panel_view_toolbar_uiinfo[] = {
	GNOMEUIINFO_RADIOITEM (N_("Icons"),
			       N_("Switch view to an icon display"),
			       gnome_icon_view_cmd, listing_iconic_xpm),
	GNOMEUIINFO_RADIOITEM (N_("Brief"),
			       N_("Switch view to show just file name and type"),
			       gnome_brief_view_cmd, listing_brief_list_xpm),
	GNOMEUIINFO_RADIOITEM (N_("Detailed"),
			       N_("Switch view to show detailed file statistics"),
			       gnome_detailed_view_cmd, listing_list_xpm),
	GNOMEUIINFO_RADIOITEM (N_("Custom"),
			       N_("Switch view to show user-defined statistics"),
			       gnome_custom_view_cmd, listing_custom_xpm),
	GNOMEUIINFO_END
};

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
set_current_panel (WPanel *panel)
{
	GList *p;

	if (g_list_length (containers) > 1)
		other_panel_ptr = current_panel_ptr;

	for (p = containers; p; p = p->next){
		if (((PanelContainer *)p->data)->panel == panel){
			current_panel_ptr = p->data;
			break;
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
/*	x_flush_events (); */
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

		if (!is_a_desktop_panel (pc->panel))
			panel_save_setup (pc->panel, pc->panel->panel_name);
	}
}
static void
run_cmd (void)
{
	char *cmd;

	cmd = input_dialog (_("Enter command to run"), _("Enter command to run"), "");
	if (cmd && *cmd){
		my_system (EXECUTE_AS_SHELL, shell, cmd);
	}
}

static void
gnome_exit (void)
{
	GtkWidget *w;
	int v;
	
	w = gnome_message_box_new (
		_("Notice that if you choose to terminate the file manager, you will\n"
		  "also terminate the GNOME desktop handler.\n\n"
		  "Are you sure you want to exit?"),
		GNOME_MESSAGE_BOX_WARNING,
		GNOME_STOCK_BUTTON_YES,
		GNOME_STOCK_BUTTON_NO,
		NULL);
	v = gnome_dialog_run (GNOME_DIALOG (w));
	if (v != 0)
		return;

	w = gnome_message_box_new (
		_("The file manager and the desktop handler are now terminating\n\n"
		   "If you want to start up again the desktop handler or the file manager\n"
		   "you can launch it from the Panel, or you can run the UNIX command `gmc'\n\n"
		   "Press OK to terminate the application, or cancel to continue using it."),
		GNOME_MESSAGE_BOX_INFO,
		GNOME_STOCK_BUTTON_OK,
		GNOME_STOCK_BUTTON_CANCEL,
		NULL);
	v = gnome_dialog_run (GNOME_DIALOG (w));
	if (v == 0) {
		/*
		 * We do not want to be restarted by the session manager now
		 */
		session_set_restart (FALSE);
		gmc_do_quit ();
	}
}

static void
do_rescan_desktop (void)
{
	desktop_reload_icons (FALSE, 0, 0);
}

static void
gnome_launch_mime_editor (void)
{
	my_system (EXECUTE_AS_SHELL, shell, "mime-type-capplet");
}

void configure_box (void);

GtkCheckMenuItem *gnome_toggle_snap (void);

static GnomeUIInfo gnome_panel_new_menu [] = {
	GNOMEUIINFO_ITEM_NONE(N_("_Terminal"),
			      N_("Launch a new terminal in the current directory"), gnome_open_terminal),
	/* If this ever changes, make sure you update create_new_menu accordingly. */
	GNOMEUIINFO_ITEM_NONE(N_("_Directory..."),
			      N_("Creates a new directory"), gnome_mkdir_cmd),
	GNOMEUIINFO_ITEM_NONE(N_("_File..."),
			      N_("Creates a new file in this directory"), gnome_newfile_cmd),
	GNOMEUIINFO_END
};


GnomeUIInfo gnome_panel_file_menu [] = {
        GNOMEUIINFO_MENU_NEW_WINDOW_ITEM(gnome_open_panel, NULL),
	/*GNOMEUIINFO_MENU_NEW_ITEM(N_("New _Window"), N_("Opens a new window"), gnome_open_panel, NULL),*/
	
	/* We want to make a new menu entry here... */
	/* For example: */
	/* New-> */
	/*  Command Prompt */
	/*  Gimp Image */
	/*  Gnumeric Spreadsheet */
	/*  Text Document */
	/*  etc... */
	GNOMEUIINFO_MENU_NEW_SUBTREE(gnome_panel_new_menu),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_OPEN_ITEM(gnome_open_files, NULL),
/*	GNOMEUIINFO_ITEM_NONE(N_("Open _FTP site"),  N_("Opens an FTP site"), ftplink_cmd },*/
	GNOMEUIINFO_ITEM_STOCK(N_("_Copy..."), N_("Copy files"), copy_cmd, GNOME_STOCK_PIXMAP_COPY),
	GNOMEUIINFO_ITEM_STOCK(N_("_Delete..."), N_("Delete files"), delete_cmd, GNOME_STOCK_PIXMAP_TRASH),
        GNOMEUIINFO_ITEM_NONE(N_("_Move..."), N_("Rename or move files"), ren_cmd),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE(N_("Show directory sizes"), N_("Shows the disk space used by each directory"), dirsizes_cmd),
	GNOMEUIINFO_SEPARATOR,
	{ GNOME_APP_UI_ITEM, N_("Close window"), N_("Closes this window"),
	  gnome_close_panel, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_CLOSE,
	  'w', GDK_CONTROL_MASK, NULL },
        GNOMEUIINFO_END
};

GnomeUIInfo gnome_panel_edit_menu [] = {
	{ GNOME_APP_UI_ITEM,  N_("Select _All"),        N_("Select all files in the current Panel"), gnome_select_all_cmd,
	  NULL, NULL, 0, NULL, 'a', GDK_CONTROL_MASK  },
	GNOMEUIINFO_ITEM_NONE(N_("_Select Files..."),  N_("Select a group of files"), gnome_select),
	GNOMEUIINFO_ITEM_NONE(N_("_Invert Selection"), N_("Reverses the list of tagged files"),
			      gnome_reverse_selection_cmd_panel),
	GNOMEUIINFO_SEPARATOR,
	{ GNOME_APP_UI_ITEM, N_("Search"),		N_("Search for a file in the current Panel"), gnome_start_search,
	  NULL, NULL, 0, NULL, 's', GDK_CONTROL_MASK  },
	GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_ITEM_NONE(N_("_Rescan Directory"), N_("Rescan the directory contents"), reread_cmd),
	GNOMEUIINFO_END
};

GnomeUIInfo gnome_panel_settings_menu [] = {
	GNOMEUIINFO_MENU_PREFERENCES_ITEM(gnome_configure_box, NULL),
	GNOMEUIINFO_END
};

GnomeUIInfo gnome_panel_layout_menu [] = {
	GNOMEUIINFO_ITEM_NONE(N_("_Sort By..."),     N_("Confirmation settings"), gnome_sort_cmd),
	GNOMEUIINFO_ITEM_NONE(N_("_Filter View..."),    N_("Global option settings"), gnome_filter_cmd),
	GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_RADIOLIST(panel_view_menu_uiinfo),
	GNOMEUIINFO_END
};

GnomeUIInfo gnome_panel_commands_menu [] = {
        GNOMEUIINFO_ITEM_STOCK(N_("_Find File..."), N_("Locate files on disk"), find_cmd, GNOME_STOCK_MENU_JUMP_TO),
	  
/*	{ GNOME_APP_UI_ITEM, N_("_Compare panels..."), N_("Compare two panel contents"), gnome_compare_panels },*/
	GNOMEUIINFO_ITEM_NONE(N_("_Edit mime types..."), N_("Edits the MIME type bindings"),
			      gnome_launch_mime_editor),
	{ GNOME_APP_UI_ITEM, N_("_Run Command..."),               N_("Runs a command"), run_cmd, NULL,
	  NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN, GDK_F2, 0 },
	GNOMEUIINFO_ITEM_NONE(N_("_Run Command in panel..."),N_("Run a command and put the results in a panel"), gnome_external_panelize),

#ifdef USE_VFS					  
/*	GNOMEUIINFO_ITEM_NONE(N_("_Active VFS list..."),N_("List of active virtual file systems"), reselect_vfs), */
#endif
#ifdef USE_EXT2FSLIB
	/*does this do anything?*/
/*	 GNOMEUIINFO_ITEM_NONE(N_("_Undelete files (ext2fs only)..."), N_("Recover deleted files"), undelete_cmd),*/
#endif
#ifdef WITH_BACKGROUND
	GNOMEUIINFO_ITEM_NONE(N_("_Background jobs..."),   N_("List of background operations"), jobs_cmd),
#endif
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK (N_("Exit"), N_("Terminates the file manager and the desktop"),
				gnome_exit, GNOME_STOCK_PIXMAP_QUIT),
        GNOMEUIINFO_END
};


GnomeUIInfo gnome_panel_about_menu [] = {
	GNOMEUIINFO_MENU_ABOUT_ITEM(gnome_about_cmd, NULL),
	GNOMEUIINFO_HELP ("gmc"),
	GNOMEUIINFO_END
};

GnomeUIInfo gnome_panel_desktop_menu [] = {
	GNOMEUIINFO_SUBTREE(N_("_Arrange Icons"), desktop_arrange_icons_items),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("Rescan _Desktop Directory"), NULL, do_rescan_desktop),
	GNOMEUIINFO_ITEM_NONE (N_("Rescan De_vices"), NULL, desktop_rescan_devices),
	GNOMEUIINFO_ITEM_NONE (N_("Recreate Default _Icons"), NULL, desktop_recreate_default_icons),
	GNOMEUIINFO_END
};

GnomeUIInfo gnome_panel_menu_with_desktop [] = {
        GNOMEUIINFO_MENU_FILE_TREE(gnome_panel_file_menu),
        GNOMEUIINFO_MENU_EDIT_TREE(gnome_panel_edit_menu),
	GNOMEUIINFO_SUBTREE(N_("_Settings"),gnome_panel_settings_menu),
        GNOMEUIINFO_SUBTREE(N_("_Layout"),gnome_panel_layout_menu),
        GNOMEUIINFO_SUBTREE(N_("_Commands"),gnome_panel_commands_menu),
	GNOMEUIINFO_SUBTREE(N_("_Desktop"), gnome_panel_desktop_menu),
	GNOMEUIINFO_SUBTREE(N_("_Help"), gnome_panel_about_menu),
        GNOMEUIINFO_END
};

GnomeUIInfo gnome_panel_menu_without_desktop [] = {
        GNOMEUIINFO_MENU_FILE_TREE(gnome_panel_file_menu),
        GNOMEUIINFO_MENU_EDIT_TREE(gnome_panel_edit_menu),
	GNOMEUIINFO_SUBTREE(N_("_Settings"),gnome_panel_settings_menu),
        GNOMEUIINFO_SUBTREE(N_("_Layout"),gnome_panel_layout_menu),
        GNOMEUIINFO_SUBTREE(N_("_Commands"),gnome_panel_commands_menu),
	GNOMEUIINFO_SUBTREE(N_("_Help"), gnome_panel_about_menu),
        GNOMEUIINFO_END
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

static int
gnome_close_panel_event (GtkWidget *widget, GdkEvent *event, WPanel *panel)
{
	gnome_close_panel (widget, panel);
	return TRUE;
}

static void
panel_enter_event (GtkWidget *widget, GdkEvent *event, WPanel *panel)
{
	/* Avoid unnecessary code execution */
	if (get_current_panel () == panel)
		return;
	
	set_current_panel (panel);
	dlg_select_widget (panel->widget.parent, panel);
	send_message (panel->widget.parent, (Widget *) panel, WIDGET_FOCUS, 0);
}

void
destroy_gde (GtkWidget *unused, void *data)
{
	gnome_desktop_entry_free ((GnomeDesktopEntry *) (data));
}

gint
create_new_menu_from (char *file, GtkWidget *shell, gint pos)
{
	DIR *dir;
	struct stat filedata;
	gboolean add_separator = TRUE;
	struct dirent *dirstruc;
	GnomeDesktopEntry *gde;
	GtkWidget *menu;
	char *file2;

	g_return_val_if_fail (shell != NULL, pos);

	dir = opendir (file);
	if (dir == NULL)
		return pos;

	if (shell == NULL){
		closedir (dir);
		return pos;
	}

	while ((dirstruc = readdir (dir)) != NULL){
		if (dirstruc->d_name[0] == '.')
			continue;

		file2 = g_concat_dir_and_file (file, dirstruc->d_name);

		if ((stat (file2, &filedata) != -1) && (S_ISREG (filedata.st_mode))){
			char *path;
			int len;
			const int desktoplen = sizeof (".desktop") - 1;

			len = strlen (dirstruc->d_name);
			if (strcmp (dirstruc->d_name + len - desktoplen, ".desktop") != 0) {
				g_free (file2);
				continue;
			}

			gde = gnome_desktop_entry_load (file2);
			if (gde == NULL) {
				g_free (file2);
				continue;
			}

			path = gnome_is_program_in_path (gde->tryexec);
			g_free (path);
			if (!path){
				gnome_desktop_entry_free (gde);
				g_free (file2);
				continue;
			}

			if (add_separator) {
				menu = gtk_menu_item_new ();
				gtk_widget_show (menu);
				gtk_menu_shell_insert (GTK_MENU_SHELL (shell), menu, pos++);
				add_separator = !add_separator;
			}

			menu = gtk_menu_item_new_with_label (gde->name);
			gtk_widget_show (menu);
			gtk_menu_shell_insert (GTK_MENU_SHELL (shell), menu, pos++);

			/* This is really bad, but it works. */
			/* FIXME: it doesn't work if we free the gde below. --
                         * need to do this right sometime -jrb
			 */
			if (gde->comment)
				gtk_object_set_data (GTK_OBJECT (menu),
						     "apphelper_statusbar_hint",
						     gde->comment);
			gtk_signal_connect (GTK_OBJECT (menu), "activate",
					    GTK_SIGNAL_FUNC (gnome_run_new),
					    gde);
			gtk_signal_connect (GTK_OBJECT (menu), "destroy",
					    GTK_SIGNAL_FUNC (destroy_gde),
					    gde);
		}
		g_free (file2);
	}
	closedir (dir);
	
	return pos;
}

/**
 * create_new_menu:
 *
 * Creates the child New menu items
 */
static void
create_new_menu (GnomeApp *app, WPanel *panel)
{
	gchar *file, *file2;
	gint pos;
	GtkWidget *shell;

	shell = gnome_app_find_menu_pos (app->menubar, _("File/New/Directory..."), &pos);

	file = gnome_unconditional_datadir_file ("mc/templates");
	pos = create_new_menu_from (file, shell, pos);

	file2 = gnome_datadir_file ("mc/templates");
	if (file2 != NULL){
		if (strcmp (file, file2) != 0)
			create_new_menu_from (file2, shell, pos);
	}

	g_free (file);
	g_free (file2);
}

/*
 * This routine is a menu relay.
 *
 * This is called before the actual command specified in the GnomeUIInfo
 * structure.  This allows me to select the panel (ie, set the global cpanel
 * variable to which this menu is bound).
 *
 * This is important, as we can have a menu tearoffed.  And the current hack
 * of setting cpanel on the focus-in event wont work.
 *
 */
static void
panel_menu_relay (GtkObject *object, WPanel *panel)
{
	void (*real_func)(GtkObject *object, WPanel *panel);
	
	real_func = gtk_object_get_user_data (object);
	set_current_panel (panel);
	(*real_func)(object, panel);
}

static void
my_menu_signal_connect (GnomeUIInfo *uiinfo, gchar *signal_name, 
			GnomeUIBuilderData *uibdata)
{
	gtk_object_set_user_data (GTK_OBJECT (uiinfo->widget), uiinfo->moreinfo);
	gtk_signal_connect (GTK_OBJECT (uiinfo->widget), 
			    signal_name, panel_menu_relay, uibdata->data ? 
			    uibdata->data : uiinfo->user_data);
}

static void
my_app_create_menus (GnomeApp *app, GnomeUIInfo *uiinfo, void *data)
{
	GnomeUIBuilderData uibdata;

	g_return_if_fail (app != NULL);
	g_return_if_fail (GNOME_IS_APP (app));
	g_return_if_fail (uiinfo != NULL);

	uibdata.connect_func = my_menu_signal_connect;
	uibdata.data = data;
	uibdata.is_interp = FALSE;
	uibdata.relay_func = NULL;
	uibdata.destroy_func = NULL;

	gnome_app_create_menus_custom (app, uiinfo, &uibdata);
}

/**
 * copy_uiinfo_widgets:
 * @uiinfo: A GnomeUIInfo array
 * 
 * Allocates an array of widgets and copies the widgets from the uiinfo array to
 * it.  The array will be NULL-terminated.
 *
 * Returns: The allocated array of widgets.
 **/
gpointer *
copy_uiinfo_widgets (GnomeUIInfo *uiinfo)
{
	gpointer *dest;
	int n;
	int i;

	g_return_val_if_fail (uiinfo != NULL, NULL);

	/* Count number of items */

	for (n = 0; uiinfo[n].type != GNOME_APP_UI_ENDOFINFO; n++);

	/* Copy the widgets */

	dest = g_new (gpointer, n + 1);

	for (i = 0; i < n; i++)
		dest[i] = uiinfo[i].widget;

	dest[i] = NULL;

	return dest;
}

WPanel *
create_container (Dlg_head *h, const char *name, const char *geometry)
{
	PanelContainer *container;
	WPanel *panel;
	GtkWidget *app, *vbox;
	int xpos, ypos, width, height;
	GnomeUIInfo *uiinfo;
	char buf[50];

	container = g_new (PanelContainer, 1);

	gnome_parse_geometry (geometry, &xpos, &ypos, &width, &height);

	container->splitted = 0;
	app = gnome_app_new ("gmc", name);

	/* Geometry configuration */
	if (width != -1 && height != -1)
		gtk_window_set_default_size (GTK_WINDOW (app), width, height);
	else
		gtk_window_set_default_size (GTK_WINDOW (app), 540, 360);

	if (xpos != -1 && ypos != -1)
		gtk_widget_set_uposition (GTK_WIDGET (app), xpos, ypos);

	panel = panel_new (name);

	/* Set the unique name for session management */

	sprintf (buf, "%d", panel->id);
	gtk_window_set_wmclass (GTK_WINDOW (app), "gmc", buf);

	/* Create the holder for the contents */

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);
	gnome_app_set_contents (GNOME_APP (app), vbox);

	if (desktop_wm_is_gnome_compliant == 1)
		uiinfo = gnome_panel_menu_without_desktop;
	else
		uiinfo = gnome_panel_menu_with_desktop;

	my_app_create_menus (GNOME_APP (app), uiinfo, panel);
	panel->view_menu_items = copy_uiinfo_widgets (panel_view_menu_uiinfo);

	create_new_menu (GNOME_APP (app), panel);

	panel->ministatus = GNOME_APPBAR(gnome_appbar_new(FALSE, TRUE, GNOME_PREFERENCES_NEVER));
	gnome_app_set_statusbar(GNOME_APP (app), GTK_WIDGET(panel->ministatus));

	if (desktop_wm_is_gnome_compliant)
		gnome_app_install_menu_hints (GNOME_APP (app), gnome_panel_menu_without_desktop);
	else
		gnome_app_install_menu_hints (GNOME_APP (app), gnome_panel_menu_with_desktop);

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
new_panel_with_geometry_at (const char *dir, const char *geometry)
{
	WPanel *panel;

	mc_chdir ((char *) dir);
	panel = create_container (desktop_dlg, dir, geometry);
	set_current_panel (panel);
	add_widget (desktop_dlg, panel);
#if 0
	x_flush_events ();
#endif

	return panel;
}

WPanel *
new_panel_at (const char *dir)
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
	
	if (!is_a_desktop_panel (cpanel))
		update_one_panel_widget (cpanel, force_update, current_file);

	if (reload_others){
		for (p = containers; p; p = p->next){
			PanelContainer *pc = p->data;
			
			if (p->data == current_panel_ptr)
				continue;
			
			if (!is_a_desktop_panel (pc->panel))
				update_one_panel_widget (pc->panel, force_update, UP_KEEPSEL);
		}
	}
	mc_chdir (cpanel->cwd);
}
