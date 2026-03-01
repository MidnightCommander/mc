/*
   Editor spell checker

   Copyright (C) 2012-2025
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <gmodule.h>

#include "lib/global.h"
#include "lib/charsets.h"
#include "lib/logging.h"
#include "lib/mcconfig.h"
#include "lib/strutil.h"
#include "lib/util.h"     // MC_PTR_FREE()
#include "lib/tty/tty.h"  // COLS, LINES

#include "editwidget.h"

#include "spell.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define B_SKIP_WORD                  (B_USER + 3)
#define B_ADD_WORD                   (B_USER + 4)

#define ASPELL_FUNCTION_AVAILABLE(f) g_module_symbol (spell_module, #f, (void *) &mc_##f)

/*** file scope type declarations ****************************************************************/

typedef struct AspellConfig AspellConfig;
typedef struct AspellSpeller AspellSpeller;
typedef struct AspellCanHaveError AspellCanHaveError;
typedef struct AspellError AspellError;
typedef struct AspellWordList AspellWordList;
typedef struct AspellStringEnumeration AspellStringEnumeration;
typedef struct AspellDictInfoEnumeration AspellDictInfoEnumeration;
typedef struct AspellDictInfoList AspellDictInfoList;
typedef struct AspellDictInfo
{
    const char *name;
} AspellDictInfo;
typedef struct Hunhandle Hunhandle;

typedef struct aspell_struct
{
    AspellConfig *config;
    AspellSpeller *speller;
} spell_t;

typedef struct hunspell_struct
{
    Hunhandle *speller;
    char *dict_aff;
    char *dict_dic;
} hunspell_t;

/*** forward declarations (file scope functions) *************************************************/
static void spell_debug_log (const char *fmt, ...) G_GNUC_PRINTF (1, 2);

/*** file scope variables ************************************************************************/

static GModule *spell_module = NULL;
static GModule *hunspell_module = NULL;
static spell_t *global_speller = NULL;
static hunspell_t *global_hunspell = NULL;
static GHashTable *spell_check_cache = NULL;
static gboolean spell_config_loaded = FALSE;
static char *plugin_spell_engine = NULL;
static char *plugin_spell_language = NULL;
static gboolean spell_state_cache_valid = FALSE;
static gboolean spell_state_cache_enabled = FALSE;
static gboolean spell_state_cache_available = FALSE;
static char spell_state_cache_reason[BUF_MEDIUM];
static gint64 spell_state_cache_ts_us = 0;
typedef enum
{
    SPELL_BACKEND_NONE = 0,
    SPELL_BACKEND_ASPELL,
    SPELL_BACKEND_HUNSPELL
} spell_backend_t;
static spell_backend_t spell_backend = SPELL_BACKEND_NONE;
static unsigned long spell_settings_lang_input_id = 0;

#define SPELL_PLUGIN_SECTION      "EditorPluginSpell"
#define SPELL_PLUGIN_ENGINE_KEY   "engine"
#define SPELL_PLUGIN_LANGUAGE_KEY "language"
#define SPELL_ENGINE_ASPELL       "aspell"
#define SPELL_ENGINE_HUNSPELL     "hunspell"
#define SPELL_STATE_CACHE_TTL_US  (250 * G_USEC_PER_SEC / 1000)

static AspellConfig *(*mc_new_aspell_config) (void);
static int (*mc_aspell_config_replace) (AspellConfig *ths, const char *key, const char *value);
static AspellCanHaveError *(*mc_new_aspell_speller) (AspellConfig *config);
static unsigned int (*mc_aspell_error_number) (const AspellCanHaveError *ths);
static const char *(*mc_aspell_speller_error_message) (const AspellSpeller *ths);
static const AspellError *(*mc_aspell_speller_error) (const AspellSpeller *ths);

static AspellSpeller *(*mc_to_aspell_speller) (AspellCanHaveError *obj);
static int (*mc_aspell_speller_check) (AspellSpeller *ths, const char *word, int word_size);
static const AspellWordList *(*mc_aspell_speller_suggest) (AspellSpeller *ths, const char *word,
                                                           int word_size);
