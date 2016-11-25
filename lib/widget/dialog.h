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

/*** typedefs(not structures) and defined constants **********************************************/

#define DIALOG(x) ((WDialog *)(x))

/* Common return values */
/* ATTENTION: avoid overlapping with FileProgressStatus values */
#define B_EXIT          0
#define B_CANCEL        1
#define B_ENTER         2
#define B_HELP          3
#define B_USER          100

/*** enums ***************************************************************************************/

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
typedef char *(*dlg_shortcut_str) (long command);

/* get dialog name to show in dialog list */
typedef char *(*dlg_title_str) (const WDialog * h, size_t len);

typedef int dlg_colors_t[DLG_COLOR_COUNT];

/* menu command execution */
typedef cb_ret_t (*menu_exec_fn) (int command);

/*** structures declarations (and typedefs of structures)*****************************************/

struct WDialog
{
    Widget widget;

    /* Set by the user */
    gboolean compact;           /* Suppress spaces around the frame */
    const char *help_ctx;       /* Name of the help entry */
    const int *color;           /* Color set. Unused in viewer and editor */
    char *title;                /* Title of the dialog */

    /* Set and received by the user */
    int ret_value;              /* Result of dlg_run() */

    /* Internal flags */
    gboolean winch_pending;     /* SIGWINCH signal has been got. Resize dialog after rise */
    int mouse_status;           /* For the autorepeat status of the mouse */

    /* Internal variables */
    GList *widgets;             /* widgets list */
    GList *current;             /* Currently active widget */
    unsigned long widget_id;    /* maximum id of all widgets */
    void *data;                 /* Data can be passed to dialog */
    char *event_group;          /* Name of event group for this dialog */

    dlg_shortcut_str get_shortcut;      /* Shortcut string */
    dlg_title_str get_title;    /* useless for modal dialogs */
};

/*** global variables defined in .c file *********************************************************/

/* Color styles for normal and error dialogs */
extern dlg_colors_t dialog_colors;
extern dlg_colors_t alarm_colors;
extern dlg_colors_t listbox_colors;

extern GList *top_dlg;

/* A hook list for idle events */
extern hook_t *idle_hook;

extern gboolean fast_refresh;
extern gboolean mouse_close_dialog;

extern const global_keymap_t *dialog_map;

/*** declarations of public functions ************************************************************/

/* Creates a dialog head  */
WDialog *dlg_create (gboolean modal, int y1, int x1, int lines, int cols,
                     widget_pos_flags_t pos_flags, gboolean compact,
                     const int *colors, widget_cb_fn callback, widget_mouse_cb_fn mouse_callback,
                     const char *help_ctx, const char *title);

void dlg_set_default_colors (void);

unsigned long add_widget_autopos (WDialog * dest, void *w, widget_pos_flags_t pos_flags,
                                  const void *before);
unsigned long add_widget (WDialog * dest, void *w);
unsigned long add_widget_before (WDialog * h, void *w, void *before);
void del_widget (void *w);

/* sets size of dialog, leaving positioning to automatic mehtods
   according to dialog flags */
void dlg_set_size (WDialog * h, int lines, int cols);
/* this function allows to set dialog position */
void dlg_set_position (WDialog * h, int y, int x, int lines, int cols);

void dlg_init (WDialog * h);
int dlg_run (WDialog * d);
void dlg_destroy (WDialog * h);

void dlg_run_done (WDialog * h);
void dlg_save_history (WDialog * h);
void dlg_process_event (WDialog * h, int key, Gpm_Event * event);

char *dlg_get_title (const WDialog * h, size_t len);

void dlg_redraw (WDialog * h);

void dlg_broadcast_msg (WDialog * h, widget_msg_t message);

/* Default callback for dialogs */
cb_ret_t dlg_default_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data);

/* Default paint routine for dialogs */
void dlg_default_repaint (WDialog * h);

void dlg_erase (WDialog * h);
void dlg_stop (WDialog * h);

/* Widget selection */
void dlg_select_prev_widget (WDialog * h);
void dlg_select_next_widget (WDialog * h);
Widget *find_widget_type (const WDialog * h, widget_cb_fn callback);
GList *dlg_find (const WDialog * h, const Widget * w);
Widget *dlg_find_by_id (const WDialog * h, unsigned long id);
void dlg_select_by_id (const WDialog * h, unsigned long id);

/* Redraw all dialogs */
void do_refresh (void);

/* Used in load_prompt() */
void update_cursor (WDialog * h);

void dlg_set_current_widget_next (WDialog * h);
void dlg_set_current_widget_prev (WDialog * h);

GList *dlg_get_widget_next_of (GList * w);
GList *dlg_get_widget_prev_of (GList * w);

/* --------------------------------------------------------------------------------------------- */
/*** inline functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static inline unsigned long
dlg_get_current_widget_id (const WDialog * h)
{
    return WIDGET (h->current->data)->id;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Select current widget in the Dialog.
 *
 * @param h WDialog object
 */

static inline void
dlg_select_current_widget (WDialog * h)
{
    if (h->current != NULL)
        widget_select (WIDGET (h->current->data));
}

/* --------------------------------------------------------------------------------------------- */

#endif /* MC__DIALOG_H */
