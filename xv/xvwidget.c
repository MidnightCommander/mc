/* Widgets for the Midnight Commander - on top of XView panel objects
   Copyright (C) 1995 Jakub Jelinek

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 */

#include <config.h>
#include <xview/xview.h>
#include <xview/panel.h>
#include "paneltext.h"

#include "dlg.h"
#include "widget.h"
#include "xvmain.h"

extern int quote;
extern Dlg_head *midnight_dlg;

void xv_widget_layout (Panel_item item, WLay layout);

static void xv_button_notify (Panel_item item, Event *event)
{
    WButton *b = (WButton *) xv_get (item, PANEL_CLIENT_DATA);
    Dlg_head *h = (Dlg_head *) xv_get (item, XV_KEY_DATA, KEY_DATA_DLG_HEAD);
    int stop = 0;
    
    if (b->callback)
        stop = (*b->callback) (b->action, b->callback_data);
    if (!b->callback || stop) {
        h->running = 0;
        h->ret_value = b->action;
    }
}

int x_create_button (Dlg_head *h, widget_data parent, WButton *b)
{
    Panel_button_item button;

    button = (Panel_button_item) xv_create (
        (Panel) x_get_parent_object ((Widget *) b, parent), PANEL_BUTTON,
        PANEL_LABEL_STRING, b->text,
        PANEL_NOTIFY_PROC, xv_button_notify,
        PANEL_CLIENT_DATA, b,
        XV_KEY_DATA, KEY_DATA_DLG_HEAD, h,
        NULL);
    b->widget.wdata = (widget_data) button;
    xv_widget_layout (button, b->widget.layout);
    return 1;
}

void x_button_set (WButton *b, char *text)
{
    if (b->widget.wdata != (widget_data)NULL)
        xv_set ((Panel_button_item) (b->widget.wdata),
            PANEL_LABEL_STRING, b->text,
            NULL);
}


/* Radio button widget */

void xv_radio_notify (Panel_item item, int value, Event* event)
{
    WRadio *r = (WRadio *) xv_get (item, PANEL_CLIENT_DATA);
    
    r->pos = r->sel = value;
}

int x_create_radio (Dlg_head *h, widget_data parent, WRadio *r)
{
    Panel_choice_item radio;
    int i;
    
    radio = (Panel_choice_item) xv_create (
        (Panel) x_get_parent_object ((Widget *) r, parent), PANEL_CHECK_BOX,
        PANEL_LAYOUT, PANEL_VERTICAL,
        PANEL_CHOOSE_ONE, TRUE,
        PANEL_NOTIFY_PROC, xv_radio_notify,
        PANEL_CLIENT_DATA, r,
        XV_KEY_DATA, KEY_DATA_DLG_HEAD, h,
        NULL);
    for (i = 0; i < r->count; i++)
        xv_set (radio,
            PANEL_CHOICE_STRING, i, r->texts [i],
            NULL);
    xv_set (radio,
        PANEL_VALUE, r->sel,
        NULL);
    r->widget.wdata = (widget_data) radio;
    xv_widget_layout (radio, r->widget.layout);
    return 1;
}


/* Checkbutton widget */

void xv_check_notify (Panel_item item, int value, Event* event)
{
    WCheck *c = (WCheck *) xv_get (item, PANEL_CLIENT_DATA);
    
    c->state = value;
}

int x_create_check (Dlg_head *h, widget_data parent, WCheck *c)
{
    Panel_choice_item check;
    
    check = (Panel_choice_item) xv_create (
        (Panel) x_get_parent_object ((Widget *) c, parent), PANEL_CHECK_BOX,
        PANEL_LAYOUT, PANEL_VERTICAL,
        PANEL_CHOOSE_ONE, FALSE,
        PANEL_NOTIFY_PROC, xv_check_notify,
        PANEL_CLIENT_DATA, c,
        PANEL_VALUE, c->state,
        PANEL_CHOICE_STRINGS,
            c->text,
            NULL,
        XV_KEY_DATA, KEY_DATA_DLG_HEAD, h,
        NULL);
    c->widget.wdata = (widget_data) check;
    xv_widget_layout (check, c->widget.layout);
    return 1;
}


/* Label widget */

int x_create_label (Dlg_head *h, widget_data parent, WLabel *l)
{
    Panel_message_item label;
    Panel panel = (Panel) x_get_parent_object ((Widget *) l, parent);
    
    label = (Panel_message_item) xv_create (
        panel, PANEL_MESSAGE,
        PANEL_VALUE_FONT, xv_get (panel, PANEL_BOLD_FONT),
        PANEL_LABEL_FONT, xv_get (panel, PANEL_BOLD_FONT),
        PANEL_LABEL_STRING, l->text,
        PANEL_LINE_BREAK_ACTION, PANEL_WRAP_AT_WORD,
        XV_KEY_DATA, KEY_DATA_DLG_HEAD, h,
        XV_KEY_DATA, KEY_DATA_TILE, panel,
        PANEL_CLIENT_DATA, l,
        NULL);
    l->widget.wdata = (widget_data) label;
    xv_widget_layout (label, l->widget.layout);
    return 1;
}

