#ifndef __XVMAIN_H
#define __XVMAIN_H

#ifdef HAVE_XVIEW
#   undef HAVE_LIBGPM
    
#   define APP_DEFAULTS "/usr/X11/lib/X11/app-defaults/Mxc"

#   include "xvkeydata.h"
    int xtoolkit_init (int *, char **);
    int xtoolkit_end (void);
    void xv_action_icons (void);
    void xv_dispatch_a_bit (void);
    void xv_dispatch_something (void);
#   include "dlg.h"
#   include "widget.h"

    int xvrundlg_event (Dlg_head *h);
    void xv_post_proc (Dlg_head *h, void (*callback)(void *), void *);
    widget_data x_create_panel_container (int which);
    widget_data x_get_parent_object (Widget *w, widget_data wdata);
    int x_container_get_id (widget_data wcontainer);
    void x_panel_container_show (widget_data wdata);
    widget_data xtoolkit_create_dialog (Dlg_head *h, int with_grid);
    widget_data xtoolkit_get_main_dialog (Dlg_head *h);
    void xtoolkit_kill_dialog (Dlg_head *h); /* When we do not call run_dlg */

    /* Prototypes for functions required by src/widget.c */
    int xv_create_button (Dlg_head *h, widget_data parent, WButton *b);
    void x_button_set (WButton *b, char *text);
    int x_create_radio (Dlg_head *h, widget_data parent, WRadio *r);
    int x_create_check (Dlg_head *h, widget_data parent, WCheck *c);
    int x_create_label (Dlg_head *h, widget_data parent, WLabel *l);
    int x_create_input (Dlg_head *h, widget_data parent, WInput *in);
    int x_create_listbox (Dlg_head *h, widget_data parent, WListbox *l);
    int x_create_buttonbar (Dlg_head *h, widget_data parent, WButtonBar *bb);
    int x_create_gauge (Dlg_head *h, widget_data parent, WGauge *g);
    int x_find_buttonbar_check (WButtonBar *bb, Widget *paneletc);
    void x_list_insert (WListbox *l, WLEntry *p, WLEntry *e);
    void x_redefine_label (WButtonBar *bb, int idx);

#define PORT_HAS_DIALOG_TITLE 1

#endif /* HAVE_XVIEW */

#endif /* __XVMAIN_H */