static AspellStringEnumeration *(*mc_aspell_word_list_elements) (const struct AspellWordList *ths);
static const char *(*mc_aspell_config_retrieve) (AspellConfig *ths, const char *key);
static void (*mc_delete_aspell_speller) (AspellSpeller *ths);
static void (*mc_delete_aspell_config) (AspellConfig *ths);
static void (*mc_delete_aspell_can_have_error) (AspellCanHaveError *ths);
static const char *(*mc_aspell_error_message) (const AspellCanHaveError *ths);
static void (*mc_delete_aspell_string_enumeration) (AspellStringEnumeration *ths);
static AspellDictInfoEnumeration *(*mc_aspell_dict_info_list_elements) (
    const AspellDictInfoList *ths);
static AspellDictInfoList *(*mc_get_aspell_dict_info_list) (AspellConfig *config);
static const AspellDictInfo *(*mc_aspell_dict_info_enumeration_next) (
    AspellDictInfoEnumeration *ths);
static const char *(*mc_aspell_string_enumeration_next) (AspellStringEnumeration *ths);
static void (*mc_delete_aspell_dict_info_enumeration) (AspellDictInfoEnumeration *ths);
static unsigned int (*mc_aspell_word_list_size) (const AspellWordList *ths);
static const AspellError *(*mc_aspell_error) (const AspellCanHaveError *ths);
static int (*mc_aspell_speller_add_to_personal) (AspellSpeller *ths, const char *word,
                                                 int word_size);
static int (*mc_aspell_speller_save_all_word_lists) (AspellSpeller *ths);

static Hunhandle *(*mc_Hunspell_create) (const char *affpath, const char *dpath);
static void (*mc_Hunspell_destroy) (Hunhandle *pHunspell);
static int (*mc_Hunspell_spell) (Hunhandle *pHunspell, const char *word);
static int (*mc_Hunspell_suggest) (Hunhandle *pHunspell, char ***slst, const char *word);
static void (*mc_Hunspell_free_list) (Hunhandle *pHunspell, char ***slst, int n);
static int (*mc_Hunspell_add) (Hunhandle *pHunspell, const char *word);

static struct
{
    const char *code;
    const char *name;
} spell_codes_map[] = {
    { "br", N_ ("Breton") },
    { "cs", N_ ("Czech") },
    { "cy", N_ ("Welsh") },
    { "da", N_ ("Danish") },
    { "de", N_ ("German") },
    { "el", N_ ("Greek") },
    { "en", N_ ("English") },
    { "en_GB", N_ ("British English") },
    { "en_CA", N_ ("Canadian English") },
    { "en_US", N_ ("American English") },
    { "eo", N_ ("Esperanto") },
    { "es", N_ ("Spanish") },
    { "fo", N_ ("Faroese") },
    { "fr", N_ ("French") },
    { "it", N_ ("Italian") },
    { "nl", N_ ("Dutch") },
    { "no", N_ ("Norwegian") },
    { "pl", N_ ("Polish") },
    { "pt", N_ ("Portuguese") },
    { "ro", N_ ("Romanian") },
    { "ru", N_ ("Russian") },
    { "sk", N_ ("Slovak") },
    { "sv", N_ ("Swedish") },
    { "uk", N_ ("Ukrainian") },
    {
        NULL,
        NULL,
    },
};

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
static void
spell_debug_log (const char *fmt, ...)
{
    char *msg;
    va_list ap;

    va_start (ap, fmt);
    msg = g_strdup_vprintf (fmt, ap);
    va_end (ap);

    if (msg == NULL)
        return;

#ifdef USE_MAINTAINER_MODE
    mc_log ("%s", msg);
#else
    g_debug ("%s", msg);
#endif
    g_free (msg);
}

/* --------------------------------------------------------------------------------------------- */
static void
spell_check_cache_reset (void)
{
    if (spell_check_cache != NULL)
        g_hash_table_remove_all (spell_check_cache);
}

/* --------------------------------------------------------------------------------------------- */
static void
spell_state_cache_invalidate (void)
{
    spell_state_cache_valid = FALSE;
    spell_state_cache_enabled = FALSE;
    spell_state_cache_available = FALSE;
    spell_state_cache_reason[0] = '\0';
    spell_state_cache_ts_us = 0;
}

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
            return _ (spell_codes_map[i].name);
    }

    return code;
}

