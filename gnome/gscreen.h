#ifndef __GSCREEN_H
#define __GSCREEN_H

void panel_action_view_unfiltered (GtkWidget *widget, WPanel *panel);
void panel_action_view (GtkWidget *widget, WPanel *panel);

WPanel *create_container (Dlg_head *h, char *str, char *geometry);
void panel_file_list_configure_contents (GtkWidget *file_list, WPanel *panel, int main_width, int height);

typedef struct {
	int splitted;
	
	WPanel *panel;
	
	enum view_modes other_display;
	Widget *other;
} PanelContainer;

extern PanelContainer *current_panel_ptr, *other_panel_ptr;
extern GList *containers;

#endif /* __GSCREEN_H */
