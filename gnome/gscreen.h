#ifndef __GSCREEN_H
#define __GSCREEN_H

void x_panel_set_size (int index);
void x_create_panel (Dlg_head *h, widget_data parent, WPanel *panel);
void x_adjust_top_file (WPanel *panel);
void x_filter_changed (WPanel *panel);
void x_add_sort_label (WPanel *panel, int index, char *text, char *tag, void *sr);
void x_sort_label_start (WPanel *panel);
void x_reset_sort_labels (WPanel *panel);

void panel_action_view_unfiltered (GtkWidget *widget, WPanel *panel);
void panel_action_view (GtkWidget *widget, WPanel *panel);

WPanel *create_container (Dlg_head *h, char *str);
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