/* --------------------------------------------------------------------------------------------- */
static void
spell_config_load (void)
{
    if (spell_config_loaded)
        return;

    /* Hunspell support removed: keep aspell as the only runtime engine. */
    plugin_spell_engine = g_strdup (SPELL_ENGINE_ASPELL);

    plugin_spell_language = mc_config_get_string (mc_global.main_config, SPELL_PLUGIN_SECTION,
                                                  SPELL_PLUGIN_LANGUAGE_KEY, "en");
    if (plugin_spell_language == NULL || *plugin_spell_language == '\0')
    {
        g_free (plugin_spell_language);
        plugin_spell_language = g_strdup ("en");
    }

    spell_config_loaded = TRUE;
    spell_debug_log ("spell: config loaded engine=%s lang=%s", plugin_spell_engine,
                     plugin_spell_language);
}

/* --------------------------------------------------------------------------------------------- */
static void
spell_config_save (void)
{
    if (!spell_config_loaded)
        return;

    g_free (plugin_spell_engine);
    plugin_spell_engine = g_strdup (SPELL_ENGINE_ASPELL);
    mc_config_set_string (mc_global.main_config, SPELL_PLUGIN_SECTION, SPELL_PLUGIN_ENGINE_KEY,
                          plugin_spell_engine);
    mc_config_set_string (mc_global.main_config, SPELL_PLUGIN_SECTION, SPELL_PLUGIN_LANGUAGE_KEY,
                          plugin_spell_language);
    spell_state_cache_invalidate ();
    spell_check_cache_reset ();
}

