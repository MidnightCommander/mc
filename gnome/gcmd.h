#ifndef __GCMD_H
#define __GCMD_H

void gnome_listing_cmd        	  (GtkWidget *widget, WPanel *panel);
void gnome_compare_panels     	  (void);
void gnome_open_terminal      	  (void);
void gnome_open_terminal_with_cmd (const char *command);
void gnome_about_cmd              (void);
void gnome_open_panel             (GtkWidget *widget, WPanel *panel);
void gnome_close_panel            (GtkWidget *widget, WPanel *panel);

void gnome_icon_view_cmd          (GtkWidget *widget, WPanel *panel);
void gnome_brief_view_cmd         (GtkWidget *widget, WPanel *panel);
void gnome_detailed_view_cmd      (GtkWidget *widget, WPanel *panel);
void gnome_custom_view_cmd        (GtkWidget *widget, WPanel *panel);

void gnome_sort_cmd               (GtkWidget *widget, WPanel *panel);
void gnome_select_all_cmd         (GtkWidget *widget, WPanel *panel);
void gnome_filter_cmd             (GtkWidget *widget, WPanel *panel);
void gnome_external_panelize      (GtkWidget *widget, WPanel *panel);
void gnome_open_files             (GtkWidget *widget, WPanel *panel);
void gnome_run_new                (GtkWidget *widget, GnomeDesktopEntry *gde);
void gnome_mkdir_cmd              (GtkWidget *widget, WPanel *panel);
void gnome_new_launcher           (GtkWidget *widget, WPanel *panel);
void gnome_reverse_selection_cmd_panel (WPanel *panel);

#endif /* __GCMD_H */
