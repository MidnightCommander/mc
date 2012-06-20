/*
   Dialog box features module for the Midnight Commander
 */

/** \file dialog.h
 *  \brief Header: dialog box features module
 */

#ifndef MC__DIALOG_H
#define MC__DIALOG_H

#include <sys/types.h>          /* size_t */

#include "lib/global.h"
#include "lib/hook.h"           /* hook_t */
#include "lib/keybind.h"        /* global_keymap_t */
#include "lib/tty/mouse.h"      /* mouse_h */

/*** defined constants ***************************************************************************/

/* Common return values */
#define B_EXIT          0
#define B_CANCEL        1
#define B_ENTER         2
#define B_HELP          3
#define B_USER          100

/*** enums ***************************************************************************************/

/* Dialog messages */
typedef enum
{
    DLG_INIT = 0,               /* Initialize dialog */
    DLG_IDLE = 1,               /* The idle state is active */
    DLG_DRAW = 2,               /* Draw dialog on screen */
    DLG_FOCUS = 3,              /* A widget has got focus */
    DLG_UNFOCUS = 4,            /* A widget has been unfocused */
    DLG_RESIZE = 5,             /* Window size has changed */
    DLG_KEY = 6,                /* Key before sending to widget */
    DLG_HOTKEY_HANDLED = 7,     /* A widget has got the hotkey */
    DLG_POST_KEY = 8,           /* The key has been handled */
    DLG_UNHANDLED_KEY = 9,      /* Key that no widget handled */
    DLG_ACTION = 10,            /* State of check- and radioboxes has changed
                                 * and listbox current entry has changed */
    DLG_VALIDATE = 11,          /* Dialog is to be closed */
    DLG_END = 12                /* Shut down dialog */
} dlg_msg_t;

/* Flags for create_dlg */
typedef enum
{
    DLG_REVERSE = (1 << 5),     /* Tab order is opposite to the add order */
    DLG_WANT_TAB = (1 << 4),    /* Should the tab key be sent to the dialog? */
    DLG_WANT_IDLE = (1 << 3),   /* Dialog wants idle events */
    DLG_COMPACT = (1 << 2),     /* Suppress spaces around the frame */
    DLG_TRYUP = (1 << 1),       /* Try to move two lines up the dialog */
    DLG_CENTER = (1 << 0),      /* Center the dialog */
    DLG_NONE = 0                /* No options */
} dlg_flags_t;

/* Dialog state */
typedef enum
{
    DLG_CONSTRUCT = 0,          /* Dialog has been constructed but not run yet */
    DLG_ACTIVE = 1,             /* Dialog is visible and active */
    DLG_SUSPENDED = 2,          /* Dialog is suspended */
    DLG_CLOSED = 3              /* Dialog is closed */
} dlg_state_t;

/* Dialog color constants */
typedef enum
{
    DLG_COLOR_NORMAL,
    DLG_COLOR_FOCUS,
    DLG_COLOR_HOT_NORMAL,
    DLG_COLOR_HOT_FOCUS,
    DLG_COLOR_TITLE,
    DLG_COLOR_COUNT
} dlg_colors_enum_t;

/*** typedefs(not structures) ********************************************************************/

/* get string representation of shortcut assigned  with command */
/* as menu is a widget of dialog, ask dialog about shortcut string */
typedef char *(*dlg_shortcut_str) (unsigned long command);

/* get dialog name to show in dialog list */
typedef char *(*dlg_title_str) (const Dlg_head * h, size_t len);

typedef int dlg_colors_t[DLG_COLOR_COUNT];

/* Dialog callback */
typedef cb_ret_t (*dlg_cb_fn) (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data);

/* menu command execution */
typedef cb_ret_t (*menu_exec_fn) (int command);

/*** structures declarations (and typedefs of structures)*****************************************/

struct Dlg_head
{
    Widget widget;

    /* Set by the user */
    gboolean modal;             /* type of dialog: modal or not */
    dlg_flags_t flags;          /* User flags */
    const char *help_ctx;       /* Name of the help entry */
    dlg_colors_t color;         /* Color set. Unused in viewer and editor */
    char *title;                /* Title of the dialog */

    /* Set and received by the user */
    int ret_value;              /* Result of run_dlg() */

