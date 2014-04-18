/*
   Editor spell checker dialogs

   Copyright (C) 2012-2014
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2012
   Andrew Borodin <aborodin@vmail.ru>, 2013

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

#include <config.h>

#include "lib/global.h"
#include "lib/strutil.h"        /* str_term_width1 */
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "editwidget.h"

#include "event.h"
#include "spell.h"
#include "spell_dialogs.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
edit_suggest_current_word (WEdit * edit)
{
    gsize cut_len = 0;
    gsize word_len = 0;
    off_t word_start = 0;
    int retval = B_SKIP_WORD;
    GString *match_word;

    /* search start of word to spell check */
    match_word = edit_buffer_get_word_from_pos (&edit->buffer, edit->buffer.curs1, &word_start,
                                                &cut_len);
    word_len = match_word->len;

#ifdef HAVE_CHARSET
    if (mc_global.source_codepage >= 0 && (mc_global.source_codepage != mc_global.display_codepage))
    {
        GString *tmp_word;

        tmp_word = str_convert_to_display (match_word->str);
        g_string_free (match_word, TRUE);
        match_word = tmp_word;
    }
#endif
    if (!aspell_check (match_word->str, (int) word_len))
    {
        GArray *suggest;
        unsigned int res;

        suggest = g_array_new (TRUE, FALSE, sizeof (char *));

        res = aspell_suggest (suggest, match_word->str, (int) word_len);
        if (res != 0)
        {
            char *new_word = NULL;

            edit->found_start = word_start;
            edit->found_len = word_len;
            edit->force |= REDRAW_PAGE;
            edit_scroll_screen_over_cursor (edit);
            edit_render_keypress (edit);

            retval = spell_dialog_spell_suggest_show (edit, match_word->str, &new_word, suggest);
            edit_cursor_move (edit, word_len - cut_len);

            if (retval == B_ENTER && new_word != NULL)
            {
                guint i;
                char *cp_word;

#ifdef HAVE_CHARSET
                if (mc_global.source_codepage >= 0 &&
                    (mc_global.source_codepage != mc_global.display_codepage))
                {
                    GString *tmp_word;

                    tmp_word = str_convert_to_input (new_word);
                    g_free (new_word);
                    new_word = g_string_free (tmp_word, FALSE);
                }
#endif
                cp_word = new_word;
                for (i = 0; i < word_len; i++)
                    edit_backspace (edit, TRUE);
                for (; *new_word; new_word++)
                    edit_insert (edit, *new_word);
                g_free (cp_word);
            }
            else if (retval == B_ADD_WORD && match_word != NULL)
                aspell_add_to_dict (match_word->str, (int) word_len);
        }

        g_array_free (suggest, TRUE);
        edit->found_start = 0;
        edit->found_len = 0;
    }
    g_string_free (match_word, TRUE);
    return retval;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* event callback */

gboolean
mc_editor_cmd_spell_suggest_word (event_info_t * event_info, gpointer data, GError ** error)
{
    WEdit *edit = (WEdit *) data;

    (void) event_info;
    (void) error;

    edit_suggest_current_word (edit);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

gboolean
mc_editor_cmd_spell_check (event_info_t * event_info, gpointer data, GError ** error)
{
    WEdit *edit = (WEdit *) data;

    (void) event_info;
    (void) error;

    if (edit->buffer.curs_line > 0)
    {
        edit_cursor_move (edit, -edit->buffer.curs1);
        edit_move_to_prev_col (edit, 0);
        edit_update_curs_row (edit);
    }

    do
    {
        int c1, c2;

        c2 = edit_buffer_get_current_byte (&edit->buffer);

        do
        {
            if (edit->buffer.curs1 >= edit->buffer.size)
                return TRUE;

            c1 = c2;
            edit_cursor_move (edit, 1);
            c2 = edit_buffer_get_current_byte (&edit->buffer);
        }
        while (is_break_char (c1) || is_break_char (c2));
    }
    while (edit_suggest_current_word (edit) != B_CANCEL);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

gboolean
mc_editor_cmd_spell_set_language (event_info_t * event_info, gpointer data, GError ** error)
{
    GArray *lang_list;

    (void) event_info;
    (void) error;
    (void) data;


    lang_list = g_array_new (TRUE, FALSE, sizeof (char *));
    if (aspell_get_lang_list (lang_list) != 0)
    {
        char *lang;

        lang = spell_dialog_lang_list_show (lang_list);
        if (lang != NULL)
            (void) aspell_set_lang (lang);
    }
    aspell_array_clean (lang_list);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
