#ifndef __GMC_MAIN_H
#define __GMC_MAIN_H
#include "dlg.h"
#include "panel.h"
#include "widget.h"
#include "info.h"

int xtoolkit_init (int *argc, char *argv []);
void xtoolkit_end (void);

extern Dlg_head *desktop_dlg;
extern int nowindows;
extern int corba_have_server;

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
void bind_gtk_keys (GtkWidget *w, Dlg_head *h);
WPanel *new_panel_at (const char *dir);
WPanel *new_panel_with_geometry_at (const char *dir, const char *geometry);
void set_current_panel (WPanel *panel);
void layout_panel_gone (WPanel *panel);
void gtkrundlg_event (Dlg_head *h);
int gmc_open (file_entry *fe);
int gmc_open_with (char *filename);
int gmc_open_filename (char *fname, GList *args);
int gmc_edit (char *fname);
int gmc_can_view_file (char *filename);
int gmc_view (char *filename, int start_line);
void x_show_info (WInfo *info, struct my_statfs *s, struct stat *b);
void x_create_info (Dlg_head *h, widget_data parent, WInfo *info);
void gnome_check_super_user (void);
gint create_new_menu_from (char *file, GtkWidget *shell, gint pos);
int rename_file_with_context (char *source, char *dest);

/*
 * stuff from gaction.c
 */
int gmc_open          (file_entry *fe);
int gmc_open_filename (char *fname, GList *args);
int gmc_edit_filename (char *fname);
int gmc_view          (char *filename, int start_line);

void gmc_window_setup_from_panel (GnomeDialog *dialog, WPanel *panel);

struct gmc_color_pairs_s {
	GdkColor *fore, *back;
};

void gmc_do_quit (void);
#if 0
extern GnomeClient *session_client;
#endif

extern struct gmc_color_pairs_s gmc_color_pairs [];
#endif
