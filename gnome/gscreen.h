#ifndef __GSCREEN_H
#define __GSCREEN_H


/* GnomeUIInfo information for view types */
extern GnomeUIInfo panel_view_menu_uiinfo[];
extern GnomeUIInfo panel_view_toolbar_uiinfo[];

WPanel *create_container (Dlg_head *h, char *str, char *geometry);

void copy_uiinfo_widgets (GnomeUIInfo *uiinfo, gpointer **dest);

typedef struct {
	int splitted;
	
	WPanel *panel;
	
	enum view_modes other_display;
	Widget *other;
} PanelContainer;

extern PanelContainer *current_panel_ptr, *other_panel_ptr;
extern GList *containers;


#endif /* __GSCREEN_H */
