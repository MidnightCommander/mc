#ifndef __TKWIDGET_H
#define __TKWIDGET_H

void tk_update_input (WInput *in);
void x_button_set (WButton *b, char *text);
int x_create_button (Dlg_head *h, widget_data parent, WButton *b);
void x_listbox_select_nth (WListbox *l, int nth);
void x_listbox_delete_nth (WListbox *l, int nth);
void x_label_set_text (WLabel *label, char *text);
int x_create_gauge (Dlg_head *h, widget_data parent, WGauge *g);
void x_gauge_show (WGauge *g);
void x_gauge_set_value (WGauge *g, int max, int current);

#endif /* __TKWIDGET_H */
