#include "x.h"
#include <stdio.h>
#include <sys/stat.h>
#include "dir.h"
#include "panel.h"
#include "gscreen.h"
#include "cmd.h"

typedef struct {
	int splitted;
	
	WPanel *panel;
	
	enum view_modes other_display;
	Widget *other;
} PanelContainer;

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
	return current_panel_ptr->panel;
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
	return -1;
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

	other_panel_ptr = current_panel_ptr;
	for (p = containers; p; p = p->next){
		if (((PanelContainer *)p->data)->panel == panel){
			printf ("Setting current panel to %p\n", p);
			current_panel_ptr = p->data;
		}
	}
}

void
print_vfs_message (char *msg)
{
	
}

void
rotate_dash (void)
{
}

int
get_current_type (void)
{
	return current_panel_ptr->panel->list_type;
}


int
get_other_type (void)
{
    /* FIXME: This is returning CURRENT panel */
    return view_nothing;
}

int
get_display_type (int index)
{
	GList *p;

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

void
gnome_listing_cmd (void)
{
	int   view_type, use_msformat;
	char  *user, *status;
	WPanel *p;
	int   index = 0;
	
	fprintf (stderr, "FIXME: index is hardcoded to 0 now\n");

	p = (WPanel *)get_panel_widget (index);
	
	view_type = display_box (p, &user, &status, &use_msformat, index);
	
	if (view_type == -1)
		return;

}

void configure_box (void);
	
GnomeUIInfo gnome_panel_filemenu [] = {
	{ GNOME_APP_UI_ITEM, "Network link...", NULL, netlink_cmd },
	{ GNOME_APP_UI_ITEM, "FTP link...",     NULL, ftplink_cmd },
	{ GNOME_APP_UI_ITEM, "Display mode...", NULL, gnome_listing_cmd },
	{ GNOME_APP_UI_ITEM, "Sort order...",   NULL, NULL },
	{ GNOME_APP_UI_ITEM, "Filter...",       NULL, NULL },
	{ GNOME_APP_UI_ITEM, "Rescan",          NULL, NULL },
	{ GNOME_APP_UI_ITEM, "Find",            NULL, find_cmd },
	{ GNOME_APP_UI_ITEM, "Hotlist",         NULL, quick_chdir_cmd },
#ifdef USE_VFS
	{ GNOME_APP_UI_ITEM, "Active VFS",      NULL, reselect_vfs },
#endif
	{ GNOME_APP_UI_ITEM, "Options",         NULL, configure_box },
	{ GNOME_APP_UI_ENDOFINFO, 0, 0 }
};

GnomeUIInfo gnome_panel_menu [] = {
	{ GNOME_APP_UI_SUBTREE, "File", NULL, &gnome_panel_filemenu },
	{ GNOME_APP_UI_ENDOFINFO, 0, 0 }
};

void
gnome_init_panels ()
{
	current_panel_ptr = NULL;
	other_panel_ptr = NULL;
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
	int slot;

	container->splitted = 0;
	app = gnome_app_new ("gmc", name);
	gtk_widget_set_usize (GTK_WIDGET (app), 400, 200);

	vbox = gtk_vbox_new (0, 0);
	gtk_widget_show (vbox);
	gnome_app_set_contents (GNOME_APP (app), vbox);
	gnome_app_create_menus (GNOME_APP (app), gnome_panel_menu);
	gtk_widget_show (app);

	panel = panel_new (name);

	gtk_signal_connect (GTK_OBJECT (app),
			    "enter_notify_event",
			    GTK_SIGNAL_FUNC (panel_enter_event),
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
	Dlg_head *h = current_panel_ptr->panel->widget.parent;
	
	mc_chdir (dir);
	panel = create_container (h, "Other");
	add_widget (h, panel);

	set_new_current_panel (panel);
}

void
setup_panels (void)
{
	load_hint ();
}

void
set_hintbar (char *str)
{
	g_panel_contents *gp;

	gp = (g_panel_contents *) current_panel_ptr->panel->widget.wdata;
	gtk_label_set (GTK_LABEL (gp->status), str);
}

