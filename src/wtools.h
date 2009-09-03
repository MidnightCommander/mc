
/** \file wtools.h
 *  \brief Header: widget based utility functions
 */

#ifndef MC_WTOOLS_H
#define MC_WTOOLS_H

#include "global.h"
#include "dialog.h"
#include "widget.h"

typedef struct {
    struct Dlg_head *dlg;
    struct WListbox *list;
} Listbox;

/* Listbox utility functions */
Listbox *create_listbox_window_delta (int delta_x, int delta_y, int cols, int lines, const char *title, const char *help);

Listbox *create_listbox_window (int cols, int lines, const char *title, const char *help);
#define LISTBOX_APPEND_TEXT(l,h,t,d) \
    listbox_add_item (l->list, LISTBOX_APPEND_AT_END, h, t, d)

int run_listbox (Listbox *l);

/* Quick Widgets */
typedef enum {
    quick_end		= 0,
    quick_checkbox	= 1,
    quick_button	= 2,
    quick_input		= 3,
    quick_label		= 4,
    quick_radio		= 5
} quick_t;

/* The widget is placed on relative_?/divisions_? of the parent widget */
typedef struct {
    quick_t widget_type;

    int relative_x;
    int x_divisions;
    int relative_y;
    int y_divisions;

    Widget *widget;

    /* widget parameters */
    union {
	struct {
	    const char *text;
	    int *state;		/* in/out */
	} checkbox;

	struct {
	    const char *text;
	    int action;
	    bcback callback;
	} button;

	struct {
	    const char *text;
	    int len;
	    int flags;			/* 1 -- is_password, 2 -- INPUT_COMPLETE_CD */
	    const char *histname;
	    char **result;
	} input;

	struct {
	    const char *text;
	} label;

	struct {
	    int count;
	    const char **items;
	    int *value;			/* in/out */
	} radio;
    } u;
} QuickWidget;

#define QUICK_CHECKBOX(x, xdiv, y, ydiv, txt, st)			\
{									\
    .widget_type = quick_checkbox,					\
    .relative_x = x,							\
    .x_divisions = xdiv,						\
    .relative_y = y,							\
    .y_divisions = ydiv,						\
    .widget = NULL,							\
    .u.checkbox.text = txt,						\
    .u.checkbox.state = st						\
}

#define QUICK_BUTTON(x, xdiv, y, ydiv, txt, act, cb)			\
{									\
    .widget_type = quick_button,					\
    .relative_x = x,							\
    .x_divisions = xdiv,						\
    .relative_y = y,							\
    .y_divisions = ydiv,						\
    .widget = NULL,							\
    .u.button.text = txt,						\
    .u.button.action = act,						\
    .u.button.callback = cb						\
}

#define QUICK_INPUT(x, xdiv, y, ydiv, txt, len_, flags_, hname, res)	\
{									\
    .widget_type = quick_input,						\
    .relative_x = x,							\
    .x_divisions = xdiv,						\
    .relative_y = y,							\
    .y_divisions = ydiv,						\
    .widget = NULL,							\
    .u.input.text = txt,						\
    .u.input.len = len_,						\
    .u.input.flags = flags_,						\
    .u.input.histname = hname,						\
    .u.input.result = res						\
}

#define QUICK_LABEL(x, xdiv, y, ydiv, txt)				\
{									\
    .widget_type = quick_label,						\
    .relative_x = x,							\
    .x_divisions = xdiv,						\
    .relative_y = y,							\
    .y_divisions = ydiv,						\
    .widget = NULL,							\
    .u.label.text = txt							\
}

#define QUICK_RADIO(x, xdiv, y, ydiv, cnt, items_, val)			\
{									\
    .widget_type = quick_radio,						\
    .relative_x = x,							\
    .x_divisions = xdiv,						\
    .relative_y = y,							\
    .y_divisions = ydiv,						\
    .widget = NULL,							\
    .u.radio.count = cnt,						\
    .u.radio.items = items_,						\
    .u.radio.value = val,						\
}

#define QUICK_END							\
{									\
    .widget_type = quick_end,						\
    .relative_x = 0,							\
    .x_divisions = 0,							\
    .relative_y = 0,							\
    .y_divisions = 0,							\
    .widget = NULL,							\
    .u.input.text = NULL,						\
    .u.input.len = 0,							\
    .u.input.flags = 0,							\
    .u.input.histname = NULL,						\
    .u.input.result = NULL						\
}

typedef struct {
    int  xlen, ylen;
    int  xpos, ypos; /* if -1, then center the dialog */
    const char *title;
    const char *help;
    QuickWidget *widgets;
    gboolean i18n;			/* If true, internationalization has happened */
} QuickDialog;

int quick_dialog (QuickDialog *qd);
int quick_dialog_skip (QuickDialog *qd, int nskip);

/* The input dialogs */

/* Pass this as def_text to request a password */
#define INPUT_PASSWORD ((char *) -1)

char *input_dialog (const char *header, const char *text,
		    const char *history_name, const char *def_text);
char *input_dialog_help (const char *header, const char *text, const char *help,
			 const char *history_name, const char *def_text);
char *input_expand_dialog (const char *header, const char *text,
			   const char *history_name, const char *def_text);

void query_set_sel (int new_sel);

/* Create message box but don't dismiss it yet, not background safe */
struct Dlg_head *create_message (int flags, const char *title,
				 const char *text, ...)
    __attribute__ ((format (__printf__, 3, 4)));

/* Show message box, background safe */
void message (int flags, const char *title, const char *text, ...)
    __attribute__ ((format (__printf__, 3, 4)));


/* Use this as header for message() - it expands to "Error" */
#define MSG_ERROR ((char *) -1)

int query_dialog (const char *header, const char *text, int flags, int count, ...);

/* flags for message() and query_dialog() */
enum {
   D_NORMAL = 0,
   D_ERROR  = 1
} /* dialog options */;

#endif