void x_label_set_text (WLabel *label, char *text)
{
    if (label->widget.wdata != (widget_data)NULL) {
        int x, y, w, wold, xi;
        Panel panel = (Panel) xv_get ((Panel_button_item) (label->widget.wdata),
            XV_KEY_DATA, KEY_DATA_TILE);
        
        wold = (int) xv_get ((Panel_button_item) (label->widget.wdata), XV_WIDTH);
        
        xv_set ((Panel_button_item) (label->widget.wdata),
            XV_SHOW, FALSE,
            NULL);
        
        xv_set ((Panel_button_item) (label->widget.wdata),
            PANEL_LABEL_STRING, label->text,
            NULL);
            
        w = (int) xv_get ((Panel_button_item) (label->widget.wdata), XV_WIDTH);
        x = (int) xv_get ((Panel_button_item) (label->widget.wdata), XV_X);
        y = (int) xv_get ((Panel_button_item) (label->widget.wdata), XV_Y);
        
        if (w != wold) {
            Panel_item item;
            
            for (item = (Panel_item) xv_get (panel, PANEL_FIRST_ITEM);
                item != XV_NULL; 
                item = (Panel_item) xv_get (item, PANEL_NEXT_ITEM)) {
                if (xv_get (item, PANEL_ITEM_OWNER))
                    continue;
                if ((int) xv_get (item, XV_Y) == y) {
                    xi = (int) xv_get (item, XV_X);
                    if (xi > x)
                        xv_set (item,
                            XV_X, xi + w - wold,
                            NULL);
                }
            }
        }
        xv_set ((Panel_button_item) (label->widget.wdata),
            XV_SHOW, TRUE,
            NULL);
    }
}



/* Gauge widget */

int x_create_gauge (Dlg_head *h, widget_data parent, WGauge *g)
{
    Panel_gauge_item gauge;
    Panel panel = (Panel) x_get_parent_object ((Widget *) g, parent);
    
    gauge = (Panel_gauge_item) xv_create (
        panel, PANEL_GAUGE,
        PANEL_DIRECTION, PANEL_HORIZONTAL,
        PANEL_MIN_VALUE, 0,
        PANEL_MAX_VALUE, g->max,
        PANEL_TICKS, 20,
        PANEL_SHOW_RANGE, TRUE,
        PANEL_VALUE, g->current,
        PANEL_GAUGE_WIDTH, 200,
        XV_KEY_DATA, KEY_DATA_DLG_HEAD, h,
        XV_SHOW, g->shown,
        PANEL_CLIENT_DATA, g,
        NULL);
    g->widget.wdata = (widget_data) gauge;
    xv_widget_layout (gauge, g->widget.layout);
    return 1;
}

void x_gauge_set_value (WGauge *g, int max, int current)
{
    if (g->widget.wdata != (widget_data)NULL) {
        if (max != g->max)
            xv_set ((Panel_gauge_item) (g->widget.wdata),
                PANEL_VALUE, current,
                PANEL_MAX_VALUE, max,
                NULL);
        else
            xv_set ((Panel_gauge_item) (g->widget.wdata),
                PANEL_VALUE, current,
                NULL);
    }
}

void x_gauge_show (WGauge *g)
{
    if (g->widget.wdata != (widget_data)NULL)
    	xv_set ((Panel_gauge_item) (g->widget.wdata),
    	    XV_SHOW, g->shown,
    	    NULL);
}


/* Input widget */

static Panel_setting xv_input_notify (Panel_item item, Event *event)
{
    Dlg_head *h = (Dlg_head *) xv_get (item, XV_KEY_DATA, KEY_DATA_DLG_HEAD);
    WInput *in = (WInput *) xv_get (item, PANEL_CLIENT_DATA);

    if (h != midnight_dlg) {
        switch (event_action (event)) {
            case ACTION_STOP:
                h->running = 0;
                h->ret_value = B_CANCEL;
               return PANEL_NONE;
            default: 
                switch (event_id (event)) {
                    case '\n':
                    case '\r':
                        if (! event_is_down (event))
                            return PANEL_NONE;
             	        h->running = 0;
             	        h->ret_value = B_ENTER;
                        return PANEL_NONE;
        	    case 27:
                        if (! event_is_down (event))
                            return PANEL_NONE;
             	        h->running = 0;
                        h->ret_value = B_CANCEL;
             	        return PANEL_NONE;
             	    default:
                        return PANEL_INSERT;
                }
        }
    } else { /* Command line is a special case */
        switch (event_id (event)) {
            case '\n':
            case '\r':
                if (! event_is_down (event))
                    return PANEL_NONE;
                send_message (midnight_dlg, (Widget *) in, WIDGET_KEY, '\n');
                return PANEL_NONE;
            default:
            	return PANEL_INSERT;
        }
    }
}

