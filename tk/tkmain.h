#ifndef __TKMAIN_H

#define __TKMAIN_H

#include <tcl.h>
#include <tk.h>
#include "dlg.h"
#include "widget.h"

/* Tcl interpreter */
extern Tcl_Interp *interp;

int xtoolkit_init (int *argc, char *argv []);
int xtoolkit_end (void);
int tkrundlg_event (Dlg_head *h);

/* Required by the standard code */
widget_data xtoolkit_create_dialog (Dlg_head *h, int with_grid);
widget_data xtoolkit_get_main_dialog (Dlg_head *h);
widget_data x_create_panel_container (int which);
void x_panel_container_show (widget_data wdata);

void x_destroy_cmd (void *);

int x_create_radio (Dlg_head *h, widget_data parent, WRadio *r);
int x_create_check (Dlg_head *h, widget_data parent, WCheck *c);
int x_create_label (Dlg_head *h, widget_data parent, WLabel *l);
int x_create_input (Dlg_head *h, widget_data parent, WInput *in);
int x_create_listbox (Dlg_head *h, widget_data parent, WListbox *l);
int x_create_buttonbar (Dlg_head *h, widget_data parent, WButtonBar *bb);
void x_list_insert (WListbox *l, WLEntry *p, WLEntry *e);
void x_redefine_label (WButtonBar *bb, int idx);

/* Configuration parameters */
extern int use_one_window;

typedef int (*tcl_callback)(ClientData, Tcl_Interp *, int, char *[]);
char * tk_new_command (widget_data parent, void *widget,
		       tcl_callback callback, char id);

#define STREQ(a,b) (*a == *b && (strcmp (a,b) == 0))

/* Widget value extraction */

/* Return the widget name */
static inline char *wtcl_cmd (Widget w)
{
    return (char *) w.wdata;
}

static inline char *wtk_win (Widget w)
{
    return (char *)(w.wdata)+1;
}

/* tkkey.c */
int lookup_keysym (char *s);

/* tkmain.c */
void tk_focus_widget (Widget_Item *p);
void tk_init_dlg (Dlg_head *h);
void tk_evalf (char *format,...);
void tk_dispatch_all (void);
void x_interactive_display (void);
int tk_getch (void);

#define PORT_HAS_DIALOG_TITLE      1
#define PORT_HAS_PANEL_UPDATE_COLS 1
#endif /* __TKMAIN_H */