/* --------------------------------------------------------------------------------------------- */
static gboolean
hunspell_available (void)
{
    static gboolean initialized = FALSE;
    const char *names[] = { "libhunspell-1.7.so.0",
                            "libhunspell-1.7",
                            "libhunspell-1.6.so.0",
                            "libhunspell-1.6",
                            "libhunspell.so.1",
                            "libhunspell",
                            NULL };
    int i;

    if (initialized)
        return hunspell_module != NULL;
    initialized = TRUE;

    for (i = 0; names[i] != NULL && hunspell_module == NULL; i++)
        hunspell_module = g_module_open (names[i], G_MODULE_BIND_LAZY);

    if (hunspell_module == NULL)
    {
        spell_debug_log ("spell: hunspell module load failed: %s",
                         g_module_error () != NULL ? g_module_error () : "unknown");
        return FALSE;
    }

    if (g_module_symbol (hunspell_module, "Hunspell_create", (void *) &mc_Hunspell_create)
        && g_module_symbol (hunspell_module, "Hunspell_destroy", (void *) &mc_Hunspell_destroy)
        && g_module_symbol (hunspell_module, "Hunspell_spell", (void *) &mc_Hunspell_spell)
        && g_module_symbol (hunspell_module, "Hunspell_suggest", (void *) &mc_Hunspell_suggest)
        && g_module_symbol (hunspell_module, "Hunspell_free_list", (void *) &mc_Hunspell_free_list)
        && g_module_symbol (hunspell_module, "Hunspell_add", (void *) &mc_Hunspell_add))
    {
        spell_debug_log ("spell: hunspell module loaded");
        return TRUE;
    }

    g_module_close (hunspell_module);
    hunspell_module = NULL;
    spell_debug_log ("spell: hunspell module missing required symbols: %s",
                     g_module_error () != NULL ? g_module_error () : "unknown");
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
static gboolean
hunspell_try_dict_base (const char *dir, const char *base, char **aff, char **dic)
{
    char *base_path;
    char *aff_path;
    char *dic_path;
    gboolean ok;

    base_path = g_build_filename (dir, base, NULL);
    aff_path = g_strconcat (base_path, ".aff", NULL);
    dic_path = g_strconcat (base_path, ".dic", NULL);
    g_free (base_path);

    ok = g_file_test (aff_path, G_FILE_TEST_EXISTS) && g_file_test (dic_path, G_FILE_TEST_EXISTS);
    if (ok)
    {
        *aff = aff_path;
        *dic = dic_path;
        return TRUE;
    }

    g_free (aff_path);
    g_free (dic_path);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
static gboolean
hunspell_find_dict (const char *lang, char **aff, char **dic)
{
    const char *dirs[] = { "/usr/share/hunspell", "/usr/local/share/hunspell", "/usr/share/myspell",
                           "/usr/share/myspell/dicts", NULL };
    char *normalized;
    char *upper_pair = NULL;
    int i;

    if (lang == NULL || *lang == '\0' || aff == NULL || dic == NULL)
        return FALSE;

    *aff = NULL;
    *dic = NULL;

    normalized = g_strdup (lang);
    g_strdelimit (normalized, "-", '_');

    if (strlen (normalized) == 2 && g_ascii_isalpha (normalized[0])
        && g_ascii_isalpha (normalized[1]))
        upper_pair =
            g_strdup_printf ("%s_%c%c", normalized, (char) toupper ((unsigned char) normalized[0]),
                             (char) toupper ((unsigned char) normalized[1]));

    for (i = 0; dirs[i] != NULL; i++)
    {
        if (hunspell_try_dict_base (dirs[i], normalized, aff, dic))
            break;
        if (strcmp (normalized, lang) != 0 && hunspell_try_dict_base (dirs[i], lang, aff, dic))
            break;
        if (upper_pair != NULL && hunspell_try_dict_base (dirs[i], upper_pair, aff, dic))
            break;
    }

    g_free (upper_pair);
    g_free (normalized);
    if (*aff == NULL || *dic == NULL)
        spell_debug_log ("spell: hunspell dict not found for lang=%s", lang);
    return *aff != NULL && *dic != NULL;
}

/* --------------------------------------------------------------------------------------------- */
static gboolean G_GNUC_UNUSED
hunspell_open_for_language (const char *lang)
{
    char *aff = NULL;
    char *dic = NULL;
    Hunhandle *speller;

    if (!hunspell_available ())
    {
        spell_debug_log ("spell: hunspell unavailable (library)");
        return FALSE;
    }

    if (!hunspell_find_dict (lang, &aff, &dic))
    {
        spell_debug_log ("spell: hunspell dict lookup failed for lang=%s", lang);
        return FALSE;
    }

    speller = mc_Hunspell_create (aff, dic);
    if (speller == NULL)
    {
        spell_debug_log ("spell: Hunspell_create failed aff=%s dic=%s", aff, dic);
        g_free (aff);
        g_free (dic);
        return FALSE;
    }

    if (global_hunspell == NULL)
        global_hunspell = g_new0 (hunspell_t, 1);

    if (global_hunspell->speller != NULL)
        mc_Hunspell_destroy (global_hunspell->speller);
    g_free (global_hunspell->dict_aff);
    g_free (global_hunspell->dict_dic);

    global_hunspell->speller = speller;
    global_hunspell->dict_aff = aff;
    global_hunspell->dict_dic = dic;
    spell_debug_log ("spell: hunspell opened aff=%s dic=%s", aff, dic);
    return TRUE;
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

    if (spell_module == NULL)
        return FALSE;

    if (ASPELL_FUNCTION_AVAILABLE (new_aspell_config)
        && ASPELL_FUNCTION_AVAILABLE (aspell_dict_info_list_elements)
        && ASPELL_FUNCTION_AVAILABLE (aspell_dict_info_enumeration_next)
        && ASPELL_FUNCTION_AVAILABLE (new_aspell_speller)
        && ASPELL_FUNCTION_AVAILABLE (aspell_error_number)
        && ASPELL_FUNCTION_AVAILABLE (aspell_speller_error_message)
        && ASPELL_FUNCTION_AVAILABLE (aspell_speller_error)
        && ASPELL_FUNCTION_AVAILABLE (aspell_error) && ASPELL_FUNCTION_AVAILABLE (to_aspell_speller)
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

    if (global_speller == NULL || global_speller->config == NULL)
        return spell_decode_lang (plugin_spell_language);

    code = mc_aspell_config_retrieve (global_speller->config, "lang");
    return spell_decode_lang (code);
}

/* --------------------------------------------------------------------------------------------- */
static const char *
spell_get_lang (void)
{
    spell_config_load ();

    if (spell_backend == SPELL_BACKEND_ASPELL)
        return aspell_get_lang ();

    return spell_decode_lang (plugin_spell_language);
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
    AspellConfig *cfg = NULL;
    AspellDictInfoList *dlist;
    AspellDictInfoEnumeration *elem;
    const AspellDictInfo *entry;
    GHashTable *seen;
    gboolean temporary_cfg = FALSE;
    unsigned int i = 0;

    if (lang_list == NULL)
        return 0;

    if (!spell_available ())
        return 0;

    if (global_speller != NULL && global_speller->config != NULL)
        cfg = global_speller->config;
    else
    {
        cfg = mc_new_aspell_config ();
        if (cfg == NULL)
            return 0;
        temporary_cfg = TRUE;
    }

    seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    // the returned pointer should _not_ need to be deleted
    dlist = mc_get_aspell_dict_info_list (cfg);
    if (dlist == NULL)
        goto cleanup;

    elem = mc_aspell_dict_info_list_elements (dlist);
    if (elem == NULL)
        goto cleanup;

    while ((entry = mc_aspell_dict_info_enumeration_next (elem)) != NULL)
        if (entry->name != NULL)
            if (!g_hash_table_contains (seen, entry->name))
            {
                g_hash_table_add (seen, g_strdup (entry->name));
                g_ptr_array_add (lang_list, g_strdup (entry->name));
                i++;
            }

    mc_delete_aspell_dict_info_enumeration (elem);

cleanup:
    g_hash_table_destroy (seen);
    if (temporary_cfg && cfg != NULL)
        mc_delete_aspell_config (cfg);

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

        g_free (plugin_spell_language);
        plugin_spell_language = g_strdup (lang);
        spell_config_save ();

        if (mc_global.source_codepage > 0)
            spell_codeset = get_codepage_id (mc_global.source_codepage);
        else
            spell_codeset = str_detect_termencoding ();

        mc_aspell_config_replace (global_speller->config, "lang", lang);
        mc_aspell_config_replace (global_speller->config, "encoding", spell_codeset);

        // the returned pointer should _not_ need to be deleted
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

    int sug_dlg_h = 14;  // dialog height
    int sug_dlg_w = 29;  // dialog width
    int xpos, ypos;
    char *lang_label;
    char *word_label;
    unsigned int i;
    int res;
    WDialog *sug_dlg;
    WGroup *g;
    WListbox *sug_list;
    int max_btn_width = 0;
    int replace_width;
    int skip_width;
    int cancel_width;
    WButton *add_btn;
    WButton *replace_btn;
    WButton *skip_btn;
    WButton *cancel_button;
    int word_label_len;

    // calculate the dialog metrics
    xpos = (COLS - sug_dlg_w) / 2;
    ypos = (LINES - sug_dlg_h) * 2 / 3;

    // Sometimes menu can hide replaced text. I don't like it
    if ((edit->curs_row >= ypos - 1) && (edit->curs_row <= ypos + sug_dlg_h - 1))
        ypos -= sug_dlg_h;

    add_btn = button_new (5, 28, B_ADD_WORD, NORMAL_BUTTON, _ ("&Add word"), 0);
    replace_btn = button_new (7, 28, B_ENTER, NORMAL_BUTTON, _ ("&Replace"), 0);
    replace_width = button_get_width (replace_btn);
    skip_btn = button_new (9, 28, B_SKIP_WORD, NORMAL_BUTTON, _ ("&Skip"), 0);
    skip_width = button_get_width (skip_btn);
    cancel_button = button_new (11, 28, B_CANCEL, NORMAL_BUTTON, _ ("&Cancel"), 0);
    cancel_width = button_get_width (cancel_button);

    max_btn_width = MAX (replace_width, skip_width);
    max_btn_width = MAX (max_btn_width, cancel_width);

    lang_label = g_strdup_printf ("%s: %s", _ ("Language"), spell_get_lang ());
    word_label = g_strdup_printf ("%s: %s", _ ("Misspelled"), word);
    word_label_len = str_term_width1 (word_label) + 5;

    sug_dlg_w += max_btn_width;
    sug_dlg_w = MAX (sug_dlg_w, word_label_len) + 1;

    sug_dlg = dlg_create (TRUE, ypos, xpos, sug_dlg_h, sug_dlg_w, WPOS_KEEP_DEFAULT, TRUE,
                          dialog_colors, NULL, NULL, "[Spell]", _ ("Check word"));
    g = GROUP (sug_dlg);

    group_add_widget (g, label_new (1, 2, lang_label));
    group_add_widget (g, label_new (3, 2, word_label));

    group_add_widget (g, groupbox_new (4, 2, sug_dlg_h - 5, 25, _ ("Suggest")));

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
        char *curr = NULL;

        listbox_get_current (sug_list, &curr, NULL);
        *new_word = g_strdup (curr);
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
        message (D_ERROR, MSG_ERROR, "%s",
                 mc_aspell_speller_error_message (global_speller->speller));
        return FALSE;
    }

    mc_aspell_speller_save_all_word_lists (global_speller->speller);

    if (mc_aspell_speller_error (global_speller->speller) != 0)
    {
        message (D_ERROR, MSG_ERROR, "%s",
                 mc_aspell_speller_error_message (global_speller->speller));
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
spell_array_clean (GPtrArray *array)
{
    if (array != NULL)
        g_ptr_array_free (array, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static int
spell_pick_lang_button_cb (WButton *button, int action)
{
    Widget *lang_input_widget;
    WInput *lang_input;
    GPtrArray *lang_list;
    const char *lang;

    (void) action;

    if (button == NULL || WIDGET (button)->owner == NULL)
        return 0;

    lang_input_widget =
        widget_find_by_id (WIDGET (WIDGET (button)->owner), spell_settings_lang_input_id);
    if (lang_input_widget == NULL)
        return 0;

    lang_input = INPUT (lang_input_widget);
    lang_list = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add (lang_list, g_strdup ("NONE"));
    (void) aspell_get_lang_list (lang_list);

    lang = spell_dialog_lang_list_show (lang_list);
    if (lang != NULL)
    {
        input_assign_text (lang_input, lang);
        input_set_point (lang_input, (int) strlen (lang));
        widget_draw (lang_input_widget);
    }

    spell_array_clean (lang_list);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
static gboolean
spell_set_lang (const char *lang)
{
    if (lang == NULL || *lang == '\0')
        return FALSE;

    spell_config_load ();
    g_free (plugin_spell_language);
    plugin_spell_language = g_strdup (lang);
    spell_config_save ();

    if (spell_backend == SPELL_BACKEND_ASPELL)
        return aspell_set_lang (lang);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
static gboolean
spell_check (const char *word, int word_size)
{
    if (spell_backend == SPELL_BACKEND_ASPELL)
        return aspell_check (word, word_size);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
static unsigned int
spell_suggest (GPtrArray *suggest, const char *word, int word_size)
{
    if (spell_backend == SPELL_BACKEND_ASPELL)
        return aspell_suggest (suggest, word, word_size);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
static gboolean
spell_add_to_dict (const char *word, int word_size)
{
    if (spell_backend == SPELL_BACKEND_ASPELL)
        return aspell_add_to_dict (word, word_size);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
static void
spell_backend_reason (char *buf, size_t bufsize)
{
    if (buf == NULL || bufsize == 0)
        return;
    g_snprintf (buf, bufsize,
                "Aspell is not installed.\n"
                "Ubuntu/Debian: sudo apt install aspell aspell-<lang>\n"
                "RHEL/Fedora: sudo dnf install aspell aspell-<lang>");
}

/* --------------------------------------------------------------------------------------------- */
static gboolean
spell_backend_selected_available (void)
{
    if (strcmp (plugin_spell_language, "NONE") == 0)
        return FALSE;

    return spell_available ();
}

/* --------------------------------------------------------------------------------------------- */
static gboolean G_GNUC_UNUSED
hunspell_language_available (char *reason, size_t reason_size)
{
    char *aff = NULL;
    char *dic = NULL;
    gboolean ok;

    ok = hunspell_find_dict (plugin_spell_language, &aff, &dic);
    g_free (aff);
    g_free (dic);

    if (!ok && reason != NULL && reason_size > 0)
        g_snprintf (reason, reason_size,
                    "Hunspell dictionary for language \"%s\" is not installed.\n"
                    "Ubuntu/Debian: sudo apt install hunspell-%s\n"
                    "RHEL/Fedora: sudo dnf install hunspell-%s\n"
                    "Then set Language in Spell plugin settings to an installed code.",
                    plugin_spell_language, plugin_spell_language, plugin_spell_language);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Initialization of Aspell support.
 */

void
spell_runtime_init (void)
{
    AspellCanHaveError *error = NULL;

    spell_config_load ();

    if (strcmp (plugin_spell_language, "NONE") == 0)
    {
        spell_state_cache_invalidate ();
        return;
    }

    if (spell_backend != SPELL_BACKEND_NONE)
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

    if (plugin_spell_language != NULL)
        mc_aspell_config_replace (global_speller->config, "lang", plugin_spell_language);

    error = mc_new_aspell_speller (global_speller->config);

    if (mc_aspell_error_number (error) == 0)
    {
        global_speller->speller = mc_to_aspell_speller (error);
        spell_backend = SPELL_BACKEND_ASPELL;
        spell_debug_log ("spell: runtime init backend=aspell lang=%s", plugin_spell_language);
        spell_state_cache_invalidate ();
    }
    else
    {
        spell_debug_log ("spell: aspell init failed: %s", mc_aspell_error_message (error));
        mc_delete_aspell_can_have_error (error);
        spell_runtime_shutdown ();
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Deinitialization of Aspell support.
 */

void
spell_runtime_shutdown (void)
{
    spell_backend = SPELL_BACKEND_NONE;
    spell_state_cache_invalidate ();

    if (global_speller != NULL)
    {
        if (global_speller->speller != NULL)
            mc_delete_aspell_speller (global_speller->speller);

        if (global_speller->config != NULL)
            mc_delete_aspell_config (global_speller->config);

        MC_PTR_FREE (global_speller);
    }

    if (global_hunspell != NULL)
    {
        if (global_hunspell->speller != NULL)
            mc_Hunspell_destroy (global_hunspell->speller);
        g_free (global_hunspell->dict_aff);
        g_free (global_hunspell->dict_dic);
        MC_PTR_FREE (global_hunspell);
    }

    if (spell_check_cache != NULL)
    {
        g_hash_table_destroy (spell_check_cache);
        spell_check_cache = NULL;
    }

    if (spell_module != NULL)
    {
        g_module_close (spell_module);
        spell_module = NULL;
    }

    if (hunspell_module != NULL)
    {
        g_module_close (hunspell_module);
        hunspell_module = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
mc_ep_result_t
spell_query_state (mc_ep_state_t *state)
{
    static char reason[BUF_MEDIUM];
    gint64 now_us;
    gboolean enabled;
    gboolean available;

    spell_config_load ();

    if (state == NULL)
        return MC_EPR_FAILED;

    now_us = g_get_monotonic_time ();
    if (spell_state_cache_valid && (now_us - spell_state_cache_ts_us) < SPELL_STATE_CACHE_TTL_US)
    {
        state->enabled = spell_state_cache_enabled;
        state->available = spell_state_cache_available;
        state->reason = spell_state_cache_reason[0] != '\0' ? spell_state_cache_reason : NULL;
        return MC_EPR_OK;
    }

    enabled = strcmp (plugin_spell_language, "NONE") != 0;
    available = enabled && spell_backend_selected_available ();
    reason[0] = '\0';

    if (!enabled)
        g_strlcpy (reason, _ ("Spell plugin is disabled (language is set to NONE)."),
                   sizeof (reason));
    else if (!available)
    {
        spell_backend_reason (reason, sizeof (reason));
        spell_debug_log ("spell: state unavailable engine=%s lang=%s reason=%s",
                         plugin_spell_engine, plugin_spell_language, reason);
    }
    spell_state_cache_valid = TRUE;
    spell_state_cache_ts_us = now_us;
    spell_state_cache_enabled = enabled;
    spell_state_cache_available = available;
    if (reason[0] != '\0')
        g_strlcpy (spell_state_cache_reason, reason, sizeof (spell_state_cache_reason));
    else
        spell_state_cache_reason[0] = '\0';

    state->enabled = enabled;
    state->available = available;
    state->reason = spell_state_cache_reason[0] != '\0' ? spell_state_cache_reason : NULL;

    return MC_EPR_OK;
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

    // search start of word to spell check
    match_word =
        edit_buffer_get_word_from_pos (&edit->buffer, edit->buffer.curs1, &word_start, &cut_len);
    word_len = match_word->len;

    if (mc_global.source_codepage >= 0 && mc_global.source_codepage != mc_global.display_codepage)
    {
        GString *tmp_word;

        tmp_word = str_nconvert_to_display (match_word->str, match_word->len);
        g_string_free (match_word, TRUE);
        match_word = tmp_word;
    }
    if (match_word != NULL)
    {
        if (!spell_check (match_word->str, (int) word_len))
        {
            GPtrArray *suggest;
            unsigned int res;
            guint i;

            suggest = g_ptr_array_new_with_free_func (g_free);

            res = spell_suggest (suggest, match_word->str, (int) word_len);
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
                    if (mc_global.source_codepage >= 0
                        && (mc_global.source_codepage != mc_global.display_codepage))
                    {
                        GString *tmp_word;

                        tmp_word = str_convert_to_input (new_word);
                        MC_PTR_FREE (new_word);
                        if (tmp_word != NULL)
                            new_word = g_string_free (tmp_word, FALSE);
                    }
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
                    spell_add_to_dict (match_word->str, (int) word_len);
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
    mc_ep_state_t state = { TRUE, TRUE, NULL };

    if (spell_query_state (&state) != MC_EPR_OK || !state.available || !state.enabled)
    {
        message (D_ERROR, _ ("Spell"), "%s",
                 state.reason != NULL ? state.reason : _ ("Spell backend is unavailable."));
        return;
    }

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
    spell_config_load ();

    {
        GPtrArray *lang_list;

        lang_list = g_ptr_array_new_with_free_func (g_free);
        g_ptr_array_add (lang_list, g_strdup ("NONE"));
        if (aspell_get_lang_list (lang_list) != 0 || lang_list->len > 0)
        {
            const char *lang;

            lang = spell_dialog_lang_list_show (lang_list);
            if (lang != NULL)
                (void) spell_set_lang (lang);
        }
        spell_array_clean (lang_list);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
edit_spell_plugin_settings (void)
{
    int selected_engine;
    char *lang_input;
    const char *engine_names[] = {
        _ ("Aspell"),
    };

    quick_widget_t quick_widgets[] = {
        QUICK_START_COLUMNS,
        QUICK_START_GROUPBOX (_ ("Engine")),
        QUICK_RADIO (1, engine_names, &selected_engine, NULL),
        QUICK_STOP_GROUPBOX,
        QUICK_NEXT_COLUMN,
        QUICK_START_GROUPBOX (_ ("Spell")),
        QUICK_LABELED_INPUT (_ ("Language:"), input_label_left, plugin_spell_language,
                             "spell-language", &lang_input, &spell_settings_lang_input_id, FALSE,
                             FALSE, INPUT_COMPLETE_NONE),
        QUICK_BUTTON (_ ("&Select..."), B_USER + 20, spell_pick_lang_button_cb, NULL),
        QUICK_STOP_GROUPBOX,
        QUICK_STOP_COLUMNS,
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };

    quick_dialog_t qdlg = {
        .rect = { -1, -1, 0, 0 },
        .title = _ ("Spell plugin settings"),
        .help = "[Spell]",
        .widgets = quick_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    spell_config_load ();
    selected_engine = 0;
    lang_input = g_strdup (plugin_spell_language);
    spell_settings_lang_input_id = 0;

    if (quick_dialog (&qdlg) != B_CANCEL)
    {
        if (lang_input != NULL && *lang_input != '\0')
            (void) spell_set_lang (lang_input);
        spell_runtime_shutdown ();
        spell_runtime_init ();
    }

    g_free (lang_input);
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

    int lang_dlg_h = 12;  // dialog height
    int lang_dlg_w = 30;  // dialog width
    const char *selected_lang = NULL;
    unsigned int i;
    int res;
    Listbox *lang_list;

    // Create listbox
    lang_list = listbox_window_centered_new (-1, -1, lang_dlg_h, lang_dlg_w, _ ("Select language"),
                                             "[Spell]");

    for (i = 0; i < languages->len; i++)
        LISTBOX_APPEND_TEXT (lang_list, 0, g_ptr_array_index (languages, i), NULL, FALSE);

    res = listbox_run (lang_list);
    if (res >= 0)
        selected_lang = g_ptr_array_index (languages, (unsigned int) res);

    return selected_lang;
}

/* --------------------------------------------------------------------------------------------- */
