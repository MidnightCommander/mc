#ifndef __GSCREEN_H
#define __GSCREEN_H


WPanel *create_container (Dlg_head *h, char *str, char *geometry);

typedef struct {
	int splitted;
	
	WPanel *panel;
	
	enum view_modes other_display;
	Widget *other;
} PanelContainer;

extern PanelContainer *current_panel_ptr, *other_panel_ptr;
extern GList *containers;

#endif /* __GSCREEN_H */