static void xv_input_postnotify (Panel_item item, Event *event, char *buffer,
    int caret_position)
{
    char *p;
    WInput *in = (WInput *) xv_get (item, PANEL_CLIENT_DATA);

    p = realloc (in->buffer, strlen (buffer) + in->field_len);
    if (p != (char *) NULL) {
        in->buffer = p;
        strcpy (p, buffer);
        in->current_max_len = strlen (buffer) + in->field_len;
    }
    in->point = caret_position; 
}

int x_create_input (Dlg_head *h, widget_data parent, WInput *in)
{
    Panel_text_item input;
    
    input = (Panel_text_item) xv_create (
        (Panel) x_get_parent_object ((Widget *) in, parent), PANELTEXT,
        PANEL_NOTIFY_LEVEL, PANEL_NON_PRINTABLE,
        PANEL_NOTIFY_PROC, xv_input_notify,
        PANEL_CLIENT_DATA, in,
        XV_KEY_DATA, KEY_DATA_DLG_HEAD, h,
        PANEL_VALUE, in->buffer,
        PANEL_VALUE_DISPLAY_LENGTH, in->widget.cols,
        PANEL_VALUE_STORED_LENGTH, 4096,
        PANELTEXT_NOTIFY, xv_input_postnotify,
        NULL);
    in->widget.wdata = (widget_data) input;
    xv_widget_layout (input, in->widget.layout);
    return 1;
}

void
x_update_input (WInput *in)
{
}


/* Listbox widget */

void x_listbox_select_nth (WListbox *l, int nth)
{
    if (l->widget.wdata != (widget_data) NULL)
	xv_set ((Panel_list_item) (l->widget.wdata),
            PANEL_LIST_SELECT, nth, TRUE,
            NULL);
}

void x_listbox_delete_nth (WListbox *l, int nth)
{
    xv_set ((Panel_list_item) (l->widget.wdata),
        PANEL_LIST_DELETE, l->pos,
        NULL);
}

static void xv_listbox_notify (Panel_item item, char *string, 
    caddr_t client_data, Panel_list_op op, Event *event)
{
    WListbox *l = (WListbox *) xv_get (item, PANEL_CLIENT_DATA);
    Dlg_head *h = (Dlg_head *) xv_get (item, XV_KEY_DATA, KEY_DATA_DLG_HEAD);

    if (op == PANEL_LIST_OP_SELECT || op == PANEL_LIST_OP_DBL_CLICK) {
        int i;
        WLEntry *e;
        
        l->current = (WLEntry *) client_data;
        for (i = 0, e = l->list; i < l->count; i++, e = e->next)
            if (e == l->current) {
                l->pos = i;
                break;
            }
    }
    if (op == PANEL_LIST_OP_DBL_CLICK) {
        h->running = 0;
        h->ret_value = B_ENTER;
    }
}

int x_create_listbox (Dlg_head *h, widget_data parent, WListbox *l)
{
    Panel_list_item listbox;
    WLEntry *e;
    int i;
    
    listbox = (Panel_list_item) xv_create (
        (Panel) x_get_parent_object ((Widget *) l, parent), PANEL_LIST,
        PANEL_NOTIFY_PROC, xv_listbox_notify,
        PANEL_CLIENT_DATA, l,
        XV_KEY_DATA, KEY_DATA_DLG_HEAD, h,
        PANEL_CHOOSE_ONE, TRUE,
        PANEL_LIST_DISPLAY_ROWS, l->widget.lines,
        PANEL_LIST_DO_DBL_CLICK, TRUE,
        NULL);
    l->widget.wdata = (widget_data) listbox;
    /* The user could add some entries even before this stuff gets displayed */
    e = l->list; i = 0;
    if (e != NULL)
        do {
            xv_set (listbox,
                PANEL_LIST_INSERT, i,
                PANEL_LIST_STRING, i, e->text,
                PANEL_LIST_CLIENT_DATA, i, e,
            NULL);
            if (e == l->current)
                x_listbox_select_nth (l, i);
            e = e->next;
            i++;
        } while (e != l->list);
    xv_widget_layout (listbox, l->widget.layout);
    return 1;
}

void x_list_insert (WListbox *l, WLEntry *p, WLEntry *e)
{
    int i;

    if (l->widget.wdata == (widget_data) NULL)
    	return;

    /* Jakub, shouldn't we have an i++ here? */
    for (i = 0; p != NULL && p != e; e = e->next, i++)
	;

    xv_set ((Panel_list_item) (l->widget.wdata),
	PANEL_LIST_INSERT, i,
        PANEL_LIST_STRING, i, e->text,
        PANEL_LIST_CLIENT_DATA, i, e,
        NULL);
}

