#ifndef MC_WTOOLS_H
#define MC_WTOOLS_H

struct Dlg_head;
struct WListbox;

typedef struct {
    struct Dlg_head *dlg;
    struct WListbox *list;
} Listbox;

/* Listbox utility functions */
Listbox *create_listbox_window (int cols, int lines, const char *title, const char *help);
#define LISTBOX_APPEND_TEXT(l,h,t,d) \
    listbox_add_item (l->list, 0, h, t, d)

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

    const char *text;		/* Text */
    int  hotkey_pos;		/* the hotkey position */
    int  value;			/* Buttons only: value of button */
    int  *result;		/* Checkbutton: where to store result */
    char **str_result;		/* Input lines: destination  */
    const char *histname;	/* Name of the section for saving history */
} QuickWidget;
#define NULL_QuickWidget { 0, 0, 0, 0, 0, NULL, 0, 0, NULL, NULL, NULL }

typedef struct {
    int  xlen, ylen;
    int  xpos, ypos; /* if -1, then center the dialog */
    const char *title;
    const char *help;
    QuickWidget *widgets;
    int  i18n;			/* If true, internationalization has happened */
} QuickDialog;

int quick_dialog (QuickDialog *qd);
int quick_dialog_skip (QuickDialog *qd, int nskip);

/* The input dialogs */

/* Pass this as def_text to request a password */
#define INPUT_PASSWORD ((char *) -1)

char *input_dialog (const char *header, const char *text, const char *def_text);
char *input_dialog_help (const char *header, const char *text, const char *help, const char *def_text);
char *input_expand_dialog (const char *header, const char *text, const char *def_text);

void query_set_sel (int new_sel);

/* Create message box but don't dismiss it yet, not background safe */
struct Dlg_head *create_message (int flags, const char *title,
				 const char *text, ...)
    __attribute__ ((format (printf, 3, 4)));

/* Show message box, background safe */
void message (int flags, const char *title, const char *text, ...)
    __attribute__ ((format (printf, 3, 4)));


/* Use this as header for message() - it expands to "Error" */
#define MSG_ERROR ((char *) -1)

int query_dialog (const char *header, const char *text, int flags, int count, ...);

/* flags for message() and query_dialog() */
enum {
   D_NORMAL = 0,
   D_ERROR  = 1
} /* dialog options */;

#endif
