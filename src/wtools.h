#ifndef __WTOOLS_H
#define __WTOOLS_H

struct Dlg_head;
struct WListbox;

/* Listbox utility functions */
typedef struct {
    struct Dlg_head *dlg;
    struct WListbox *list;
} Listbox;

Listbox *create_listbox_window (int cols, int lines, char *title, char *help);
#define LISTBOX_APPEND_TEXT(l,h,t,d) \
    listbox_add_item (l->list, 0, h, t, d);

int run_listbox (Listbox *l);

/* Quick Widgets */
enum {
    quick_end, quick_checkbox, 
    quick_button, quick_input,
    quick_label, quick_radio
} /* quick_t */;

/* The widget is placed on relative_?/divisions_? of the parent widget */
/* Please note that the contents of the fields in the union are just */
/* used for setting up the dialog.  They are a convenient place to put */
/* the values for a widget */

typedef struct {
    int widget_type;
    int relative_x;
    int x_divisions;
    int relative_y;
    int y_divisions;

    char *text;			/* Text */
    int  hotkey_pos;		/* the hotkey position */
    int  value;			/* Buttons only: value of button */
    int  *result;		/* Checkbutton: where to store result */
    char **str_result;		/* Input lines: destination  */
    char *histname;		/* Name of the section for saving history */
} QuickWidget;
    
typedef struct {
    int  xlen, ylen;
    int  xpos, ypos; /* if -1, then center the dialog */
    char *title;
    char *help;
    QuickWidget *widgets;
    int  i18n;			/* If true, internationalization has happened */
} QuickDialog;

int quick_dialog (QuickDialog *qd);
int quick_dialog_skip (QuickDialog *qd, int nskip);

/* The input dialogs */

/* Pass this as def_text to request a password */
#define INPUT_PASSWORD ((char *) -1)

char *input_dialog (char *header, char *text, char *def_text);
char *input_dialog_help (char *header, char *text, char *help, char *def_text);
char *input_expand_dialog (char *header, char *text, char *def_text);

void query_set_sel (int new_sel);

struct Dlg_head *create_message (int flags, const char *title,
				 const char *text, ...)
    __attribute__ ((format (printf, 3, 4)));
void message (int flags, const char *title, const char *text, ...)
    __attribute__ ((format (printf, 3, 4)));

/* Use this as header for message() - it expands to "Error" */
#define MSG_ERROR ((char *) -1)

int query_dialog (const char *header, const char *text, int flags, int count, ...);

/* flags for message() and query_dialog() */
enum {
   D_NORMAL = 0,
   D_ERROR  = 1,
} /* dialog options */;

#endif	/* __WTOOLS_H */
