/** \file quick.h
 *  \brief Header: quick dialog engine
 */

#ifndef MC__QUICK_H
#define MC__QUICK_H

/*** typedefs(not structures) and defined constants **********************************************/

#define QUICK_CHECKBOX(x, xdiv, y, ydiv, txt, st)                       \
{                                                                       \
    .widget_type = quick_checkbox,                                      \
    .relative_x = x,                                                    \
    .x_divisions = xdiv,                                                \
    .relative_y = y,                                                    \
    .y_divisions = ydiv,                                                \
    .widget = NULL,                                                     \
    .options = 0,                                                       \
    .u = {                                                              \
        .checkbox = {                                                   \
            .text = txt,                                                \
            .state = st                                                 \
        }                                                               \
    }                                                                   \
}

#define QUICK_BUTTON(x, xdiv, y, ydiv, txt, act, cb)                    \
{                                                                       \
    .widget_type = quick_button,                                        \
    .relative_x = x,                                                    \
    .x_divisions = xdiv,                                                \
    .relative_y = y,                                                    \
    .y_divisions = ydiv,                                                \
    .widget = NULL,                                                     \
    .options = 0,                                                       \
    .u = {                                                              \
        .button = {                                                     \
            .text = txt,                                                \
            .action = act,                                              \
            .callback = cb                                              \
        }                                                               \
    }                                                                   \
}

#define QUICK_INPUT(x, xdiv, y, ydiv, txt, len_, flags_, hname, res)    \
{                                                                       \
    .widget_type = quick_input,                                         \
    .relative_x = x,                                                    \
    .x_divisions = xdiv,                                                \
    .relative_y = y,                                                    \
    .y_divisions = ydiv,                                                \
    .widget = NULL,                                                     \
    .options = 0,                                                       \
    .u = {                                                              \
        .input = {                                                      \
            .text = txt,                                                \
            .len = len_,                                                \
            .flags = flags_,                                            \
            .histname = hname,                                          \
            .result = res                                               \
        }                                                               \
    }                                                                   \
}

#define QUICK_LABEL(x, xdiv, y, ydiv, txt)                              \
{                                                                       \
    .widget_type = quick_label,                                         \
    .relative_x = x,                                                    \
    .x_divisions = xdiv,                                                \
    .relative_y = y,                                                    \
    .y_divisions = ydiv,                                                \
    .widget = NULL,                                                     \
    .options = 0,                                                       \
    .u = {                                                              \
        .label = {                                                      \
            .text = txt                                                 \
        }                                                               \
    }                                                                   \
}

#define QUICK_RADIO(x, xdiv, y, ydiv, cnt, items_, val)                 \
{                                                                       \
    .widget_type = quick_radio,                                         \
    .relative_x = x,                                                    \
    .x_divisions = xdiv,                                                \
    .relative_y = y,                                                    \
    .y_divisions = ydiv,                                                \
    .widget = NULL,                                                     \
    .options = 0,                                                       \
    .u = {                                                              \
        .radio = {                                                      \
            .count = cnt,                                               \
            .items = items_,                                            \
            .value = val                                                \
        }                                                               \
    }                                                                   \
}

#define QUICK_GROUPBOX(x, xdiv, y, ydiv, w, h, t)                       \
{                                                                       \
    .widget_type = quick_groupbox,                                      \
    .relative_x = x,                                                    \
    .x_divisions = xdiv,                                                \
    .relative_y = y,                                                    \
    .y_divisions = ydiv,                                                \
    .widget = NULL,                                                     \
    .options = 0,                                                       \
    .u = {                                                              \
        .groupbox = {                                                   \
            .width = w,                                                 \
            .height = h,                                                \
            .title = t                                                  \
        }                                                               \
    }                                                                   \
}

#define QUICK_END                                                       \
{                                                                       \
    .widget_type = quick_end,                                           \
    .relative_x = 0,                                                    \
    .x_divisions = 0,                                                   \
    .relative_y = 0,                                                    \
    .y_divisions = 0,                                                   \
    .widget = NULL,                                                     \
    .options = 0,                                                       \
    .u = {                                                              \
        .input = {                                                      \
            .text = NULL,                                               \
            .len = 0,                                                   \
            .flags = 0,                                                 \
            .histname = NULL,                                           \
            .result = NULL                                              \
        }                                                               \
    }                                                                   \
}

/*** enums ***************************************************************************************/

/* Quick Widgets */
typedef enum
{
    quick_end = 0,
    quick_checkbox = 1,
    quick_button = 2,
    quick_input = 3,
    quick_label = 4,
    quick_radio = 5,
    quick_groupbox = 6
} quick_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/* The widget is placed on relative_?/divisions_? of the parent widget */
typedef struct
{
    quick_t widget_type;

    int relative_x;
    int x_divisions;
    int relative_y;
    int y_divisions;

    Widget *widget;
    widget_options_t options;

    /* widget parameters */
    union
    {
        struct
        {
            const char *text;
            int *state;         /* in/out */
        } checkbox;

        struct
        {
            const char *text;
            int action;
            bcback_fn callback;
        } button;

        struct
        {
            const char *text;
            int len;
            int flags;          /* 1 -- is_password, 2 -- INPUT_COMPLETE_CD */
            const char *histname;
            char **result;
            gboolean strip_password;
        } input;

        struct
        {
            const char *text;
        } label;

        struct
        {
            int count;
            const char **items;
            int *value;         /* in/out */
        } radio;

        struct
        {
            int width;
            int height;
            const char *title;
        } groupbox;
    } u;
} QuickWidget;

typedef struct
{
    int xlen, ylen;
    int xpos, ypos;             /* if -1, then center the dialog */
    const char *title;
    const char *help;
    QuickWidget *widgets;
    dlg_cb_fn callback;
    gboolean i18n;              /* If true, internationalization has happened */
} QuickDialog;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

int quick_dialog_skip (QuickDialog * qd, int nskip);

/*** inline functions ****************************************************************************/

static inline int
quick_dialog (QuickDialog * qd)
{
    return quick_dialog_skip (qd, 0);
}

#endif /* MC__QUICK_H */
