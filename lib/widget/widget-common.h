
/** \file widget-common.h
 *  \brief Header: shared stuff of widgets
 */

#ifndef MC__WIDGET_INTERNAL_H
#define MC__WIDGET_INTERNAL_H

#include "lib/tty/mouse.h"
#include "lib/widget/mouse.h"   /* mouse_msg_t, mouse_event_t */

/*** typedefs(not structures) and defined constants **********************************************/

#define WIDGET(x) ((Widget *)(x))
#define CONST_WIDGET(x) ((const Widget *)(x))

#define widget_move(w, _y, _x) tty_gotoyx (CONST_WIDGET(w)->y + (_y), CONST_WIDGET(w)->x + (_x))
/* Sets/clear the specified flag in the options field */
#define widget_want_cursor(w,i) widget_set_options(w, WOP_WANT_CURSOR, i)
#define widget_want_hotkey(w,i) widget_set_options(w, WOP_WANT_HOTKEY, i)
#define widget_want_tab(w,i) widget_set_options(w, WOP_WANT_TAB, i)
#define widget_idle(w,i) widget_set_state(w, WST_IDLE, i)
#define widget_disable(w,i) widget_set_state(w, WST_DISABLED, i)

/*** enums ***************************************************************************************/

/* Widget messages */
typedef enum
{
    MSG_INIT = 0,               /* Initialize widget */
    MSG_FOCUS,                  /* Draw widget in focused state or widget has got focus */
    MSG_UNFOCUS,                /* Draw widget in unfocused state or widget has been unfocused */
    MSG_CHANGED_FOCUS,          /* Notification to owner about focus state change */
    MSG_ENABLE,                 /* Change state to enabled */
    MSG_DISABLE,                /* Change state to disabled */
    MSG_DRAW,                   /* Draw widget on screen */
    MSG_KEY,                    /* Sent to widgets on key press */
    MSG_HOTKEY,                 /* Sent to widget to catch preprocess key */
    MSG_HOTKEY_HANDLED,         /* A widget has got the hotkey */
    MSG_UNHANDLED_KEY,          /* Key that no widget handled */
    MSG_POST_KEY,               /* The key has been handled */
    MSG_ACTION,                 /* Send to widget to handle command */
    MSG_NOTIFY,                 /* Typically sent to dialog to inform it of state-change
                                 * of listboxes, check- and radiobuttons. */
    MSG_CURSOR,                 /* Sent to widget to position the cursor */
    MSG_IDLE,                   /* The idle state is active */
    MSG_RESIZE,                 /* Screen size has changed */
    MSG_VALIDATE,               /* Dialog is to be closed */
    MSG_END,                    /* Shut down dialog */
    MSG_DESTROY                 /* Sent to widget at destruction time */
} widget_msg_t;

/* Widgets are expected to answer to the following messages:
   MSG_FOCUS:   MSG_HANDLED if the accept the focus, MSG_NOT_HANDLED if they do not.
   MSG_UNFOCUS: MSG_HANDLED if they accept to release the focus, MSG_NOT_HANDLED if they don't.
   MSG_KEY:     MSG_HANDLED if they actually used the key, MSG_NOT_HANDLED if not.
   MSG_HOTKEY:  MSG_HANDLED if they actually used the key, MSG_NOT_HANDLED if not.
 */

typedef enum
{
    MSG_NOT_HANDLED = 0,
    MSG_HANDLED = 1
} cb_ret_t;

/* Widget options */
typedef enum
{
    WOP_DEFAULT = (0 << 0),
    WOP_WANT_HOTKEY = (1 << 0),
    WOP_WANT_CURSOR = (1 << 1),
    WOP_WANT_TAB = (1 << 2),    /* Should the tab key be sent to the dialog? */
    WOP_IS_INPUT = (1 << 3),
    WOP_SELECTABLE = (1 << 4),
    WOP_TOP_SELECT = (1 << 5)
} widget_options_t;

/* Widget state */
typedef enum
{
    WST_DEFAULT = (0 << 0),
    WST_DISABLED = (1 << 0),    /* Widget cannot be selected */
    WST_IDLE = (1 << 1),
    WST_MODAL = (1 << 2),       /* Widget (dialog) is modal */
    WST_FOCUSED = (1 << 3),

    WST_CONSTRUCT = (1 << 15),  /* Dialog has been constructed but not run yet */
    WST_ACTIVE = (1 << 16),     /* Dialog is visible and active */
    WST_SUSPENDED = (1 << 17),  /* Dialog is suspended */
    WST_CLOSED = (1 << 18)      /* Dialog is closed */
} widget_state_t;

