#ifndef MC_DLG_H
#define MC_DLG_H
#include "mouse.h"

/* Color constants */
#define FOCUSC           h->color[1]
#define NORMALC          h->color[0]
#define HOT_NORMALC      h->color[2]
#define HOT_FOCUSC       h->color[3]

/* Common return values */
#define B_EXIT		0
#define B_CANCEL	1
#define B_ENTER		2
#define B_HELP		3
#define B_USER          100

/* Widget messages */
enum {
    WIDGET_INIT,		/* Initialize widget */
    WIDGET_FOCUS,		/* Draw widget in focused state */
    WIDGET_UNFOCUS,		/* Draw widget in unfocused state */
    WIDGET_DRAW,		/* Sent to widget to draw themselves */
    WIDGET_KEY,			/* Sent to widgets on key press */
    WIDGET_HOTKEY,		/* Sent to widget to catch preprocess key */
    WIDGET_DESTROY,		/* Sent to widget at destruction time */
    WIDGET_CURSOR,		/* Sent to widget to position the cursor */
    WIDGET_IDLE,		/* Send to widgets with options & W_WANT_IDLE*/
    WIDGET_USER  = 0x100000

} /* Widget_Messages */;

enum {
    MSG_NOT_HANDLED,
    MSG_HANDLED
} /* WRET */;

/* Widgets are expected to answer to the following messages:

   WIDGET_FOCUS:   1 if the accept the focus, 0 if they do not.
   WIDGET_UNFOCUS: 1 if they accept to release the focus, 0 if they don't.
   WIDGET_KEY:     1 if they actually used the key, 0 if not.
   WIDGET_HOTKEY:  1 if they actually used the key, 0 if not.
*/

/* Dialog messages */
enum {
    DLG_KEY,			/* Key before sending to widget */
    DLG_INIT,			/* Initialize dialog */
    DLG_END,			/* Shut down dialog */
    DLG_ACTION,			/* State of check- and radioboxes has changed */
    DLG_DRAW,			/* Draw dialog on screen */
    DLG_FOCUS,			/* A widget has got focus */
    DLG_UNFOCUS,		/* A widget has been unfocused */
    DLG_RESIZE,			/* Window size has changed */
    DLG_POST_KEY,		/* The key has been handled */
    DLG_IDLE,			/* The idle state is active */
    DLG_UNHANDLED_KEY,		/* Key that no widget handled */
    DLG_HOTKEY_HANDLED,		/* A widget has got the hotkey */
    DLG_VALIDATE		/* Dialog is to be closed */
} /* Dialog_Messages */ ;


/* Dialog callback */
struct Dlg_head;
typedef int (*dlg_cb_fn)(struct Dlg_head *h, int Par, int Msg);

typedef struct Dlg_head {

    /* Set by the user */
    int flags;			/* User flags */
    char *help_ctx;		/* Name of the help entry */
    const int *color;		/* Color set */
    char *title;		/* Title of the dialog */

    /* Set and received by the user */
    int ret_value;		/* Result of run_dlg() */

    /* Geometry */
    int x, y;			/* Position relative to screen origin */
    int cols, lines;		/* Width and height of the window */

    /* Internal flags */
    int running;
    int mouse_status;		/* For the autorepeat status of the mouse */
    int refresh_pushed;		/* Did the dialog actually run? */

    /* Internal variables */
    int count;			/* number of widgets */
    struct Widget_Item *current, *first, *last;
    dlg_cb_fn callback;
    struct Widget_Item *initfocus;
    void *previous_dialog;	/* Pointer to the previously running Dlg_head */

} Dlg_head;


/* Call when the widget is destroyed */
typedef void (*destroy_fn)(void *widget);

/* Widget callback */
typedef int (*callback_fn)(void *widget, int Msg, int Par);

/* Every Widget must have this as it's first element */
typedef struct Widget {
    int x, y;
    int cols, lines;
    int options;
    callback_fn callback;  /* The callback function */
    destroy_fn destroy;
    mouse_h mouse;
    struct Dlg_head *parent;
} Widget;

/* The options for the widgets */
#define  W_WANT_HOTKEY       2
#define  W_WANT_CURSOR       4
#define  W_WANT_IDLE         8
#define  W_IS_INPUT         16

