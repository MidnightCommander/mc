#ifndef __WIDGET_H
#define __WIDGET_H

#include "dlg.h"		/* Widget */

#define C_BOOL		1
#define C_CHANGE	2

/* Please note that the first element in all the widgets is a     */
/* widget variable of type Widget.  We abuse this fact everywhere */

#define HIDDEN_BUTTON		0
#define NARROW_BUTTON		1
#define NORMAL_BUTTON		2
#define DEFPUSH_BUTTON		3

typedef struct WButton {
    Widget widget;
    int action;			/* what to do when pressed */
    int selected;		/* button state */
    unsigned int flags;		/* button flags */
    char *text;			/* text of button */
    int hotkey;			/* hot KEY */
    int hotpos;			/* offset hot KEY char in text */
    int (*callback) (int);	/* Callback function */
} WButton;

typedef struct WRadio {
    Widget widget;
    unsigned int state;		/* radio button state */
    int pos, sel;
    int count;			/* number of members */
    char **texts;		/* texts of labels */
    int upper_letter_is_hotkey; /* If true, then the capital letter is a hk */
} WRadio;

typedef struct WCheck {
    Widget widget;
    unsigned int state;		/* check button state */
    char *text;			/* text of check button */
    int hotkey;                 /* hot KEY */                    
    int hotpos;			/* offset hot KEY char in text */
} WCheck;

typedef struct WGauge {
    Widget widget;
    int shown;
    int max;
    int current;
} WGauge;

GList *history_get (char *input_name);
void history_put (char *input_name, GList *h);
char *show_hist (GList *history, int widget_y, int widget_x);

typedef struct {
    Widget widget;
    int  point;			/* cursor position in the input line */
    int  mark;			/* The mark position */
    int  first_shown;		/* Index of the first shown character */
    int  current_max_len;	/* Maximum length of input line */
    int  field_len;		/* Length of the editing field */
    int  color;			/* color used */
    int  first;			/* Is first keystroke? */
    int  disable_update;	/* Do we want to skip updates? */
    int  is_password;		/* Is this a password input line? */
    unsigned char *buffer;	/* pointer to editing buffer */
    GList *history;		/* The history */
    int  need_push;		/* need to push the current Input on hist? */
    char **completions;		/* Possible completions array */
    int  completion_flags;	/* INPUT_COMPLETE* bitwise flags(complete.h) */
    char *history_name;		/* name of history for loading and saving */
} WInput;

/* For history load-save functions */
#define INPUT_LAST_TEXT ((char *) 2)
#define HISTORY_FILE_NAME ".mc/history"

typedef struct {
    Widget widget;
    int    auto_adjust_cols;	/* compute widget.cols from strlen(text)? */
    char   *text;
    int	   transparent;		/* Paint in the default color fg/bg */
} WLabel;

typedef struct WLEntry {
    char *text;			/* Text to display */
    int  hotkey;
    void *data;			/* Client information */
    struct WLEntry *next;
    struct WLEntry *prev;
} WLEntry;

/* Listbox actions when selecting an option: */
enum {
    listbox_nothing,
    listbox_finish		/* finish dialog automatically */
};

struct WListbox;
typedef struct WListbox WListbox;
typedef int (*lcback) (WListbox *);

struct WListbox {
    Widget widget;
    WLEntry *list;		/* Pointer to the circular double linked list. */
    WLEntry *top;		/* The first element displayed */
    WLEntry *current;		/* The current element displayed */
    int pos;			/* Cur. pos, must be kept in sync with current */
    int count;			/* Number of items in the listbox */
    int width;
    int height;			/* Size of the widget */
    int action;			/* Action type */
    int allow_duplicates;	/* Do we allow duplicates on the list? */
    int scrollbar;		/* Draw a scrollbar? */
    lcback cback;		/* The callback function */
    int cursor_x, cursor_y;	/* Cache the values */
};

typedef void (*buttonbarfn)(void *);

typedef struct {
    Widget widget;
    int    visible;		/* Is it visible? */
    struct {
	char   *text;
	buttonbarfn function;
	void   *data;
    } labels [10];
} WButtonBar;

/* Constructors */
WButton *button_new   (int y, int x, int action, int flags, char *text, 
			int (*callback)(int));
WRadio  *radio_new    (int y, int x, int count, char **text, int use_hotkey);
WCheck  *check_new    (int y, int x, int state,  char *text);
WInput  *input_new    (int y, int x, int color, int len, const char *text, char *histname);
WLabel  *label_new    (int y, int x, const char *text);
WGauge  *gauge_new    (int y, int x, int shown, int max, int current);
WListbox *listbox_new (int x, int y, int width, int height, int action, lcback callback);

/* Input lines */
void winput_set_origin (WInput *i, int x, int field_len);
int handle_char (WInput *in, int c_code);
int is_in_input_map (WInput *in, int c_code);
void update_input (WInput *in, int clear_first);
void new_input (WInput *in);
int push_history (WInput *in, char *text);
void stuff (WInput *in, char *text, int insert_extra_space);
void input_disable_update (WInput *in);
void input_set_prompt (WInput *in, int field_len, char *prompt);
void input_enable_update (WInput *in);
void input_set_point (WInput *in, int pos);
void input_show_cursor (WInput *in);
void assign_text (WInput *in, char *text);
int input_callback (WInput *in, int Msg, int Par);

/* Labels */
void label_set_text (WLabel *label, char *text);

/* Gauges */
void gauge_set_value (WGauge *g, int max, int current);
void gauge_show (WGauge *g, int shown);

/* Buttons */
void button_set_text (WButton *b, char *text);

/* Listbox manager */
WLEntry *listbox_get_data (WListbox *l, int pos);

/* search text int listbox entries */
WLEntry *listbox_search_text (WListbox *l, char *text);
void listbox_select_entry (WListbox *l, WLEntry *dest);
void listbox_select_by_number (WListbox *l, int n);
void listbox_select_last (WListbox *l, int set_top);
void listbox_remove_current (WListbox *l, int force);
void listbox_remove_list (WListbox *l);
void listbox_get_current (WListbox *l, char **string, char **extra);

enum append_pos {
    LISTBOX_APPEND_AT_END,	/* append at the end */
    LISTBOX_APPEND_BEFORE,	/* insert before current */
    LISTBOX_APPEND_AFTER	/* insert after current */
};

char *listbox_add_item (WListbox *l, enum append_pos pos, int
			hotkey, char *text, void *data);

/* Hintbar routines */

/* Buttonbar routines */
WButtonBar *buttonbar_new (int visible);
WButtonBar *find_buttonbar (Dlg_head *h);
typedef void (*voidfn)(void);
void define_label (Dlg_head *, int index, char *text, voidfn);
void define_label_data (Dlg_head *h, int idx, char *text,
			buttonbarfn cback, void *data);
void redraw_labels (Dlg_head *h);
void buttonbar_hint (WButtonBar *bb, char *s);

#endif	/* __WIDGET_H */
