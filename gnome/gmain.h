#ifndef __GMC_MAIN_H
#define __GMC_MAIN_H
#include "dlg.h"
#include "panel.h"
#include "widget.h"

int xtoolkit_init (int *argc, char *argv []);
void xtoolkit_end (void);

extern Dlg_head *desktop_dlg;
extern int nowindows;

/* Required by the standard code */
widget_data xtoolkit_create_dialog   (Dlg_head *h, int with_grid);
widget_data xtoolkit_get_main_dialog (Dlg_head *h);
void x_dlg_set_window                (Dlg_head *h, GtkWidget *win);
widget_data x_create_panel_container (int which);
void x_panel_container_show          (widget_data wdata);
void x_destroy_cmd        	     (void *);
int  x_create_radio       	     (Dlg_head *h, widget_data parent, WRadio *r);
int  x_create_check       	     (Dlg_head *h, widget_data parent, WCheck *c);
int  x_create_label       	     (Dlg_head *h, widget_data parent, WLabel *l);
int  x_create_input       	     (Dlg_head *h, widget_data parent, WInput *in);
int  x_create_listbox     	     (Dlg_head *h, widget_data parent, WListbox *l);
int  x_create_buttonbar   	     (Dlg_head *h, widget_data parent, WButtonBar *bb);
void x_filter_changed   	     (WPanel *panel);
void x_list_insert      	     (WListbox *l, WLEntry *p, WLEntry *e);
void x_redefine_label   	     (WButtonBar *bb, int idx);
void x_add_widget       	     (Dlg_head *h, Widget_Item *w);
int  translate_gdk_keysym_to_curses (GdkEventKey *event);
void gnome_init_panels ();
void set_current_panel (int index);
void bind_gtk_keys (GtkWidget *w, Dlg_head *h);
WPanel *new_panel_at (char *dir);
WPanel *new_panel_with_geometry_at (char *dir, char *geometry);
void layout_panel_gone (WPanel *panel);

struct gmc_color_pairs_s {
	GdkColor *fore, *back;
};

extern struct gmc_color_pairs_s gmc_color_pairs [];
#endif
