
/** \file widget.h
 *  \brief Header: widgets
 */

#ifndef MC_WIDGET_H
#define MC_WIDGET_H

#include "lib/global.h"
#include "dialog.h"		/* Widget */

/* Completion stuff */

typedef enum {
    INPUT_COMPLETE_FILENAMES = 1<<0,
    INPUT_COMPLETE_HOSTNAMES = 1<<1,
    INPUT_COMPLETE_COMMANDS  = 1<<2,
    INPUT_COMPLETE_VARIABLES = 1<<3,
    INPUT_COMPLETE_USERNAMES = 1<<4,
    INPUT_COMPLETE_CD        = 1<<5,
    INPUT_COMPLETE_SHELL_ESC = 1<<6,

    INPUT_COMPLETE_DEFAULT   = INPUT_COMPLETE_FILENAMES
                             | INPUT_COMPLETE_HOSTNAMES
                             | INPUT_COMPLETE_VARIABLES
                             | INPUT_COMPLETE_USERNAMES
} INPUT_COMPLETE_FLAGS;

/* Please note that the first element in all the widgets is a     */
/* widget variable of type Widget.  We abuse this fact everywhere */

/* button callback */
typedef int (*bcback) (int);

/* structure for label (caption) with hotkey, if original text does not contain
 * hotkey, only start is valid and is equal to original text
 * hotkey is defined as char*, but mc support only singlebyte hotkey
 */
struct hotkey_t {
    char *start;
    char *hotkey;
    char *end;
};

/* used in static definition of menu entries */
#define NULL_HOTKEY {NULL, NULL, NULL}

/* create hotkey from text */
struct hotkey_t parse_hotkey (const char *text);
/* release hotkey, free all mebers of hotkey_t */
void release_hotkey (const struct hotkey_t hotkey);
/* return width on terminal of hotkey */
int hotkey_width (const struct hotkey_t hotkey);

typedef struct WButton {
    Widget widget;
    int action;			/* what to do when pressed */
    int selected;		/* button state */

#define HIDDEN_BUTTON		0
#define NARROW_BUTTON		1
#define NORMAL_BUTTON		2
#define DEFPUSH_BUTTON		3
    unsigned int flags;		/* button flags */
    struct hotkey_t text;	/* text of button, contain hotkey too */
    int hotpos;			/* offset hot KEY char in text */
    bcback callback;		/* Callback function */
} WButton;

typedef struct WRadio {
    Widget widget;
    unsigned int state;		/* radio button state */
    int pos, sel;
    int count;			/* number of members */
    struct hotkey_t *texts;	/* texts of labels */
} WRadio;

typedef struct WCheck {
    Widget widget;

#define C_BOOL			0x0001
#define C_CHANGE		0x0002
    unsigned int state;		/* check button state */
    struct hotkey_t text;		/* text of check button */
} WCheck;

typedef struct WGauge {
    Widget widget;
    int shown;
    int max;
    int current;
} WGauge;

GList *history_get (const char *input_name);
void history_put (const char *input_name, GList *h);
/* for repositioning of history dialog we should pass widget to this
 * function, as position of history dialog depends on widget's position */
char *show_hist (GList **history, Widget *widget);

typedef struct {
    Widget widget;
    int  point;		   /* cursor position in the input line in characters */
    int  mark;			/* The mark position in characters */
    int  term_first_shown;	/* column of the first shown character */
    size_t current_max_size;	/* Maximum length of input line (bytes) */
    int  field_width;		/* width of the editing field */
    int  color;			/* color used */
    int  first;			/* Is first keystroke? */
    int  disable_update;	/* Do we want to skip updates? */
    int  is_password;		/* Is this a password input line? */
    char *buffer;		/* pointer to editing buffer */
    GList *history;		/* The history */
    int  need_push;		/* need to push the current Input on hist? */
    char **completions;		/* Possible completions array */
    INPUT_COMPLETE_FLAGS completion_flags;	/* INPUT_COMPLETE* bitwise flags(complete.h) */
    char *history_name;		/* name of history for loading and saving */
    char charbuf[MB_LEN_MAX];   /* buffer for multibytes characters */
    size_t charpoint;         /* point to end of mulibyte sequence in charbuf */
} WInput;

/* For history load-save functions */
#define INPUT_LAST_TEXT ((char *) 2)

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
} WLEntry;

struct WListbox;
typedef struct WListbox WListbox;
typedef int (*lcback) (WListbox *);

/* Callback should return one of the following values */
enum {
    LISTBOX_CONT,		/* continue */
    LISTBOX_DONE		/* finish dialog */
};

