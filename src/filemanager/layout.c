/*
   Panel layout module for the Midnight Commander

   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007, 2009, 2011
   The Free Software Foundation, Inc.

   Written by:
   Janne Kukonlehto, 1995
   Miguel de Icaza, 1995

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file layout.c
 *  \brief Source: panel layout module
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/skin.h"
#include "lib/tty/key.h"
#include "lib/tty/mouse.h"
#include "lib/mcconfig.h"
#include "lib/vfs/vfs.h"        /* For mc_get_current_wd() */
#include "lib/strutil.h"
#include "lib/widget.h"
#include "lib/event.h"

#include "src/consaver/cons.saver.h"
#include "src/viewer/mcviewer.h"        /* The view widget */
#include "src/setup.h"
#include "src/background.h"

#include "command.h"
#include "midnight.h"
#include "tree.h"
/* Needed for the extern declarations of integer parameters */
#include "dir.h"
#include "layout.h"
#include "info.h"               /* The Info widget */


/*** global variables ****************************************************************************/

/* Controls the display of the rotating dash on the verbose mode */
int nice_rotating_dash = 1;

/* Set if the panels are split horizontally */
int horizontal_split = 0;

/* Set if the split is the same */
int equal_split = 1;

/* First panel size if the panel are not split equally */
int first_panel_size = 0;

/* The number of output lines shown (if available) */
int output_lines = 0;

/* Set if the command prompt is to be displayed */
gboolean command_prompt = TRUE;

/* Set if the main menu is visible */
int menubar_visible = 1;

/* Set to show current working dir in xterm window title */
gboolean xterm_title = TRUE;

/* Set to show free space on device assigned to current directory */
int free_space = 1;

/* The starting line for the output of the subprogram */
int output_start_y = 0;

int ok_to_refresh = 1;

/*** file scope macro definitions ****************************************************************/

/* The maximum number of views managed by the set_display_type routine */
/* Must be at least two (for current and other).  Please note that until */
/* Janne gets around this, we will only manage two of them :-) */
#define MAX_VIEWS 2

/* Width 12 for a wee Quick (Hex) View */
#define MINWIDTH 12
#define MINHEIGHT 5

#define B_2LEFT B_USER
#define B_2RIGHT (B_USER + 1)
#define B_PLUS (B_USER + 2)
#define B_MINUS (B_USER + 3)

#define LAYOUT_OPTIONS_COUNT  G_N_ELEMENTS (check_options)
#define OTHER_OPTIONS_COUNT   (LAYOUT_OPTIONS_COUNT - 1)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static struct
{
    panel_view_mode_t type;
    Widget *widget;
    char *last_saved_dir;       /* last view_list working directory */
} panels[MAX_VIEWS] =
{
    /* *INDENT-OFF* */
    /* init MAX_VIEWS items */
    { view_listing, NULL, NULL},
    { view_listing, NULL, NULL}
    /* *INDENT-ON* */
};

/* These variables are used to avoid updating the information unless */
/* we need it */
static int old_first_panel_size;
static int old_horizontal_split;
static int old_output_lines;

/* Internal variables */
static int _horizontal_split;
static int _equal_split;
static int _first_panel_size;
static int _menubar_visible;
static int _output_lines;
static int _command_prompt;
static int _keybar_visible;
static int _message_visible;
static gboolean _xterm_title;
static int _free_space;

static int height;

static WRadio *radio_widget;

static struct
{
    const char *text;
    int *variable;
    WCheck *widget;
} check_options[] =
{
    /* *INDENT-OFF* */
    { N_("Show free sp&ace"), &free_space, NULL},
    { N_("&XTerm window title"), &xterm_title, NULL},
    { N_("H&intbar visible"), &mc_global.message_visible, NULL},
    { N_("&Keybar visible"), &mc_global.keybar_visible, NULL},
    { N_("Command &prompt"), &command_prompt, NULL},
    { N_("Menu&bar visible"), &menubar_visible, NULL},
    { N_("&Equal split"), &equal_split, NULL}
    /* *INDENT-ON* */
};

static const char *output_lines_label = NULL;
static int output_lines_label_len;