    /* Internal flags */
    dlg_state_t state;
    gboolean fullscreen;        /* Parents dialogs don't need refresh */
    gboolean winch_pending;     /* SIGWINCH signal has been got. Resize dialog after rise */
    int mouse_status;           /* For the autorepeat status of the mouse */

    /* Internal variables */
    GList *widgets;             /* widgets list */
    GList *current;             /* Curently active widget */
    unsigned long widget_id;    /* maximum id of all widgets */
    void *data;                 /* Data can be passed to dialog */
    char *event_group;          /* Name of event group for this dialog */

    dlg_cb_fn callback;
    dlg_shortcut_str get_shortcut;      /* Shortcut string */
    dlg_title_str get_title;    /* useless for modal dialogs */
};

/*** global variables defined in .c file *********************************************************/

/* Color styles for normal and error dialogs */
extern dlg_colors_t dialog_colors;
extern dlg_colors_t alarm_colors;

extern GList *top_dlg;

/* A hook list for idle events */
extern hook_t *idle_hook;

extern int fast_refresh;
extern int mouse_close_dialog;

extern const global_keymap_t *dialog_map;

/*** declarations of public functions ************************************************************/

/* draw box in window */
void draw_box (const Dlg_head * h, int y, int x, int ys, int xs, gboolean single);

/* Creates a dialog head  */
Dlg_head *create_dlg (gboolean modal, int y1, int x1, int lines, int cols,
                      const int *colors, dlg_cb_fn callback, mouse_h mouse_handler,
                      const char *help_ctx, const char *title, dlg_flags_t flags);

void dlg_set_default_colors (void);

unsigned long add_widget_autopos (Dlg_head * dest, void *w, widget_pos_flags_t pos_flags,
                                  const void *before);
unsigned long add_widget (Dlg_head * dest, void *w);
unsigned long add_widget_before (Dlg_head * h, void *w, void *before);
void del_widget (void *w);

/* sets size of dialog, leaving positioning to automatic mehtods
   according to dialog flags */
void dlg_set_size (Dlg_head * h, int lines, int cols);
/* this function allows to set dialog position */
void dlg_set_position (Dlg_head * h, int y1, int x1, int y2, int x2);

void init_dlg (Dlg_head * h);
int run_dlg (Dlg_head * d);
void destroy_dlg (Dlg_head * h);

void dlg_run_done (Dlg_head * h);
void dlg_save_history (Dlg_head * h);
void dlg_process_event (Dlg_head * h, int key, Gpm_Event * event);

char *dlg_get_title (const Dlg_head * h, size_t len);

/* To activate/deactivate the idle message generation */
void set_idle_proc (Dlg_head * d, int enable);

void dlg_redraw (Dlg_head * h);

void dlg_broadcast_msg (Dlg_head * h, widget_msg_t message, gboolean reverse);

/* Default callback for dialogs */
cb_ret_t default_dlg_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data);

/* Default paint routine for dialogs */
void common_dialog_repaint (Dlg_head * h);

void dlg_replace_widget (Widget * old, Widget * new);
int dlg_overlap (Widget * a, Widget * b);
void dlg_erase (Dlg_head * h);
void dlg_stop (Dlg_head * h);

/* Widget selection */
void dlg_select_widget (void *w);
void dlg_set_top_widget (void *w);
void dlg_one_up (Dlg_head * h);
void dlg_one_down (Dlg_head * h);
gboolean dlg_focus (Dlg_head * h);
Widget *find_widget_type (const Dlg_head * h, callback_fn callback);
Widget *dlg_find_by_id (const Dlg_head * h, unsigned long id);
void dlg_select_by_id (const Dlg_head * h, unsigned long id);

/* Redraw all dialogs */
void do_refresh (void);

/* Used in load_prompt() */
void update_cursor (Dlg_head * h);

/*** inline functions ****************************************************************************/

/* Return TRUE if the widget is active, FALSE otherwise */
static inline gboolean
dlg_widget_active (void *w)
{
    return (w == WIDGET (w)->owner->current->data);
}

/* --------------------------------------------------------------------------------------------- */

static inline unsigned long
dlg_get_current_widget_id (const struct Dlg_head *h)
{
    return WIDGET (h->current->data)->id;
}

#endif /* MC__DIALOG_H */