/* Items in the circular buffer.  Each item refers to a widget.  */
typedef struct Widget_Item {
    int dlg_id;
    struct Widget_Item *next;
    struct Widget_Item *prev;
    Widget *widget;
} Widget_Item;

/* draw box in window */
void draw_box (Dlg_head *h, int y, int x, int ys, int xs);

/* doubled line if possible */
void draw_double_box (Dlg_head *h, int y, int x, int ys, int xs);

/* Creates a dialog head  */
Dlg_head *create_dlg (int y1, int x1, int lines, int cols,
		      const int *color_set, dlg_cb_fn callback,
		      char *help_ctx, const char *title, int flags);


/* The flags: */
#define DLG_WANT_IDLE   64	/* Dialog wants idle events */
#define DLG_BACKWARD    32	/* Tab order is reverse to the index order */
#define DLG_WANT_TAB    16	/* Should the tab key be sent to the dialog? */
#define DLG_HAS_MENUBAR  8	/* GrossHack: Send events on row 1 to a menubar? */
#define DLG_COMPACT      4	/* Suppress spaces around the frame */
#define DLG_TRYUP        2	/* Try to move two lines up the dialog */
#define DLG_CENTER       1	/* Center the dialog */
#define DLG_NONE         0	/* No options */

int  add_widget           (Dlg_head *dest, void *Widget);

/* Runs dialog d */       
int run_dlg               (Dlg_head *d);
		          
void dlg_run_done         (Dlg_head *h);
void dlg_process_event    (Dlg_head *h, int key, Gpm_Event *event);
void init_dlg             (Dlg_head *h);

/* To activate/deactivate the idle message generation */
void set_idle_proc        (Dlg_head *d, int enable);
		          
void dlg_redraw           (Dlg_head *h);
void destroy_dlg          (Dlg_head *h);
		          
void widget_set_size      (Widget *widget, int x1, int y1, int x2, int y2);

void dlg_broadcast_msg    (Dlg_head *h, int message, int reverse);

void init_widget (Widget *w, int y, int x, int lines, int cols,
		  callback_fn callback, destroy_fn destroy,
		  mouse_h mouse_handler);

/* Default callback for dialogs */
int default_dlg_callback  (Dlg_head *h, int id, int msg);

/* Default callback for widgets */
int default_proc          (int Msg, int Par);

/* Default paint routine for dialogs */
void common_dialog_repaint (struct Dlg_head *h);

#define real_widget_move(w, _y, _x) move((w)->y + _y, (w)->x + _x)
#define dlg_move(h, _y, _x) move(((Dlg_head *) h)->y + _y, \
			     ((Dlg_head *) h)->x + _x)

#define widget_move(w,y,x) real_widget_move((Widget*)w,y,x)

extern Dlg_head *current_dlg;

static inline int
send_message (Widget *w, int Msg, int Par)
{
    return (*(w->callback))(w, Msg, Par);
}

void dlg_replace_widget   (Dlg_head *h, Widget *old, Widget *new);
int  dlg_overlap          (Widget *a, Widget *b);
void widget_erase         (Widget *);
void dlg_erase            (Dlg_head *h);
void dlg_stop             (Dlg_head *h);

/* Widget selection */
int  dlg_select_widget     (Dlg_head *h, void *widget);
void dlg_one_up            (Dlg_head *h);
void dlg_one_down          (Dlg_head *h);
int  dlg_focus             (Dlg_head *h);
int  dlg_select_nth_widget (Dlg_head *h, int n);
int  dlg_item_number       (Dlg_head *h);
Widget *find_widget_type   (Dlg_head *h, void *callback);

/* Sets/clear the specified flag in the options field */
#define widget_option(w,f,i) \
    w.options = ((i) ? (w.options | (f)) : (w.options & (~(f))))

#define widget_want_cursor(w,i) widget_option(w, W_WANT_CURSOR, i)
#define widget_want_hotkey(w,i) widget_option(w, W_WANT_HOTKEY, i)
#define widget_want_postkey(w,i) widget_option(w, W_WANT_POSTKEY, i)

/* Used in load_prompt() */
void update_cursor (Dlg_head *h);

#endif /* MC_DLG_H */
