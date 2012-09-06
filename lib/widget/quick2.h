/** \file quick2.h
 *  \brief Header: quick dialog engine
 */

#ifndef MC__QUICK2_H
#define MC__QUICK2_H

#include "lib/tty/mouse.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define QUICK2_CHECKBOX(txt, st, id_)                                           \
{                                                                               \
    .widget_type = quick2_checkbox,                                             \
    .options = 0,                                                               \
    .id = id_,                                                                  \
    .u = {                                                                      \
        .checkbox = {                                                           \
            .text = txt,                                                        \
            .state = st                                                         \
        }                                                                       \
    }                                                                           \
}

#define QUICK2_BUTTON(txt, act, cb, id_)                                        \
{                                                                               \
    .widget_type = quick2_button,                                               \
    .options = 0,                                                               \
    .id = id_,                                                                  \
    .u = {                                                                      \
        .button = {                                                             \
            .text = txt,                                                        \
            .action = act,                                                      \
            .callback = cb                                                      \
        }                                                                       \
    }                                                                           \
}

#define QUICK2_INPUT(txt, flags_, hname, res, id_)                              \
{                                                                               \
    .widget_type = quick2_input,                                                \
    .options = 0,                                                               \
    .id = id_,                                                                  \
    .u = {                                                                      \
        .input = {                                                              \
            .label_text = NULL,                                                 \
            .label_location = input_label_none,                                 \
            .label = NULL,                                                      \
            .text = txt,                                                        \
            .flags = flags_,                                                    \
            .histname = hname,                                                  \
            .result = res                                                       \
        }                                                                       \
    }                                                                           \
}

#define QUICK2_LABELED_INPUT(label_, label_loc, txt, flags_, hname, res, id_)   \
{                                                                               \
    .widget_type = quick2_input,                                                \
    .options = 0,                                                               \
    .id = id_,                                                                  \
    .u = {                                                                      \
        .input = {                                                              \
            .label_text = label_,                                               \
            .label_location = label_loc,                                        \
            .label = NULL,                                                      \
            .text = txt,                                                        \
            .flags = flags_,                                                    \
            .histname = hname,                                                  \
            .result = res                                                       \
        }                                                                       \
    }                                                                           \
}

#define QUICK2_LABEL(txt, id_)                                                  \
{                                                                               \
    .widget_type = quick2_label,                                                \
    .options = 0,                                                               \
    .id = id_,                                                                  \
    .u = {                                                                      \
        .label = {                                                              \
            .text = txt,                                                        \
            .input = NULL                                                       \
        }                                                                       \
    }                                                                           \
}

#define QUICK2_RADIO(cnt, items_, val, id_)                                     \
{                                                                               \
    .widget_type = quick2_radio,                                                \
    .options = 0,                                                               \
    .id = id_,                                                                  \
    .u = {                                                                      \
        .radio = {                                                              \
            .count = cnt,                                                       \
            .items = items_,                                                    \
            .value = val                                                        \
        }                                                                       \
    }                                                                           \
}

#define QUICK2_START_GROUPBOX(t)                                                \
{                                                                               \
    .widget_type = quick2_start_groupbox,                                       \
    .options = 0,                                                               \
    .id = NULL,                                                                 \
    .u = {                                                                      \
        .groupbox = {                                                           \
            .title = t                                                          \
        }                                                                       \
    }                                                                           \
}

#define QUICK2_STOP_GROUPBOX                                                    \
{                                                                               \
    .widget_type = quick2_stop_groupbox,                                        \
    .options = 0,                                                               \
    .id = NULL,                                                                 \
    .u = {                                                                      \
        .input = {                                                              \
            .text = NULL,                                                       \
            .flags = 0,                                                         \
            .histname = NULL,                                                   \
            .result = NULL                                                      \
        }                                                                       \
    }                                                                           \
}

#define QUICK2_SEPARATOR(line_)                                                 \
{                                                                               \
    .widget_type = quick2_separator,                                            \
    .options = 0,                                                               \
    .id = NULL,                                                                 \
    .u = {                                                                      \
        .separator = {                                                          \
            .space = TRUE,                                                      \
            .line = line_                                                       \
        }                                                                       \
    }                                                                           \
}

