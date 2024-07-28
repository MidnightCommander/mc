/*
   Editor spell checker

   Copyright (C) 2012-2024
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2012
   Andrew Borodin <aborodin@vmail.ru>, 2013-2024


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

#include <stdlib.h>
#include <string.h>
#include <gmodule.h>
#include <aspell.h>

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif
#include "lib/strutil.h"
#include "lib/util.h"           /* MC_PTR_FREE() */
#include "lib/tty/tty.h"        /* COLS, LINES */

#include "src/setup.h"

#include "editwidget.h"

#include "spell.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define B_SKIP_WORD (B_USER+3)
#define B_ADD_WORD (B_USER+4)

#define ASPELL_FUNCTION_AVAILABLE(f) \
    g_module_symbol (spell_module, #f, (void *) &mc_##f)

/*** file scope type declarations ****************************************************************/

typedef struct aspell_struct
{
    AspellConfig *config;
    AspellSpeller *speller;
} spell_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static GModule *spell_module = NULL;
static spell_t *global_speller = NULL;

static AspellConfig *(*mc_new_aspell_config) (void);
static int (*mc_aspell_config_replace) (AspellConfig * ths, const char *key, const char *value);
static AspellCanHaveError *(*mc_new_aspell_speller) (AspellConfig * config);
static unsigned int (*mc_aspell_error_number) (const AspellCanHaveError * ths);
static const char *(*mc_aspell_speller_error_message) (const AspellSpeller * ths);
static const AspellError *(*mc_aspell_speller_error) (const AspellSpeller * ths);

static AspellSpeller *(*mc_to_aspell_speller) (AspellCanHaveError * obj);
static int (*mc_aspell_speller_check) (AspellSpeller * ths, const char *word, int word_size);
static const AspellWordList *(*mc_aspell_speller_suggest) (AspellSpeller * ths,
                                                           const char *word, int word_size);
static AspellStringEnumeration *(*mc_aspell_word_list_elements) (const struct AspellWordList * ths);
static const char *(*mc_aspell_config_retrieve) (AspellConfig * ths, const char *key);
static void (*mc_delete_aspell_speller) (AspellSpeller * ths);
static void (*mc_delete_aspell_config) (AspellConfig * ths);
static void (*mc_delete_aspell_can_have_error) (AspellCanHaveError * ths);
static const char *(*mc_aspell_error_message) (const AspellCanHaveError * ths);
static void (*mc_delete_aspell_string_enumeration) (AspellStringEnumeration * ths);
static AspellDictInfoEnumeration *(*mc_aspell_dict_info_list_elements)
    (const AspellDictInfoList * ths);
static AspellDictInfoList *(*mc_get_aspell_dict_info_list) (AspellConfig * config);
static const AspellDictInfo *(*mc_aspell_dict_info_enumeration_next)
    (AspellDictInfoEnumeration * ths);
static const char *(*mc_aspell_string_enumeration_next) (AspellStringEnumeration * ths);
static void (*mc_delete_aspell_dict_info_enumeration) (AspellDictInfoEnumeration * ths);
static unsigned int (*mc_aspell_word_list_size) (const AspellWordList * ths);
static const AspellError *(*mc_aspell_error) (const AspellCanHaveError * ths);
static int (*mc_aspell_speller_add_to_personal) (AspellSpeller * ths, const char *word,
                                                 int word_size);
static int (*mc_aspell_speller_save_all_word_lists) (AspellSpeller * ths);

static struct
{
    const char *code;
    const char *name;
} spell_codes_map[] = {
    /* *INDENT-OFF* */
    {"br", N_("Breton")},
    {"cs", N_("Czech")},
    {"cy", N_("Welsh")},
    {"da", N_("Danish")},
    {"de", N_("German")},
    {"el", N_("Greek")},
    {"en", N_("English")},
    {"en_GB", N_("British English")},
    {"en_CA", N_("Canadian English")},
    {"en_US", N_("American English")},
    {"eo", N_("Esperanto")},
    {"es", N_("Spanish")},
    {"fo", N_("Faroese")},
    {"fr", N_("French")},
    {"it", N_("Italian")},
    {"nl", N_("Dutch")},
    {"no", N_("Norwegian")},
    {"pl", N_("Polish")},
    {"pt", N_("Portuguese")},
    {"ro", N_("Romanian")},
    {"ru", N_("Russian")},
    {"sk", N_("Slovak")},
    {"sv", N_("Swedish")},
    {"uk", N_("Ukrainian")},
    {NULL, NULL}
    /* *INDENT-ON* */
};

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Found the language name by language code. For example: en_US -> American English.
 *
 * @param code Short name of the language (ru, en, pl, uk, etc...)
 * @return language name
 */

static const char *
spell_decode_lang (const char *code)
{
    size_t i;

    for (i = 0; spell_codes_map[i].code != NULL; i++)
    {
        if (strcmp (spell_codes_map[i].code, code) == 0)
            return _(spell_codes_map[i].name);
    }

    return code;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Checks if aspell library and symbols are available.
 *
 * @return FALSE or error
 */

static gboolean
spell_available (void)
{
    if (spell_module != NULL)
        return TRUE;

    spell_module = g_module_open ("libaspell", G_MODULE_BIND_LAZY);

    if (spell_module != NULL
        && ASPELL_FUNCTION_AVAILABLE (new_aspell_config)
        && ASPELL_FUNCTION_AVAILABLE (aspell_dict_info_list_elements)
        && ASPELL_FUNCTION_AVAILABLE (aspell_dict_info_enumeration_next)
        && ASPELL_FUNCTION_AVAILABLE (new_aspell_speller)
        && ASPELL_FUNCTION_AVAILABLE (aspell_error_number)
        && ASPELL_FUNCTION_AVAILABLE (aspell_speller_error_message)
        && ASPELL_FUNCTION_AVAILABLE (aspell_speller_error)
        && ASPELL_FUNCTION_AVAILABLE (aspell_error)
        && ASPELL_FUNCTION_AVAILABLE (to_aspell_speller)
        && ASPELL_FUNCTION_AVAILABLE (aspell_speller_check)
        && ASPELL_FUNCTION_AVAILABLE (aspell_speller_suggest)
        && ASPELL_FUNCTION_AVAILABLE (aspell_word_list_elements)
        && ASPELL_FUNCTION_AVAILABLE (aspell_string_enumeration_next)
        && ASPELL_FUNCTION_AVAILABLE (aspell_config_replace)
        && ASPELL_FUNCTION_AVAILABLE (aspell_error_message)
        && ASPELL_FUNCTION_AVAILABLE (delete_aspell_speller)
        && ASPELL_FUNCTION_AVAILABLE (delete_aspell_config)
        && ASPELL_FUNCTION_AVAILABLE (delete_aspell_string_enumeration)
        && ASPELL_FUNCTION_AVAILABLE (get_aspell_dict_info_list)
        && ASPELL_FUNCTION_AVAILABLE (delete_aspell_can_have_error)
        && ASPELL_FUNCTION_AVAILABLE (delete_aspell_dict_info_enumeration)
        && ASPELL_FUNCTION_AVAILABLE (aspell_config_retrieve)
        && ASPELL_FUNCTION_AVAILABLE (aspell_word_list_size)
        && ASPELL_FUNCTION_AVAILABLE (aspell_speller_add_to_personal)
        && ASPELL_FUNCTION_AVAILABLE (aspell_speller_save_all_word_lists))
        return TRUE;

    g_module_close (spell_module);
    spell_module = NULL;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get the current language name.
 *
 * @return language name
 */

static const char *
aspell_get_lang (void)
{
    const char *code;

    code = mc_aspell_config_retrieve (global_speller->config, "lang");
    return spell_decode_lang (code);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get array of available languages.
 *
 * @param lang_list Array of languages. Must be cleared before use
 * @return language list length
 */

static unsigned int
aspell_get_lang_list (GPtrArray *lang_list)
{
    AspellDictInfoList *dlist;
    AspellDictInfoEnumeration *elem;
    const AspellDictInfo *entry;
    unsigned int i = 0;

    if (spell_module == NULL)
        return 0;

    /* the returned pointer should _not_ need to be deleted */
    dlist = mc_get_aspell_dict_info_list (global_speller->config);
    elem = mc_aspell_dict_info_list_elements (dlist);

    while ((entry = mc_aspell_dict_info_enumeration_next (elem)) != NULL)
        if (entry->name != NULL)
        {
            g_ptr_array_add (lang_list, g_strdup (entry->name));
            i++;
        }

    mc_delete_aspell_dict_info_enumeration (elem);

    return i;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Set the language.
 *
 * @param lang Language name
 * @return FALSE or error
 */

static gboolean
aspell_set_lang (const char *lang)
{
    if (lang != NULL)
    {
        AspellCanHaveError *error;
        const char *spell_codeset;

        g_free (spell_language);
        spell_language = g_strdup (lang);

#ifdef HAVE_CHARSET
        if (mc_global.source_codepage > 0)
            spell_codeset = get_codepage_id (mc_global.source_codepage);
        else
#endif
            spell_codeset = str_detect_termencoding ();

        mc_aspell_config_replace (global_speller->config, "lang", lang);
        mc_aspell_config_replace (global_speller->config, "encoding", spell_codeset);

        /* the returned pointer should _not_ need to be deleted */
        if (global_speller->speller != NULL)
            mc_delete_aspell_speller (global_speller->speller);

        global_speller->speller = NULL;

        error = mc_new_aspell_speller (global_speller->config);
        if (mc_aspell_error (error) != 0)
        {
            mc_delete_aspell_can_have_error (error);
            return FALSE;
        }

        global_speller->speller = mc_to_aspell_speller (error);
    }
    return TRUE;
}

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

static int
spell_dialog_spell_suggest_show (WEdit *edit, const char *word, char **new_word,
                                 const GPtrArray *suggest)
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
    WGroup *g;
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

    max_btn_len = MAX (replace_len, skip_len);
    max_btn_len = MAX (max_btn_len, cancel_len);

    lang_label = g_strdup_printf ("%s: %s", _("Language"), aspell_get_lang ());
    word_label = g_strdup_printf ("%s: %s", _("Misspelled"), word);
    word_label_len = str_term_width1 (word_label) + 5;

    sug_dlg_w += max_btn_len;
    sug_dlg_w = MAX (sug_dlg_w, word_label_len) + 1;

    sug_dlg = dlg_create (TRUE, ypos, xpos, sug_dlg_h, sug_dlg_w, WPOS_KEEP_DEFAULT, TRUE,
                          dialog_colors, NULL, NULL, "[ASpell]", _("Check word"));
    g = GROUP (sug_dlg);

    group_add_widget (g, label_new (1, 2, lang_label));
    group_add_widget (g, label_new (3, 2, word_label));

    group_add_widget (g, groupbox_new (4, 2, sug_dlg_h - 5, 25, _("Suggest")));

    sug_list = listbox_new (5, 2, sug_dlg_h - 7, 24, FALSE, NULL);
    for (i = 0; i < suggest->len; i++)
        listbox_add_item (sug_list, LISTBOX_APPEND_AT_END, 0, g_ptr_array_index (suggest, i), NULL,
                          FALSE);
    group_add_widget (g, sug_list);

    group_add_widget (g, add_btn);
    group_add_widget (g, replace_btn);
    group_add_widget (g, skip_btn);
    group_add_widget (g, cancel_button);

    res = dlg_run (sug_dlg);
    if (res == B_ENTER)
    {
        char *tmp = NULL;
        listbox_get_current (sug_list, &curr, NULL);

        if (curr != NULL)
            tmp = g_strdup (curr);
        *new_word = tmp;
    }

    widget_destroy (WIDGET (sug_dlg));
    g_free (lang_label);
    g_free (word_label);

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Add word to personal dictionary.
 *
 * @param word Word for spell check
 * @param word_size  Word size (in bytes)
 * @return FALSE or error
 */
static gboolean
aspell_add_to_dict (const char *word, int word_size)
{
    mc_aspell_speller_add_to_personal (global_speller->speller, word, word_size);

    if (mc_aspell_speller_error (global_speller->speller) != 0)
    {
        edit_error_dialog (_("Error"), mc_aspell_speller_error_message (global_speller->speller));
        return FALSE;
    }

    mc_aspell_speller_save_all_word_lists (global_speller->speller);

    if (mc_aspell_speller_error (global_speller->speller) != 0)
    {
        edit_error_dialog (_("Error"), mc_aspell_speller_error_message (global_speller->speller));
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Examine dictionaries and suggest possible words that may repalce the incorrect word.
 *
 * @param suggest array of words to iterate through
 * @param word Word for spell check
 * @param word_size Word size (in bytes)
 * @return count of suggests for the word
 */

static unsigned int
aspell_suggest (GPtrArray *suggest, const char *word, const int word_size)
{
    unsigned int size = 0;

    if (word != NULL && global_speller != NULL && global_speller->speller != NULL)
    {
        const AspellWordList *wordlist;

        wordlist = mc_aspell_speller_suggest (global_speller->speller, word, word_size);
        if (wordlist != NULL)
        {
            AspellStringEnumeration *elements = NULL;
            unsigned int i;

            elements = mc_aspell_word_list_elements (wordlist);
            size = mc_aspell_word_list_size (wordlist);

            for (i = 0; i < size; i++)
            {
                const char *cur_sugg_word;

                cur_sugg_word = mc_aspell_string_enumeration_next (elements);
                if (cur_sugg_word != NULL)
                    g_ptr_array_add (suggest, g_strdup (cur_sugg_word));
            }

            mc_delete_aspell_string_enumeration (elements);
        }
    }

    return size;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Check word.
 *
 * @param word Word for spell check
 * @param word_size Word size (in bytes)
 * @return FALSE if word is not in the dictionary
 */

static gboolean
aspell_check (const char *word, const int word_size)
{
    int res = 0;

    if (word != NULL && global_speller != NULL && global_speller->speller != NULL)
        res = mc_aspell_speller_check (global_speller->speller, word, word_size);

    return (res == 1);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Clear the array of languages.
 *
 * @param array Array of languages
 */

static void
aspell_array_clean (GPtrArray *array)
{
    if (array != NULL)
        g_ptr_array_free (array, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Initialization of Aspell support.
 */

void
aspell_init (void)
{
    AspellCanHaveError *error = NULL;

    if (strcmp (spell_language, "NONE") == 0)
        return;

    if (global_speller != NULL)
        return;

    global_speller = g_try_malloc (sizeof (spell_t));
    if (global_speller == NULL)
        return;

    if (!spell_available ())
    {
        MC_PTR_FREE (global_speller);
        return;
    }

    global_speller->config = mc_new_aspell_config ();
    global_speller->speller = NULL;

    if (spell_language != NULL)
        mc_aspell_config_replace (global_speller->config, "lang", spell_language);

    error = mc_new_aspell_speller (global_speller->config);

    if (mc_aspell_error_number (error) == 0)
        global_speller->speller = mc_to_aspell_speller (error);
    else
    {
        edit_error_dialog (_("Error"), mc_aspell_error_message (error));
        mc_delete_aspell_can_have_error (error);
        aspell_clean ();
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Deinitialization of Aspell support.
 */

void
aspell_clean (void)
{
    if (global_speller == NULL)
        return;

    if (global_speller->speller != NULL)
        mc_delete_aspell_speller (global_speller->speller);

    if (global_speller->config != NULL)
        mc_delete_aspell_config (global_speller->config);

    MC_PTR_FREE (global_speller);

    g_module_close (spell_module);
    spell_module = NULL;
}

/* --------------------------------------------------------------------------------------------- */

int
edit_suggest_current_word (WEdit *edit)
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
    if (mc_global.source_codepage >= 0 && mc_global.source_codepage != mc_global.display_codepage)
    {
        GString *tmp_word;

        tmp_word = str_convert_to_display (match_word->str);
        g_string_free (match_word, TRUE);
        match_word = tmp_word;
    }
#endif
    if (match_word != NULL)
    {
        if (!aspell_check (match_word->str, (int) word_len))
        {
            GPtrArray *suggest;
            unsigned int res;
            guint i;

            suggest = g_ptr_array_new_with_free_func (g_free);

            res = aspell_suggest (suggest, match_word->str, (int) word_len);
            if (res != 0)
            {
                char *new_word = NULL;

                edit->found_start = word_start;
                edit->found_len = word_len;
                edit->force |= REDRAW_PAGE;
                edit_scroll_screen_over_cursor (edit);
                edit_render_keypress (edit);

                retval =
                    spell_dialog_spell_suggest_show (edit, match_word->str, &new_word, suggest);
                edit_cursor_move (edit, word_len - cut_len);

                if (retval == B_ENTER && new_word != NULL)
                {
#ifdef HAVE_CHARSET
                    if (mc_global.source_codepage >= 0 &&
                        (mc_global.source_codepage != mc_global.display_codepage))
                    {
                        GString *tmp_word;

                        tmp_word = str_convert_to_input (new_word);
                        MC_PTR_FREE (new_word);
                        if (tmp_word != NULL)
                            new_word = g_string_free (tmp_word, FALSE);
                    }
#endif
                    for (i = 0; i < word_len; i++)
                        edit_backspace (edit, TRUE);
                    if (new_word != NULL)
                    {
                        for (i = 0; new_word[i] != '\0'; i++)
                            edit_insert (edit, new_word[i]);
                        g_free (new_word);
                    }
                }
                else if (retval == B_ADD_WORD)
                    aspell_add_to_dict (match_word->str, (int) word_len);
            }

            g_ptr_array_free (suggest, TRUE);
            edit->found_start = 0;
            edit->found_len = 0;
        }

        g_string_free (match_word, TRUE);
    }

    return retval;
}

/* --------------------------------------------------------------------------------------------- */

void
edit_spellcheck_file (WEdit *edit)
{
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
                return;

            c1 = c2;
            edit_cursor_move (edit, 1);
            c2 = edit_buffer_get_current_byte (&edit->buffer);
        }
        while (is_break_char (c1) || is_break_char (c2));
    }
    while (edit_suggest_current_word (edit) != B_CANCEL);
}

/* --------------------------------------------------------------------------------------------- */

void
edit_set_spell_lang (void)
{
    GPtrArray *lang_list;

    lang_list = g_ptr_array_new_with_free_func (g_free);
    if (aspell_get_lang_list (lang_list) != 0)
    {
        const char *lang;

        lang = spell_dialog_lang_list_show (lang_list);
        if (lang != NULL)
            (void) aspell_set_lang (lang);
    }
    aspell_array_clean (lang_list);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Show dialog to select language for spell check.
 *
 * @param languages Array of available languages
 * @return name of chosen language
 */

const char *
spell_dialog_lang_list_show (const GPtrArray *languages)
{

    int lang_dlg_h = 12;        /* dialog height */
    int lang_dlg_w = 30;        /* dialog width */
    const char *selected_lang = NULL;
    unsigned int i;
    int res;
    Listbox *lang_list;

    /* Create listbox */
    lang_list = listbox_window_centered_new (-1, -1, lang_dlg_h, lang_dlg_w,
                                             _("Select language"), "[ASpell]");

    for (i = 0; i < languages->len; i++)
        LISTBOX_APPEND_TEXT (lang_list, 0, g_ptr_array_index (languages, i), NULL, FALSE);

    res = listbox_run (lang_list);
    if (res >= 0)
        selected_lang = g_ptr_array_index (languages, (unsigned int) res);

    return selected_lang;
}

/* --------------------------------------------------------------------------------------------- */