static WButton *bleft_widget, *bright_widget;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* don't use max() macro to avoid double call of str_term_width1() in widget width calculation */
#undef max

static int
max (int a, int b)
{
    return a > b ? a : b;
}

/* --------------------------------------------------------------------------------------------- */

static inline void
_check_split (void)
{
    if (_horizontal_split)
    {
        if (_equal_split)
            _first_panel_size = height / 2;
        else if (_first_panel_size < MINHEIGHT)
            _first_panel_size = MINHEIGHT;
        else if (_first_panel_size > height - MINHEIGHT)
            _first_panel_size = height - MINHEIGHT;
    }
    else
    {
        if (_equal_split)
            _first_panel_size = COLS / 2;
        else if (_first_panel_size < MINWIDTH)
            _first_panel_size = MINWIDTH;
        else if (_first_panel_size > COLS - MINWIDTH)
            _first_panel_size = COLS - MINWIDTH;
    }

    old_first_panel_size = _first_panel_size;
    old_horizontal_split = _horizontal_split;
}

/* --------------------------------------------------------------------------------------------- */

static void
update_split (const Dlg_head * h)
{
    /* Check split has to be done before testing if it changed, since
       it can change due to calling _check_split() as well */
    _check_split ();

    tty_setcolor (check_options[6].widget->state & C_BOOL ? DISABLED_COLOR : COLOR_NORMAL);

    dlg_move (h, 6, 5);
    tty_printf ("%03d", _first_panel_size);

    dlg_move (h, 6, 17);
    if (_horizontal_split)
        tty_printf ("%03d", height - _first_panel_size);
    else
        tty_printf ("%03d", COLS - _first_panel_size);

    dlg_move (h, 6, 12);
    tty_print_char ('=');
}

/* --------------------------------------------------------------------------------------------- */

