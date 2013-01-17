/*
   Editor spell checker dialogs

   Copyright (C) 2012
   The Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2012

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
#include "lib/widget.h"
#include "lib/tty/tty.h"        /* COLS, LINES */

#include "editwidget.h"

#include "spell.h"
#include "spell_dialogs.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Show suggests for the current word.
 *
 * @param edit Editor object
 * @param word Word for spell check
 * @param new_word Word to replace the incorrect word
 * @param suggest Array of suggests for current word
 * @return code of pressed button
 */

int
spell_dialog_spell_suggest_show (WEdit * edit, const char *word, char **new_word, GArray * suggest)
{

    int sug_dlg_h = 14;         /* dialog height */
    int sug_dlg_w = 29;         /* dialog width */
    int xpos, ypos;
    char *lang_label;
    char *word_label;
    unsigned int i;
    int res;
    char *curr = NULL;
    WDialog *sug_dlg;
    WListbox *sug_list;
    int max_btn_len = 0;
    int replace_len;
    int skip_len;
    int cancel_len;
    WButton *add_btn;
    WButton *replace_btn;
    WButton *skip_btn;
    WButton *cancel_button;
    int word_label_len;

    /* calculate the dialog metrics */
    xpos = (COLS - sug_dlg_w) / 2;
    ypos = (LINES - sug_dlg_h) * 2 / 3;

    /* Sometimes menu can hide replaced text. I don't like it */
    if ((edit->curs_row >= ypos - 1) && (edit->curs_row <= ypos + sug_dlg_h - 1))
        ypos -= sug_dlg_h;

    add_btn = button_new (5, 28, B_ADD_WORD, NORMAL_BUTTON, _("&Add word"), 0);
    replace_btn = button_new (7, 28, B_ENTER, NORMAL_BUTTON, _("&Replace"), 0);
    replace_len = button_get_len (replace_btn);
    skip_btn = button_new (9, 28, B_SKIP_WORD, NORMAL_BUTTON, _("&Skip"), 0);
    skip_len = button_get_len (skip_btn);
    cancel_button = button_new (11, 28, B_CANCEL, NORMAL_BUTTON, _("&Cancel"), 0);
    cancel_len = button_get_len (cancel_button);

    max_btn_len = max (replace_len, skip_len);
    max_btn_len = max (max_btn_len, cancel_len);

    lang_label = g_strdup_printf ("%s: %s", _("Language"), aspell_get_lang ());
    word_label = g_strdup_printf ("%s: %s", _("Misspelled"), word);
    word_label_len = str_term_width1 (word_label) + 5;

    sug_dlg_w += max_btn_len;
    sug_dlg_w = max (sug_dlg_w, word_label_len) + 1;

    sug_dlg = create_dlg (TRUE, ypos, xpos, sug_dlg_h, sug_dlg_w,
                          dialog_colors, NULL, NULL, "[ASpell]", _("Check word"), DLG_COMPACT);

    add_widget (sug_dlg, label_new (1, 2, lang_label));
    add_widget (sug_dlg, label_new (3, 2, word_label));

    add_widget (sug_dlg, groupbox_new (4, 2, sug_dlg_h - 5, 25, _("Suggest")));

    sug_list = listbox_new (5, 2, sug_dlg_h - 7, 24, FALSE, NULL);
    for (i = 0; i < suggest->len; i++)
        listbox_add_item (sug_list, LISTBOX_APPEND_AT_END, 0, g_array_index (suggest, char *, i),
                          NULL);
    add_widget (sug_dlg, sug_list);

    add_widget (sug_dlg, add_btn);
    add_widget (sug_dlg, replace_btn);
    add_widget (sug_dlg, skip_btn);
    add_widget (sug_dlg, cancel_button);

    res = run_dlg (sug_dlg);
    if (res == B_ENTER)
    {
        char *tmp = NULL;
        listbox_get_current (sug_list, &curr, NULL);

        if (curr != NULL)
            tmp = g_strdup (curr);
        *new_word = tmp;
    }

    destroy_dlg (sug_dlg);
    g_free (lang_label);
    g_free (word_label);

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Show dialog to select language for spell check.
 *
 * @param languages Array of available languages
 * @return name of choosed language
 */

char *
spell_dialog_lang_list_show (GArray * languages)
{

    int lang_dlg_h = 12;        /* dialog height */
    int lang_dlg_w = 30;        /* dialog width */
    char *selected_lang = NULL;
    unsigned int i;
    int res;
    Listbox *lang_list;

    /* Create listbox */
    lang_list = create_listbox_window_centered (-1, -1, lang_dlg_h, lang_dlg_w,
                                                _("Select language"), "[ASpell]");

    for (i = 0; i < languages->len; i++)
        LISTBOX_APPEND_TEXT (lang_list, 0, g_array_index (languages, char *, i), NULL);

    res = run_listbox (lang_list);
    if (res >= 0)
        selected_lang = g_strdup (g_array_index (languages, char *, (unsigned int) res));

    return selected_lang;

}

/* --------------------------------------------------------------------------------------------- */
