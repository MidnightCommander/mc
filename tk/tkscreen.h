#ifndef __TKSCREEN_H
#define __TKSCREEN_H

#include "panel.h"      /* WPanel */

void x_panel_set_size (int index);
void x_create_panel (Dlg_head *h, widget_data parent, WPanel *panel);
void x_adjust_top_file (WPanel *panel);
void x_filter_changed (WPanel *panel);
void x_add_sort_label (WPanel *panel, int index, char *text, char *tag, void *sr);
void x_sort_label_start (WPanel *panel);
void x_reset_sort_labels (WPanel *panel);

#endif /* __TKSCREEN_H */
