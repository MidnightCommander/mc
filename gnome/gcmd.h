#ifndef __GCMD_H
#define __GCMD_H

void gnome_listing_cmd    (GtkWidget *widget, WPanel *panel);
void gnome_compare_panels (void);
void gnome_open_terminal  (void);
void gnome_about_cmd      (void);
void gnome_quit_cmd       (void);
void gnome_open_panel     (GtkWidget *widget, WPanel *panel);
int  gnome_close_panel    (GtkWidget *widget, WPanel *panel);
void gnome_icon_view_cmd       (GtkWidget *widget, WPanel *panel);
void gnome_partial_view_cmd    (GtkWidget *widget, WPanel *panel);
void gnome_full_view_cmd       (GtkWidget *widget, WPanel *panel);
void gnome_custom_view_cmd     (GtkWidget *widget, WPanel *panel);
void gnome_sort_cmd       (GtkWidget *widget, WPanel *panel);
#endif /* __GCMD_H */