static void xv_buttonbar_notify (Panel_item item, Event *event)
{
    WButtonBar *bb = (WButtonBar *) xv_get (item, PANEL_CLIENT_DATA);
    int idx = xv_get (item, XV_KEY_DATA, KEY_DATA_BUTTONBAR_IDX);
    
    if (bb->labels [idx - 1].function)
        xv_post_proc (midnight_dlg, *bb->labels [idx - 1].function, 
            bb->labels [idx - 1].data);
}

extern Menu MenuBar [];

static Menu xv_create_buttonbarmenu (void)
{
    return xv_create (XV_NULL, MENU,
        MENU_ITEM,
            MENU_STRING, "Panel",
            MENU_PULLRIGHT, MenuBar [0],
            NULL,
        MENU_ITEM,
            MENU_STRING, "File",
            MENU_PULLRIGHT, MenuBar [1],
            NULL,
        MENU_ITEM,
            MENU_STRING, "Command",
            MENU_PULLRIGHT, MenuBar [2],
            NULL,
        MENU_ITEM,
            MENU_STRING, "Options",
            MENU_PULLRIGHT, MenuBar [3],
            NULL,
        NULL);
}

int x_create_buttonbar (Dlg_head *h, widget_data parent, WButtonBar *bb)
{
    Panel_button_item button;
    Panel panel = (Panel) x_get_parent_object ((Widget *) bb, parent);
    int i;

    for (i = 1; i <= 10; i++) {
        button = xv_create (panel, PANEL_BUTTON,
            PANEL_LABEL_STRING, bb->labels [i - 1].text,
            PANEL_CLIENT_DATA, bb,
            XV_KEY_DATA, KEY_DATA_BUTTONBAR_IDX, i,
        NULL);
        if (bb->labels [i - 1].text && 
            !strcmp (bb->labels [i - 1].text, "PullDn")) {
            xv_set (button,
                PANEL_ITEM_MENU, xv_create_buttonbarmenu (),
                NULL);
        } else {
            xv_set (button,
                PANEL_NOTIFY_PROC, xv_buttonbar_notify,
                NULL);
        }
        if (i == 1)
            xv_set (button,
            	XV_X, 4,
            	NULL);
    }
    xv_set (panel, 
        XV_KEY_DATA, KEY_DATA_PANEL_CONTAINER, xv_get (panel, XV_OWNER),
        NULL);
    bb->widget.wdata = (widget_data) panel;
    return 1;
}

void x_redefine_label (WButtonBar *bb, int idx)
{
    Panel_item ip;
    int i, j, x;
    Panel_item items [10];
    Panel panel = (Panel) bb->widget.wdata;
    Menu menu;

    for (i = 0; i < 10; i++)
        items [i] = XV_NULL;
    if (panel != XV_NULL) {
        PANEL_EACH_ITEM (panel, ip)
            if ((WButtonBar *) xv_get (ip, PANEL_CLIENT_DATA) == bb) {
                i = xv_get (ip, XV_KEY_DATA, KEY_DATA_BUTTONBAR_IDX);
                if (i == idx) {
                    xv_set (ip,
                        PANEL_LABEL_STRING, bb->labels [idx - 1].text,
                        NULL);
                    if (bb->labels [idx - 1].text && 
                        !strcmp (bb->labels [idx - 1].text, "PullDn")) {
                        if (xv_get (ip, PANEL_ITEM_MENU) == XV_NULL)
                            xv_set (ip,
                                PANEL_NOTIFY_PROC, NULL,
                                PANEL_ITEM_MENU, xv_create_buttonbarmenu (),
                                NULL);
        	    } else {
        	        if ((menu = xv_get (ip, PANEL_ITEM_MENU)) != XV_NULL)
            		    xv_set (ip,
                                PANEL_NOTIFY_PROC, xv_buttonbar_notify,
                                PANEL_ITEM_MENU, XV_NULL,
                                NULL);
                            xv_destroy_safe (menu);
                    }
                }
                items [i - 1] = ip;
            }
        PANEL_END_EACH
    	j = xv_get (panel, PANEL_ITEM_X_GAP);
    	for (i = 0, x = 4; i < 10; i++) {
            xv_set (items [i],
            	XV_X, x,
            	NULL);
            x += j + xv_get (items [i], XV_WIDTH);
        }
    }
}

/* returns 1 if bb is in another panel container than paneletc (panel, tree,
 * info, view, etc. */
int x_find_buttonbar_check (WButtonBar *bb, Widget *paneletc)
{
    return (bb->widget.wcontainer != paneletc->wcontainer);
}