/* Flags for widget repositioning on dialog resize */
typedef enum
{
    WPOS_FULLSCREEN = (1 << 0), /* widget occupies the whole screen */
    WPOS_CENTER_HORZ = (1 << 1),        /* center widget in horizontal */
    WPOS_CENTER_VERT = (1 << 2),        /* center widget in vertical */
    WPOS_CENTER = WPOS_CENTER_HORZ | WPOS_CENTER_VERT,  /* center widget */
    WPOS_TRYUP = (1 << 3),      /* try to move two lines up the widget */
    WPOS_KEEP_LEFT = (1 << 4),  /* keep widget distance to left border of dialog */
    WPOS_KEEP_RIGHT = (1 << 5), /* keep widget distance to right border of dialog */
    WPOS_KEEP_TOP = (1 << 6),   /* keep widget distance to top border of dialog */
    WPOS_KEEP_BOTTOM = (1 << 7),        /* keep widget distance to bottom border of dialog */
    WPOS_KEEP_HORZ = WPOS_KEEP_LEFT | WPOS_KEEP_RIGHT,
    WPOS_KEEP_VERT = WPOS_KEEP_TOP | WPOS_KEEP_BOTTOM,
    WPOS_KEEP_ALL = WPOS_KEEP_HORZ | WPOS_KEEP_VERT,
    WPOS_KEEP_DEFAULT = WPOS_KEEP_LEFT | WPOS_KEEP_TOP
} widget_pos_flags_t;
/* NOTES:
 * If WPOS_FULLSCREEN is set then all other position flags are ignored.
 * If WPOS_CENTER_HORZ flag is used, other horizontal flags (WPOS_KEEP_LEFT, WPOS_KEEP_RIGHT,
 * and WPOS_KEEP_HORZ) are ignored.
 * If WPOS_CENTER_VERT flag is used, other horizontal flags (WPOS_KEEP_TOP, WPOS_KEEP_BOTTOM,
 * and WPOS_KEEP_VERT) are ignored.
 */

/*** structures declarations (and typedefs of structures)*****************************************/

/* Widget callback */
typedef cb_ret_t (*widget_cb_fn) (Widget * widget, Widget * sender, widget_msg_t msg, int parm,
                                  void *data);
/* Widget mouse callback */
typedef void (*widget_mouse_cb_fn) (Widget * w, mouse_msg_t msg, mouse_event_t * event);

/* Every Widget must have this as its first element */
struct Widget
{
    int x, y;
    int cols, lines;
    widget_pos_flags_t pos_flags;       /* repositioning flags */
    widget_options_t options;
    widget_state_t state;
    unsigned int id;            /* Number of the widget, starting with 0 */
    widget_cb_fn callback;
    widget_mouse_cb_fn mouse_callback;
    WDialog *owner;
    /* Mouse-related fields. */
    struct
    {
        /* Public members: */
        gboolean forced_capture;        /* Overrides the 'capture' member. Set explicitly by the programmer. */

        /* Implementation details: */
        gboolean capture;       /* Whether the widget "owns" the mouse. */
        mouse_msg_t last_msg;   /* The previous event type processed. */
        int last_buttons_down;
    } mouse;
};

/* structure for label (caption) with hotkey, if original text does not contain
 * hotkey, only start is valid and is equal to original text
 * hotkey is defined as char*, but mc support only singlebyte hotkey
 */
typedef struct hotkey_t
{
    char *start;
    char *hotkey;
    char *end;
} hotkey_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* create hotkey from text */
hotkey_t parse_hotkey (const char *text);
/* release hotkey, free all mebers of hotkey_t */
void release_hotkey (const hotkey_t hotkey);
/* return width on terminal of hotkey */
int hotkey_width (const hotkey_t hotkey);
/* draw hotkey of widget */
void hotkey_draw (Widget * w, const hotkey_t hotkey, gboolean focused);

/* widget initialization */
void widget_init (Widget * w, int y, int x, int lines, int cols,
                  widget_cb_fn callback, widget_mouse_cb_fn mouse_callback);
/* Default callback for widgets */
cb_ret_t widget_default_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm,
                                  void *data);
void widget_set_options (Widget * w, widget_options_t options, gboolean enable);
cb_ret_t widget_set_state (Widget * w, widget_state_t state, gboolean enable);
void widget_set_size (Widget * widget, int y, int x, int lines, int cols);
/* select color for widget in dependance of state */
void widget_selectcolor (Widget * w, gboolean focused, gboolean hotkey);
void widget_redraw (Widget * w);
void widget_erase (Widget * w);
gboolean widget_is_active (const void *w);
gboolean widget_overlapped (const Widget * a, const Widget * b);
void widget_replace (Widget * old, Widget * new);
void widget_select (Widget * w);
void widget_set_bottom (Widget * w);

/* get mouse pointer location within widget */
Gpm_Event mouse_get_local (const Gpm_Event * global, const Widget * w);
gboolean mouse_global_in_widget (const Gpm_Event * event, const Widget * w);

/* --------------------------------------------------------------------------------------------- */
/*** inline functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static inline cb_ret_t
send_message (void *w, void *sender, widget_msg_t msg, int parm, void *data)
{
    cb_ret_t ret = MSG_NOT_HANDLED;

#if 1
    if (w != NULL)              /* This must be always true, but... */
#endif
        ret = WIDGET (w)->callback (WIDGET (w), WIDGET (sender), msg, parm, data);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/**
  * Check whether one or several option flags are set or not.
  * @param w widget
  * @param options widget option flags
  *
  * @return TRUE if all requested option flags are set, FALSE otherwise.
  */

static inline gboolean
widget_get_options (const Widget * w, widget_options_t options)
{
    return ((w->options & options) == options);
}

/* --------------------------------------------------------------------------------------------- */

/**
  * Check whether one or several state flags are set or not.
  * @param w widget
  * @param state widget state flags
  *
  * @return TRUE if all requested state flags are set, FALSE otherwise.
  */

static inline gboolean
widget_get_state (const Widget * w, widget_state_t state)
{
    return ((w->state & state) == state);
}

/* --------------------------------------------------------------------------------------------- */

#endif /* MC__WIDGET_INTERNAL_H */