struct WListbox {
    Widget widget;
    GList *list;		/* Pointer to the double linked list */
    int pos;			/* The current element displayed */
    int top;			/* The first element displayed */
    int count;			/* Number of items in the listbox */
    gboolean allow_duplicates;	/* Do we allow duplicates on the list? */
    gboolean scrollbar;		/* Draw a scrollbar? */
    gboolean deletable;		/* Can list entries be deleted? */
    lcback cback;		/* The callback function */
    int cursor_x, cursor_y;	/* Cache the values */
};

/* number of bttons in buttonbar */
#define BUTTONBAR_LABELS_NUM	10

typedef struct WButtonBar {
    Widget widget;
    gboolean visible;		/* Is it visible? */
    int btn_width;		/* width of one button */
    struct {
	char *text;
	unsigned long command;
	Widget *receiver;
    } labels [BUTTONBAR_LABELS_NUM];
} WButtonBar;

typedef struct WGroupbox {
    Widget widget;
    char *title;
} WGroupbox;


/* Default callback for widgets */
cb_ret_t default_proc (widget_msg_t msg, int parm);

/* Constructors */
WButton *button_new   (int y, int x, int action, int flags, const char *text,
		      bcback callback);
WRadio  *radio_new    (int y, int x, int count, const char **text);
WCheck  *check_new    (int y, int x, int state,  const char *text);
WInput  *input_new    (int y, int x, int color, int len, const char *text, const char *histname, INPUT_COMPLETE_FLAGS completion_flags);
WLabel  *label_new    (int y, int x, const char *text);
WGauge  *gauge_new    (int y, int x, int shown, int max, int current);
WListbox *listbox_new (int y, int x, int height, int width, gboolean deletable, lcback callback);
WButtonBar *buttonbar_new (gboolean visible);
WGroupbox *groupbox_new (int y, int x, int height, int width, const char *title);

/* Input lines */
void winput_set_origin (WInput *i, int x, int field_width);
cb_ret_t handle_char (WInput *in, int c_code);
int is_in_input_map (WInput *in, int c_code);
void update_input (WInput *in, int clear_first);
void new_input (WInput *in);
void stuff (WInput *in, const char *text, int insert_extra_space);
void input_disable_update (WInput *in);
void input_set_prompt (WInput *in, int field_len, const char *prompt);
void input_enable_update (WInput *in);
void input_set_point (WInput *in, int pos);
void input_show_cursor (WInput *in);
void assign_text (WInput *in, const char *text);
cb_ret_t input_callback (Widget *, widget_msg_t msg, int parm);

/* Labels */
void label_set_text (WLabel *label, const char *text);

/* Gauges */
void gauge_set_value (WGauge *g, int max, int current);
void gauge_show (WGauge *g, int shown);

/* Buttons */
/* return copy of button text */
const char *button_get_text (const WButton *b);
void button_set_text (WButton *b, const char *text);
int button_get_len (const WButton *b);

/* Listbox manager */
WLEntry *listbox_get_data (WListbox *l, int pos);

/* search text int listbox entries */
int listbox_search_text (WListbox *l, const char *text);
void listbox_select_entry (WListbox *l, int dest);
void listbox_select_first (WListbox *l);
void listbox_select_last (WListbox *l);
void listbox_remove_current (WListbox *l);
void listbox_set_list (WListbox *l, GList *list);
void listbox_remove_list (WListbox *l);
void listbox_get_current (WListbox *l, char **string, void **extra);

typedef enum {
    LISTBOX_APPEND_AT_END = 0,	/* append at the end */
    LISTBOX_APPEND_BEFORE,	/* insert before current */
    LISTBOX_APPEND_AFTER,	/* insert after current */
    LISTBOX_APPEND_SORTED	/* insert alphabetically */
} listbox_append_t;

char *listbox_add_item (WListbox *l, listbox_append_t pos,
			int hotkey, const char *text, void *data);

struct global_keymap_t;

WButtonBar *find_buttonbar (const Dlg_head *h);
void buttonbar_set_label (WButtonBar *bb, int idx, const char *text,
			    const struct global_keymap_t *keymap, const Widget *receiver);
#define buttonbar_clear_label(bb, idx, recv) buttonbar_set_label (bb, idx, "", NULL, recv)
void buttonbar_set_visible (WButtonBar *bb, gboolean visible);
void buttonbar_redraw (WButtonBar *bb);

void free_completions (WInput *in);
void complete (WInput *in);

#endif					/* MC_WIDGET_H */