#define QUICK2_START_COLUMNS                                                    \
{                                                                               \
    .widget_type = quick2_start_columns,                                        \
    .options = 0,                                                               \
    .id = NULL,                                                                 \
    .u = {                                                                      \
        .input = {                                                              \
            .text = NULL,                                                       \
            .flags = 0,                                                         \
            .histname = NULL,                                                   \
            .result = NULL                                                      \
        }                                                                       \
    }                                                                           \
}

#define QUICK2_NEXT_COLUMN                                                      \
{                                                                               \
    .widget_type = quick2_next_column,                                          \
    .options = 0,                                                               \
    .id = NULL,                                                                 \
    .u = {                                                                      \
        .input = {                                                              \
            .text = NULL,                                                       \
            .flags = 0,                                                         \
            .histname = NULL,                                                   \
            .result = NULL                                                      \
        }                                                                       \
    }                                                                           \
}

#define QUICK2_STOP_COLUMNS                                                     \
{                                                                               \
    .widget_type = quick2_stop_columns,                                         \
    .options = 0,                                                               \
    .id = NULL,                                                                 \
    .u = {                                                                      \
        .input = {                                                              \
            .text = NULL,                                                       \
            .flags = 0,                                                         \
            .histname = NULL,                                                   \
            .result = NULL                                                      \
        }                                                                       \
    }                                                                           \
}

#define QUICK2_START_BUTTONS(space_, line_)                                     \
{                                                                               \
    .widget_type = quick2_buttons,                                              \
    .options = 0,                                                               \
    .id = NULL,                                                                 \
    .u = {                                                                      \
        .separator = {                                                          \
            .space = space_,                                                    \
            .line = line_                                                       \
        }                                                                       \
    }                                                                           \
}

#define QUICK2_END                                                              \
{                                                                               \
    .widget_type = quick2_end,                                                  \
    .options = 0,                                                               \
    .id = NULL,                                                                 \
    .u = {                                                                      \
        .input = {                                                              \
            .text = NULL,                                                       \
            .flags = 0,                                                         \
            .histname = NULL,                                                   \
            .result = NULL                                                      \
        }                                                                       \
    }                                                                           \
}

/*** enums ***************************************************************************************/

/* Quick Widgets */
typedef enum
{
    quick2_end = 0,
    quick2_checkbox = 1,
    quick2_button = 2,
    quick2_input = 3,
    quick2_label = 4,
    quick2_radio = 5,
    quick2_start_groupbox = 6,
    quick2_stop_groupbox = 7,
    quick2_separator = 8,
    quick2_start_columns = 9,
    quick2_next_column = 10,
    quick2_stop_columns = 11,
    quick2_buttons = 12
} quick2_t;

typedef enum
{
    input_label_none = 0,
    input_label_above = 1,
    input_label_left = 2,
    input_label_right = 3,
    input_label_below = 4
} quick_input_label_location_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/* The widget is placed on relative_?/divisions_? of the parent widget */
typedef struct quick_widget_t quick_widget_t;

struct quick_widget_t
{
    quick2_t widget_type;

    widget_options_t options;
    unsigned long *id;

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
            const char *label_text;
            quick_input_label_location_t label_location;
            quick_widget_t *label;
            const char *text;
            int flags;          /* 1 -- is_password, 2 -- INPUT_COMPLETE_CD */
            const char *histname;
            char **result;
            gboolean strip_password;
        } input;

        struct
        {
            const char *text;
            quick_widget_t *input;
        } label;

        struct
        {
            int count;
            const char **items;
            int *value;         /* in/out */
        } radio;

        struct
        {
            const char *title;
        } groupbox;

        struct
        {
            gboolean space;
            gboolean line;
        } separator;
    } u;
};

typedef struct
{
    int y, x;                   /* if -1, then center the dialog */
    int cols;                   /* heigth is calculated automatically */
    const char *title;
    const char *help;
    quick_widget_t *widgets;
    dlg_cb_fn callback;
    mouse_h mouse;
} quick_dialog_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

int quick2_dialog_skip (quick_dialog_t * quick_dlg, int nskip);

/*** inline functions ****************************************************************************/

static inline int
quick2_dialog (quick_dialog_t * quick_dlg)
{
    return quick2_dialog_skip (quick_dlg, 1);
}

#endif /* MC__QUICK2_H */