static int
b_left_right_cback (WButton * button, int action)
{
    (void) action;

    if (button == bleft_widget)
        _first_panel_size++;
    else
        _first_panel_size--;

    update_split (button->widget.owner);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
bplus_cback (WButton * button, int action)
{
    (void) button;
    (void) action;

    if (_output_lines < 99)
        _output_lines++;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
bminus_cback (WButton * button, int action)
{
    (void) button;
    (void) action;

    if (_output_lines > 0)
        _output_lines--;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
layout_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case DLG_DRAW:
        /*When repainting the whole dialog (e.g. with C-l) we have to
           update everything */
        common_dialog_repaint (h);

        old_first_panel_size = -1;
        old_horizontal_split = -1;
        old_output_lines = -1;

        update_split (h);

        if (old_output_lines != _output_lines)
        {
            old_output_lines = _output_lines;
            tty_setcolor (mc_global.tty.console_flag != '\0' ? COLOR_NORMAL : DISABLED_COLOR);
            dlg_move (h, 9, 5);
            tty_print_string (output_lines_label);
            dlg_move (h, 9, 5 + 3 + output_lines_label_len);
            tty_printf ("%02d", _output_lines);
        }
        return MSG_HANDLED;

    case DLG_POST_KEY:
        _menubar_visible = check_options[5].widget->state & C_BOOL;
        _command_prompt = check_options[4].widget->state & C_BOOL;
        _keybar_visible = check_options[3].widget->state & C_BOOL;
        _message_visible = check_options[2].widget->state & C_BOOL;
        _xterm_title = check_options[1].widget->state & C_BOOL;
        _free_space = check_options[0].widget->state & C_BOOL;

        if (mc_global.tty.console_flag != '\0')
        {
            int minimum;
            if (_output_lines < 0)
                _output_lines = 0;
            height = LINES - _keybar_visible - _command_prompt -
                _menubar_visible - _output_lines - _message_visible;
            minimum = MINHEIGHT * (1 + _horizontal_split);
            if (height < minimum)
            {
                _output_lines -= minimum - height;
                height = minimum;
            }
        }
        else
            height = LINES - _keybar_visible - _command_prompt -
                _menubar_visible - _output_lines - _message_visible;

        if (old_output_lines != _output_lines)
        {
            old_output_lines = _output_lines;
            tty_setcolor (mc_global.tty.console_flag != '\0' ? COLOR_NORMAL : DISABLED_COLOR);
            dlg_move (h, 9, 5 + 3 + output_lines_label_len);
            tty_printf ("%02d", _output_lines);
        }
        return MSG_HANDLED;

    case DLG_ACTION:
        if (sender == (Widget *) radio_widget)
        {
            if (_horizontal_split != radio_widget->sel)
            {
                _horizontal_split = radio_widget->sel;
                if (_equal_split)
                {
                    if (_horizontal_split)
                        _first_panel_size = height / 2;
                    else
                        _first_panel_size = COLS / 2;
                }
            }

            update_split (h);

            return MSG_HANDLED;
        }

        if (sender == (Widget *) check_options[6].widget)
        {
            _equal_split = check_options[6].widget->state & C_BOOL;

            widget_disable (bleft_widget->widget, _equal_split);
            send_message ((Widget *) bleft_widget, WIDGET_DRAW, 0);
            widget_disable (bright_widget->widget, _equal_split);
            send_message ((Widget *) bright_widget, WIDGET_DRAW, 0);

            update_split (h);

            return MSG_HANDLED;
        }

        return MSG_NOT_HANDLED;

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static Dlg_head *
init_layout (void)
{
    Dlg_head *layout_dlg;
    int l1 = 0, width;
    int b1, b2, b;
    size_t i;

    const char *title1 = N_("Panel split");
    const char *title2 = N_("Console output");
    const char *title3 = N_("Other options");

    const char *s_split_direction[2] = {
        N_("&Vertical"),
        N_("&Horizontal")
    };

    const char *ok_button = N_("&OK");
    const char *cancel_button = N_("&Cancel");

    output_lines_label = _("Output lines:");

    /* save old params */
    _equal_split = equal_split;
    _menubar_visible = menubar_visible;
    _command_prompt = command_prompt;
    _keybar_visible = mc_global.keybar_visible;
    _message_visible = mc_global.message_visible;
    _xterm_title = xterm_title;
    _free_space = free_space;
    old_first_panel_size = -1;
    old_horizontal_split = -1;
    old_output_lines = -1;
    _first_panel_size = first_panel_size;
    _output_lines = output_lines;

#ifdef ENABLE_NLS
    {
        static gboolean i18n = FALSE;

        title1 = _(title1);
        title2 = _(title2);
        title3 = _(title3);

        i = G_N_ELEMENTS (s_split_direction);
        while (i-- != 0)
            s_split_direction[i] = _(s_split_direction[i]);

        if (!i18n)
        {
            for (i = 0; i < (size_t) LAYOUT_OPTIONS_COUNT; i++)
                check_options[i].text = _(check_options[i].text);
            i18n = TRUE;
        }

        ok_button = _(ok_button);
        cancel_button = _(cancel_button);
    }
#endif

    /* radiobuttons */
    i = G_N_ELEMENTS (s_split_direction);
    while (i-- != 0)
        l1 = max (l1, str_term_width1 (s_split_direction[i]) + 7);
    /* checkboxes */
    for (i = 0; i < (size_t) LAYOUT_OPTIONS_COUNT; i++)
        l1 = max (l1, str_term_width1 (check_options[i].text) + 7);
    /* groupboxes */
    l1 = max (l1, str_term_width1 (title1) + 4);
    l1 = max (l1, str_term_width1 (title2) + 4);
    l1 = max (l1, str_term_width1 (title3) + 4);
    /* label + "+"/"-" buttons */
    output_lines_label_len = str_term_width1 (output_lines_label);
    l1 = max (l1, output_lines_label_len + 12);
    /* buttons */
    b1 = str_term_width1 (ok_button) + 5;       /* default button */
    b2 = str_term_width1 (cancel_button) + 3;
    b = b1 + b2 + 1;
    /* dialog width */
    width = max (l1 * 2 + 7, b);

    layout_dlg =
        create_dlg (TRUE, 0, 0, 14, width,
                    dialog_colors, layout_callback, "[Layout]",
                    _("Layout"), DLG_CENTER | DLG_REVERSE);

    /* buttons */
    add_widget (layout_dlg,
                button_new (11, (width - b) / 3 * 2 + b1 + 1, B_CANCEL, NORMAL_BUTTON,
                            cancel_button, 0));
    add_widget (layout_dlg,
                button_new (11, (width - b) / 3, B_ENTER, DEFPUSH_BUTTON, ok_button, 0));

#define XTRACT(i) *check_options[i].variable, check_options[i].text

    /* "Other options" groupbox */
    for (i = 0; i < (size_t) OTHER_OPTIONS_COUNT; i++)
    {
        check_options[i].widget = check_new (OTHER_OPTIONS_COUNT - i + 2, 6 + l1, XTRACT (i));
        add_widget (layout_dlg, check_options[i].widget);
    }

    add_widget (layout_dlg, groupbox_new (2, 4 + l1, 9, l1, title3));

    /* "Console output" groupbox */
    {
        const int disabled = mc_global.tty.console_flag != '\0' ? 0 : W_DISABLED;
        Widget *w;

        w = (Widget *) button_new (9, output_lines_label_len + 5 + 5, B_MINUS,
                                   NARROW_BUTTON, "&-", bminus_cback);
        w->options |= disabled;
        add_widget (layout_dlg, w);

        w = (Widget *) button_new (9, output_lines_label_len + 5, B_PLUS,
                                   NARROW_BUTTON, "&+", bplus_cback);
        w->options |= disabled;
        add_widget (layout_dlg, w);

        w = (Widget *) groupbox_new (8, 3, 3, l1, title2);
        w->options |= disabled;
        add_widget (layout_dlg, w);
    }


    /* "Panel split" groupbox */
    bright_widget = button_new (6, 14, B_2RIGHT, NARROW_BUTTON, "&>", b_left_right_cback);
    widget_disable (bright_widget->widget, _equal_split);
    add_widget (layout_dlg, bright_widget);

    bleft_widget = button_new (6, 8, B_2LEFT, NARROW_BUTTON, "&<", b_left_right_cback);
    widget_disable (bleft_widget->widget, _equal_split);
    add_widget (layout_dlg, bleft_widget);

    check_options[6].widget = check_new (5, 5, XTRACT (6));
    add_widget (layout_dlg, check_options[6].widget);

    radio_widget = radio_new (3, 5, 2, s_split_direction);
    radio_widget->sel = horizontal_split;
    add_widget (layout_dlg, radio_widget);

    add_widget (layout_dlg, groupbox_new (2, 3, 6, l1, title1));

#undef XTRACT

    return layout_dlg;
}

/* --------------------------------------------------------------------------------------------- */

static void
check_split (void)
{
    if (horizontal_split)
    {
        if (equal_split)
            first_panel_size = height / 2;
        else if (first_panel_size < MINHEIGHT)
            first_panel_size = MINHEIGHT;
        else if (first_panel_size > height - MINHEIGHT)
            first_panel_size = height - MINHEIGHT;
    }
    else
    {
        if (equal_split)
            first_panel_size = COLS / 2;
        else if (first_panel_size < MINWIDTH)
            first_panel_size = MINWIDTH;
        else if (first_panel_size > COLS - MINWIDTH)
            first_panel_size = COLS - MINWIDTH;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_do_cols (int idx)
{
    if (get_display_type (idx) == view_listing)
        set_panel_formats ((WPanel *) panels[idx].widget);
    else
        panel_update_cols (panels[idx].widget, frame_half);
}

/* --------------------------------------------------------------------------------------------- */
/** Save current list_view widget directory into panel */

static Widget *
restore_into_right_dir_panel (int idx, Widget * from_widget)
{
    Widget *new_widget = NULL;
    const char *saved_dir = panels[idx].last_saved_dir;
    gboolean last_was_panel = (from_widget && get_display_type (idx) != view_listing);
    const char *p_name = get_nth_panel_name (idx);

    if (last_was_panel)
        new_widget = (Widget *) panel_new_with_dir (p_name, saved_dir);
    else
        new_widget = (Widget *) panel_new (p_name);

    return new_widget;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
layout_change (void)
{
    setup_panels ();
    /* update the main menu, because perhaps there was a change in the way
       how the panel are split (horizontal/vertical),
       and a change of menu visibility. */
    update_menu ();
    load_hint (1);
}

/* --------------------------------------------------------------------------------------------- */

void
layout_box (void)
{
    Dlg_head *layout_dlg;
    gboolean layout_do_change = FALSE;

    layout_dlg = init_layout ();

    if (run_dlg (layout_dlg) == B_ENTER)
    {
        size_t i;
        for (i = 0; i < (size_t) LAYOUT_OPTIONS_COUNT; i++)
            if (check_options[i].widget != NULL)
                *check_options[i].variable = check_options[i].widget->state & C_BOOL;
        horizontal_split = radio_widget->sel;
        first_panel_size = _first_panel_size;
        output_lines = _output_lines;
        layout_do_change = TRUE;
    }

    destroy_dlg (layout_dlg);
    if (layout_do_change)
        layout_change ();
}

/* --------------------------------------------------------------------------------------------- */

void
setup_panels (void)
{
    int start_y;
    int promptl;                /* the prompt len */

    if (mc_global.tty.console_flag != '\0')
    {
        int minimum;
        if (output_lines < 0)
            output_lines = 0;
        height =
            LINES - mc_global.keybar_visible - command_prompt - menubar_visible -
            output_lines - mc_global.message_visible;
        minimum = MINHEIGHT * (1 + horizontal_split);
        if (height < minimum)
        {
            output_lines -= minimum - height;
            height = minimum;
        }
    }
    else
    {
        height =
            LINES - menubar_visible - command_prompt - mc_global.keybar_visible -
            mc_global.message_visible;
    }
    check_split ();
    start_y = menubar_visible;

    /* The column computing is defered until panel_do_cols */
    if (horizontal_split)
    {
        widget_set_size (panels[0].widget, start_y, 0, first_panel_size, 0);

        widget_set_size (panels[1].widget, start_y + first_panel_size, 0,
                         height - first_panel_size, 0);
    }
    else
    {
        int first_x = first_panel_size;

        widget_set_size (panels[0].widget, start_y, 0, height, 0);

        widget_set_size (panels[1].widget, start_y, first_x, height, 0);

    }
    panel_do_cols (0);
    panel_do_cols (1);

    promptl = str_term_width1 (mc_prompt);

    widget_set_size (&the_menubar->widget, 0, 0, 1, COLS);

    if (command_prompt)
    {
        widget_set_size (&cmdline->widget, LINES - 1 - mc_global.keybar_visible, promptl, 1,
                         COLS - promptl);
        input_set_origin (cmdline, promptl, COLS - promptl);
        widget_set_size (&the_prompt->widget, LINES - 1 - mc_global.keybar_visible, 0, 1, promptl);
    }
    else
    {
        widget_set_size (&cmdline->widget, 0, 0, 0, 0);
        input_set_origin (cmdline, 0, 0);
        widget_set_size (&the_prompt->widget, LINES, COLS, 0, 0);
    }

    widget_set_size (&the_bar->widget, LINES - 1, 0, mc_global.keybar_visible, COLS);
    buttonbar_set_visible (the_bar, mc_global.keybar_visible);

    /* Output window */
    if (mc_global.tty.console_flag != '\0' && output_lines)
    {
        output_start_y = LINES - command_prompt - mc_global.keybar_visible - output_lines;
        show_console_contents (output_start_y,
                               LINES - output_lines - mc_global.keybar_visible - 1,
                               LINES - mc_global.keybar_visible - 1);
    }
    if (mc_global.message_visible)
        widget_set_size (&the_hint->widget, height + start_y, 0, 1, COLS);
    else
        widget_set_size (&the_hint->widget, 0, 0, 0, 0);

    update_xterm_title_path ();
}

/* --------------------------------------------------------------------------------------------- */

void
use_dash (gboolean flag)
{
    if (flag)
        ok_to_refresh++;
    else
        ok_to_refresh--;
}

/* --------------------------------------------------------------------------------------------- */

void
set_hintbar (const char *str)
{
    label_set_text (the_hint, str);
    if (ok_to_refresh > 0)
        mc_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

void
rotate_dash (void)
{
    static const char rotating_dash[] = "|/-\\";
    static size_t pos = 0;

    if (!nice_rotating_dash || (ok_to_refresh <= 0))
        return;

    if (pos >= sizeof (rotating_dash) - 1)
        pos = 0;
    tty_gotoyx (0, COLS - 1);
    tty_setcolor (NORMAL_COLOR);
    tty_print_char (rotating_dash[pos]);
    mc_refresh ();
    pos++;
}

/* --------------------------------------------------------------------------------------------- */

const char *
get_nth_panel_name (int num)
{
    static char buffer[BUF_SMALL];

    if (!num)
        return "New Left Panel";
    else if (num == 1)
        return "New Right Panel";
    else
    {
        g_snprintf (buffer, sizeof (buffer), "%ith Panel", num);
        return buffer;
    }
}

/* --------------------------------------------------------------------------------------------- */
/* I wonder if I should start to use the folding mode than Dugan uses */
/*                                                                     */
/* This is the centralized managing of the panel display types         */
/* This routine takes care of destroying and creating new widgets      */
/* Please note that it could manage MAX_VIEWS, not just left and right */
/* Currently nothing in the code takes advantage of this and has hard- */
/* coded values for two panels only                                    */

/* Set the num-th panel to the view type: type */
/* This routine also keeps at least one WPanel object in the screen */
/* since a lot of routines depend on the current_panel variable */

void
set_display_type (int num, panel_view_mode_t type)
{
    int x = 0, y = 0, cols = 0, lines = 0;
    unsigned int the_other = 0; /* Index to the other panel */
    const char *file_name = NULL;       /* For Quick view */
    Widget *new_widget = NULL, *old_widget = NULL;
    panel_view_mode_t old_type;
    WPanel *the_other_panel = NULL;

    if (num >= MAX_VIEWS)
    {
        fprintf (stderr, "Cannot allocate more that %d views\n", MAX_VIEWS);
        abort ();
    }
    /* Check that we will have a WPanel * at least */
    if (type != view_listing)
    {
        the_other = num == 0 ? 1 : 0;

        if (panels[the_other].type != view_listing)
            return;
    }

    /* Get rid of it */
    if (panels[num].widget != NULL)
    {
        Widget *w = panels[num].widget;
        WPanel *panel = (WPanel *) w;

        x = w->x;
        y = w->y;
        cols = w->cols;
        lines = w->lines;
        old_widget = w;
        old_type = panels[num].type;

        if (old_type == view_listing && panel->frame_size == frame_full && type != view_listing)
        {
            cols = COLS - first_panel_size;
            if (num == 1)
                x = first_panel_size;
        }
    }

    /* Restoring saved path from panels.ini for nonlist panel */
    /* when it's first creation (for example view_info) */
    if (old_widget == NULL && type != view_listing)
    {
        char panel_dir[MC_MAXPATHLEN];
        mc_get_current_wd (panel_dir, sizeof (panel_dir));
        panels[num].last_saved_dir = g_strdup (panel_dir);
    }

    switch (type)
    {
    case view_nothing:
    case view_listing:
        new_widget = restore_into_right_dir_panel (num, old_widget);
        widget_set_size (new_widget, y, x, lines, cols);
        break;

    case view_info:
        new_widget = (Widget *) info_new (y, x, lines, cols);
        break;

    case view_tree:
        new_widget = (Widget *) tree_new (y, x, lines, cols, TRUE);
        break;

    case view_quick:
        new_widget = (Widget *) mcview_new (y, x, lines, cols, TRUE);
        the_other_panel = (WPanel *) panels[the_other].widget;
        if (the_other_panel != NULL)
            file_name = the_other_panel->dir.list[the_other_panel->selected].fname;
        else
            file_name = "";

        mcview_load ((struct mcview_struct *) new_widget, 0, file_name, 0);
        break;
    }

    if (type != view_listing)
        /* Must save dir, for restoring after change type to */
        /* view_listing */
        save_panel_dir (num);

    panels[num].type = type;
    panels[num].widget = new_widget;

    /* We use replace to keep the circular list of the dialog in the */
    /* same state.  Maybe we could just kill it and then replace it  */
    if ((midnight_dlg != NULL) && (old_widget != NULL))
    {
        if (old_type == view_listing)
        {
            /* save and write directory history of panel
             * ... and other histories of midnight_dlg  */
            dlg_save_history (midnight_dlg);
        }

        dlg_replace_widget (old_widget, new_widget);
    }

    if (type == view_listing)
    {
        WPanel *panel = (WPanel *) new_widget;

        /* if existing panel changed type to view_listing, then load history */
        if (old_widget != NULL)
        {
            ev_history_load_save_t event_data = { NULL, new_widget };

            mc_event_raise (midnight_dlg->event_group, MCEVENT_HISTORY_LOAD, &event_data);
        }

        if (num == 0)
            left_panel = panel;
        else
            right_panel = panel;

        /* forced update format after set new sizes */
        set_panel_formats (panel);
    }

    if (type == view_tree)
        the_tree = (WTree *) new_widget;

    /* Prevent current_panel's value from becoming invalid.
     * It's just a quick hack to prevent segfaults. Comment out and
     * try following:
     * - select left panel
     * - invoke menue left/tree
     * - as long as you stay in the left panel almost everything that uses
     *   current_panel causes segfault, e.g. C-Enter, C-x c, ...
     */
    if ((type != view_listing) && (current_panel == (WPanel *) old_widget))
        current_panel = num == 0 ? right_panel : left_panel;

    g_free (old_widget);
}

/* --------------------------------------------------------------------------------------------- */

void
panel_update_cols (Widget * widget, panel_display_t frame_size)
{
    int cols, origin;

    /* don't touch panel if it is not in dialog yet */
    /* if panel is not in dialog it is not in widgets list
       and cannot be compared with get_panel_widget() result */
    if (widget->owner == NULL)
        return;

    if (horizontal_split)
    {
        widget->cols = COLS;
        return;
    }

    if (frame_size == frame_full)
    {
        cols = COLS;
        origin = 0;
    }
    else if (widget == get_panel_widget (0))
    {
        cols = first_panel_size;
        origin = 0;
    }
    else
    {
        cols = COLS - first_panel_size;
        origin = first_panel_size;
    }

    widget->cols = cols;
    widget->x = origin;
}

/* --------------------------------------------------------------------------------------------- */
/** This routine is deeply sticked to the two panels idea.
   What should it do in more panels. ANSWER - don't use it
   in any multiple panels environment. */

void
swap_panels (void)
{
    WPanel *panel1, *panel2;
    Widget *tmp_widget;

    panel1 = (WPanel *) panels[0].widget;
    panel2 = (WPanel *) panels[1].widget;

    if (panels[0].type == view_listing && panels[1].type == view_listing &&
        !mc_config_get_bool (mc_main_config, CONFIG_PANELS_SECTION, "simple_swap", FALSE))
    {
        WPanel panel;

#define panelswap(x) panel.x = panel1->x; panel1->x = panel2->x; panel2->x = panel.x;

#define panelswapstr(e) strcpy (panel.e, panel1->e); \
                        strcpy (panel1->e, panel2->e); \
                        strcpy (panel2->e, panel.e);
        /* Change content and related stuff */
        panelswap (dir);
        panelswap (active);
        panelswapstr (cwd);
        panelswapstr (lwd);
        panelswap (count);
        panelswap (marked);
        panelswap (dirs_marked);
        panelswap (total);
        panelswap (top_file);
        panelswap (selected);
        panelswap (is_panelized);
        panelswap (dir_stat);
#undef panelswapstr
#undef panelswap

        panel1->searching = FALSE;
        panel2->searching = FALSE;

        if (current_panel == panel1)
            current_panel = panel2;
        else
            current_panel = panel1;

        /* if sort options are different -> resort panels */
        if (memcmp (&panel1->sort_info, &panel2->sort_info, sizeof (panel_sort_info_t)) != 0)
        {
            panel_re_sort (other_panel);
            panel_re_sort (current_panel);
        }

        if (dlg_widget_active (panels[0].widget))
            dlg_select_widget (panels[1].widget);
        else if (dlg_widget_active (panels[1].widget))
            dlg_select_widget (panels[0].widget);
    }
    else
    {
        WPanel *tmp_panel;
        int x, y, cols, lines;
        int tmp_type;

        tmp_panel = right_panel;
        right_panel = left_panel;
        left_panel = tmp_panel;

        if (panels[0].type == view_listing)
        {
            if (strcmp (panel1->panel_name, get_nth_panel_name (0)) == 0)
            {
                g_free (panel1->panel_name);
                panel1->panel_name = g_strdup (get_nth_panel_name (1));
            }
        }
        if (panels[1].type == view_listing)
        {
            if (strcmp (panel2->panel_name, get_nth_panel_name (1)) == 0)
            {
                g_free (panel2->panel_name);
                panel2->panel_name = g_strdup (get_nth_panel_name (0));
            }
        }

        x = panels[0].widget->x;
        y = panels[0].widget->y;
        cols = panels[0].widget->cols;
        lines = panels[0].widget->lines;

        panels[0].widget->x = panels[1].widget->x;
        panels[0].widget->y = panels[1].widget->y;
        panels[0].widget->cols = panels[1].widget->cols;
        panels[0].widget->lines = panels[1].widget->lines;

        panels[1].widget->x = x;
        panels[1].widget->y = y;
        panels[1].widget->cols = cols;
        panels[1].widget->lines = lines;

        tmp_widget = panels[0].widget;
        panels[0].widget = panels[1].widget;
        panels[1].widget = tmp_widget;
        tmp_type = panels[0].type;
        panels[0].type = panels[1].type;
        panels[1].type = tmp_type;

        /* force update formats because of possible changed sizes */
        if (panels[0].type == view_listing)
            set_panel_formats ((WPanel *) panels[0].widget);
        if (panels[1].type == view_listing)
            set_panel_formats ((WPanel *) panels[1].widget);
    }
}

/* --------------------------------------------------------------------------------------------- */

panel_view_mode_t
get_display_type (int idx)
{
    return panels[idx].type;
}

/* --------------------------------------------------------------------------------------------- */

struct Widget *
get_panel_widget (int idx)
{
    return panels[idx].widget;
}

/* --------------------------------------------------------------------------------------------- */

int
get_current_index (void)
{
    if (panels[0].widget == ((Widget *) current_panel))
        return 0;
    else
        return 1;
}

/* --------------------------------------------------------------------------------------------- */

int
get_other_index (void)
{
    return !get_current_index ();
}

/* --------------------------------------------------------------------------------------------- */

struct WPanel *
get_other_panel (void)
{
    return (struct WPanel *) get_panel_widget (get_other_index ());
}

/* --------------------------------------------------------------------------------------------- */
/** Returns the view type for the current panel/view */

panel_view_mode_t
get_current_type (void)
{
    if (panels[0].widget == (Widget *) current_panel)
        return panels[0].type;
    else
        return panels[1].type;
}

/* --------------------------------------------------------------------------------------------- */
/** Returns the view type of the unselected panel */

panel_view_mode_t
get_other_type (void)
{
    if (panels[0].widget == (Widget *) current_panel)
        return panels[1].type;
    else
        return panels[0].type;
}

/* --------------------------------------------------------------------------------------------- */
/** Save current list_view widget directory into panel */

void
save_panel_dir (int idx)
{
    panel_view_mode_t type = get_display_type (idx);
    Widget *widget = get_panel_widget (idx);

    if ((type == view_listing) && (widget != NULL))
    {
        WPanel *w = (WPanel *) widget;
        char *widget_work_dir = w->cwd;

        g_free (panels[idx].last_saved_dir);    /* last path no needed */
        /* Because path can be nonlocal */
        panels[idx].last_saved_dir = g_strdup (widget_work_dir);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Return working dir, if it's view_listing - cwd,
   but for other types - last_saved_dir */

const char *
get_panel_dir_for (const WPanel * widget)
{
    int i;

    for (i = 0; i < MAX_VIEWS; i++)
        if ((WPanel *) get_panel_widget (i) == widget)
            break;

    if (i >= MAX_VIEWS)
        return ".";

    if (get_display_type (i) == view_listing)
        return ((WPanel *) get_panel_widget (i))->cwd;

    return panels[i].last_saved_dir;
}

/* --------------------------------------------------------------------------------------------- */
