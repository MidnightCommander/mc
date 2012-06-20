/*
   Editor options dialog box

   Copyright (C) 1996, 1997, 1998, 2001, 2002, 2003, 2004, 2005, 2007, 2011,
   2012
   The Free Software Foundation, Inc.

   Written by:
   Paul Sheer, 1996, 1997
   Andrew Borodin <aborodin@vmail.ru> 2012

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

/** \file
 *  \brief Source: editor options dialog box
 *  \author Paul Sheer
 *  \date 1996, 1997
 */

#include <config.h>

#include <stdlib.h>             /* atoi(), NULL */

#include "lib/global.h"
#include "lib/widget.h"

#include "editwidget.h"
#include "edit-impl.h"
#include "src/setup.h"          /* option_tab_spacing */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define OPT_DLG_H 17
#define OPT_DLG_W 74

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static const char *wrap_str[] = {
    N_("None"),
    N_("Dynamic paragraphing"),
    N_("Type writer wrap"),
    NULL
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_NLS
static void
i18n_translate_array (const char *array[])
{
    while (*array != NULL)
    {
        *array = _(*array);
        array++;
    }
}
#endif /* ENABLE_NLS */
/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for the iteration of objects in the 'editors' array.
 * Tear down 'over_col' property in all editors.
 *
 * @param data      probably WEdit object
 * @param user_data unused
 */

static void
edit_reset_over_col (void *data, void *user_data)
{
    (void) user_data;

    if (edit_widget_is_editor ((const Widget *) data))
        ((WEdit *) data)->over_col = 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Callback for the iteration of objects in the 'editors' array.
 * Reload syntax lighlighting in all editors.
 *
 * @param data      probably WEdit object
 * @param user_data unused
 */

static void
edit_reload_syntax (void *data, void *user_data)
{
    (void) user_data;

    if (edit_widget_is_editor (WIDGET (data)))
    {
        WEdit *edit = (WEdit *) data;
        edit_load_syntax (edit, NULL, edit->syntax_type);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
edit_options_dialog (Dlg_head * h)
{
    char wrap_length[16], tab_spacing[16], *p, *q;
    int wrap_mode = 0;
    int old_syntax_hl;

    QuickWidget quick_widgets[] = {
        /*  0 */ QUICK_BUTTON (6, 10, OPT_DLG_H - 3, OPT_DLG_H, N_("&Cancel"), B_CANCEL, NULL),
        /*  1 */ QUICK_BUTTON (2, 10, OPT_DLG_H - 3, OPT_DLG_H, N_("&OK"), B_ENTER, NULL),
        /*  2 */ QUICK_LABEL (OPT_DLG_W / 2 + 1, OPT_DLG_W, 12, OPT_DLG_H,
                              N_("Word wrap line length:")),
        /*  3 */ QUICK_INPUT (OPT_DLG_W / 2 + 25, OPT_DLG_W, 12, OPT_DLG_H,
                              wrap_length, OPT_DLG_W / 2 - 4 - 24, 0, "edit-word-wrap", &p),
        /*  4 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, 11, OPT_DLG_H,
                                 N_("&Group undo"), &option_group_undo),
        /*  5 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, 10, OPT_DLG_H,
                                 N_("Cursor beyond end of line"), &option_cursor_beyond_eol),
        /*  6 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, 9, OPT_DLG_H,
                                 N_("Pers&istent selection"), &option_persistent_selections),
        /*  7 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, 8, OPT_DLG_H,
                                 N_("Synta&x highlighting"), &option_syntax_highlighting),
        /*  8 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, 7, OPT_DLG_H,
                                 N_("Visible tabs"), &visible_tabs),
        /*  9 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, 6, OPT_DLG_H,
                                 N_("Visible trailing spaces"), &visible_tws),
        /* 10 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, 5, OPT_DLG_H,
                                 N_("Save file &position"), &option_save_position),
        /* 11 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, 4, OPT_DLG_H,
                                 N_("Confir&m before saving"), &edit_confirm_save),
        /* 12 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, 3, OPT_DLG_H,
                                 N_("&Return does autoindent"), &option_return_does_auto_indent),
        /* 13 */ QUICK_LABEL (3, OPT_DLG_W, 11, OPT_DLG_H, N_("Tab spacing:")),
        /* 14 */ QUICK_INPUT (3 + 24, OPT_DLG_W, 11, OPT_DLG_H,
                              tab_spacing, OPT_DLG_W / 2 - 4 - 24, 0, "edit-tab-spacing", &q),
        /* 15 */ QUICK_CHECKBOX (3, OPT_DLG_W, 10, OPT_DLG_H,
                                 N_("Fill tabs with &spaces"), &option_fill_tabs_with_spaces),
        /* 16 */ QUICK_CHECKBOX (3, OPT_DLG_W, 9, OPT_DLG_H,
                                 N_("&Backspace through tabs"), &option_backspace_through_tabs),
        /* 17 */ QUICK_CHECKBOX (3, OPT_DLG_W, 8, OPT_DLG_H,
                                 N_("&Fake half tabs"), &option_fake_half_tabs),
        /* 18 */ QUICK_RADIO (4, OPT_DLG_W, 4, OPT_DLG_H, 3, wrap_str, &wrap_mode),
        /* 19 */ QUICK_LABEL (3, OPT_DLG_W, 3, OPT_DLG_H, N_("Wrap mode")),
        QUICK_END
    };

    QuickDialog Quick_options = {
        OPT_DLG_W, OPT_DLG_H, -1, -1, N_("Editor options"),
        "[Editor options]", quick_widgets, NULL, NULL, FALSE
    };

#ifdef ENABLE_NLS
    static gboolean i18n_flag = FALSE;

    if (!i18n_flag)
    {
        i18n_translate_array (wrap_str);
        i18n_flag = TRUE;
    }
#endif /* ENABLE_NLS */

    g_snprintf (wrap_length, sizeof (wrap_length), "%d", option_word_wrap_line_length);
    g_snprintf (tab_spacing, sizeof (tab_spacing), "%d", option_tab_spacing);

    if (option_auto_para_formatting)
        wrap_mode = 1;
    else if (option_typewriter_wrap)
        wrap_mode = 2;
    else
        wrap_mode = 0;

    if (quick_dialog (&Quick_options) == B_CANCEL)
        return;

    old_syntax_hl = option_syntax_highlighting;

    if (!option_cursor_beyond_eol)
        g_list_foreach (h->widgets, edit_reset_over_col, NULL);

    if (p != NULL)
    {
        option_word_wrap_line_length = atoi (p);
        if (option_word_wrap_line_length <= 0)
            option_word_wrap_line_length = DEFAULT_WRAP_LINE_LENGTH;
        g_free (p);
    }

    if (q != NULL)
    {
        option_tab_spacing = atoi (q);
        if (option_tab_spacing <= 0)
            option_tab_spacing = DEFAULT_TAB_SPACING;
        g_free (q);
    }

    if (wrap_mode == 1)
    {
        option_auto_para_formatting = 1;
        option_typewriter_wrap = 0;
    }
    else if (wrap_mode == 2)
    {
        option_auto_para_formatting = 0;
        option_typewriter_wrap = 1;
    }
    else
    {
        option_auto_para_formatting = 0;
        option_typewriter_wrap = 0;
    }

    /* Load or unload syntax rules if the option has changed */
    if (option_syntax_highlighting != old_syntax_hl)
        g_list_foreach (h->widgets, edit_reload_syntax, NULL);
}

/* --------------------------------------------------------------------------------------------- */
