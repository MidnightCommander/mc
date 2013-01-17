/*
   Editor spell checker

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

#include <stdlib.h>
#include <string.h>
#include <gmodule.h>
#include <aspell.h>

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif
#include "lib/strutil.h"

#include "src/setup.h"

#include "edit-impl.h"
#include "spell.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct aspell_struct
{
    AspellConfig *config;
    AspellSpeller *speller;
} spell_t;

/*** file scope variables ************************************************************************/

static GModule *spell_module = NULL;
static spell_t *global_speller = NULL;

static struct AspellConfig *(*mc_new_aspell_config) (void);
static int (*mc_aspell_config_replace) (struct AspellConfig * ths, const char *key,
                                        const char *value);
static struct AspellCanHaveError *(*mc_new_aspell_speller) (struct AspellConfig * config);
static unsigned int (*mc_aspell_error_number) (const struct AspellCanHaveError * ths);
static const char *(*mc_aspell_speller_error_message) (const struct AspellSpeller * ths);
const struct AspellError *(*mc_aspell_speller_error) (const struct AspellSpeller * ths);

static struct AspellSpeller *(*mc_to_aspell_speller) (struct AspellCanHaveError * obj);
static int (*mc_aspell_speller_check) (struct AspellSpeller * ths, const char *word, int word_size);
static const struct AspellWordList *(*mc_aspell_speller_suggest) (struct AspellSpeller * ths,
                                                                  const char *word, int word_size);
static struct AspellStringEnumeration *(*mc_aspell_word_list_elements) (const struct AspellWordList
                                                                        * ths);
static const char *(*mc_aspell_config_retrieve) (struct AspellConfig * ths, const char *key);
static void (*mc_delete_aspell_speller) (struct AspellSpeller * ths);
static void (*mc_delete_aspell_config) (struct AspellConfig * ths);
static void (*mc_delete_aspell_can_have_error) (struct AspellCanHaveError * ths);
static const char *(*mc_aspell_error_message) (const struct AspellCanHaveError * ths);
static void (*mc_delete_aspell_string_enumeration) (struct AspellStringEnumeration * ths);
static struct AspellDictInfoEnumeration *(*mc_aspell_dict_info_list_elements)
    (const struct AspellDictInfoList * ths);
static struct AspellDictInfoList *(*mc_get_aspell_dict_info_list) (struct AspellConfig * config);
static const struct AspellDictInfo *(*mc_aspell_dict_info_enumeration_next)
    (struct AspellDictInfoEnumeration * ths);
static const char *(*mc_aspell_string_enumeration_next) (struct AspellStringEnumeration * ths);
static void (*mc_delete_aspell_dict_info_enumeration) (struct AspellDictInfoEnumeration * ths);
static unsigned int (*mc_aspell_word_list_size) (const struct AspellWordList * ths);
static const struct AspellError *(*mc_aspell_error) (const struct AspellCanHaveError * ths);
static int (*mc_aspell_speller_add_to_personal) (struct AspellSpeller * ths, const char *word,
                                                 int word_size);
static int (*mc_aspell_speller_save_all_word_lists) (struct AspellSpeller * ths);

static struct
{
    const char *code;
    const char *name;
} spell_codes_map[] =
{
    /* *INDENT-OFF* */
    {"br", "Breton"},
    {"cs", "Czech"},
    {"cy", "Welsh"},
    {"da", "Danish"},
    {"de", "German"},
    {"el", "Greek"},
    {"en", "English"},
    {"en_GB", "British English"},
    {"en_CA", "Canadian English"},
    {"en_US", "American English"},
    {"eo", "Esperanto"},
    {"es", "Spanish"},
    {"fo", "Faroese"},
    {"fr", "French"},
    {"it", "Italian"},
    {"nl", "Dutch"},
    {"no", "Norwegian"},
    {"pl", "Polish"},
    {"pt", "Portuguese"},
    {"ro", "Romanian"},
    {"ru", "Russian"},
    {"sk", "Slovak"},
    {"sv", "Swedish"},
    {"uk", "Ukrainian"},
    {NULL, NULL}
    /* *INDENT-ON* */
};

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
            return spell_codes_map[i].name;
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
    gchar *spell_module_fname;
    gboolean ret = FALSE;

    if (spell_module != NULL)
        return TRUE;

    spell_module_fname = g_module_build_path (NULL, "libaspell");
    spell_module = g_module_open (spell_module_fname, G_MODULE_BIND_LAZY);

    g_free (spell_module_fname);

    if (spell_module == NULL)
        return FALSE;

    if (!g_module_symbol (spell_module, "new_aspell_config", (void *) &mc_new_aspell_config))
        goto error_ret;

    if (!g_module_symbol (spell_module, "aspell_dict_info_list_elements",
                          (void *) &mc_aspell_dict_info_list_elements))
        goto error_ret;

    if (!g_module_symbol (spell_module, "aspell_dict_info_enumeration_next",
                          (void *) &mc_aspell_dict_info_enumeration_next))
        goto error_ret;

    if (!g_module_symbol (spell_module, "new_aspell_speller", (void *) &mc_new_aspell_speller))
        goto error_ret;

    if (!g_module_symbol (spell_module, "aspell_error_number", (void *) &mc_aspell_error_number))
        goto error_ret;

    if (!g_module_symbol (spell_module, "aspell_speller_error_message",
                          (void *) &mc_aspell_speller_error_message))
        goto error_ret;

    if (!g_module_symbol (spell_module, "aspell_speller_error", (void *) &mc_aspell_speller_error))
        goto error_ret;

    if (!g_module_symbol (spell_module, "aspell_error", (void *) &mc_aspell_error))
        goto error_ret;

    if (!g_module_symbol (spell_module, "to_aspell_speller", (void *) &mc_to_aspell_speller))
        goto error_ret;

    if (!g_module_symbol (spell_module, "aspell_speller_check", (void *) &mc_aspell_speller_check))
        goto error_ret;

    if (!g_module_symbol
        (spell_module, "aspell_speller_suggest", (void *) &mc_aspell_speller_suggest))
        goto error_ret;

    if (!g_module_symbol
        (spell_module, "aspell_word_list_elements", (void *) &mc_aspell_word_list_elements))
        goto error_ret;

    if (!g_module_symbol (spell_module, "aspell_string_enumeration_next",
                          (void *) &mc_aspell_string_enumeration_next))
        goto error_ret;

    if (!g_module_symbol
        (spell_module, "aspell_config_replace", (void *) &mc_aspell_config_replace))
        goto error_ret;

    if (!g_module_symbol (spell_module, "aspell_error_message", (void *) &mc_aspell_error_message))
        goto error_ret;

    if (!g_module_symbol
        (spell_module, "delete_aspell_speller", (void *) &mc_delete_aspell_speller))
        goto error_ret;

    if (!g_module_symbol (spell_module, "delete_aspell_config", (void *) &mc_delete_aspell_config))
        goto error_ret;

    if (!g_module_symbol (spell_module, "delete_aspell_string_enumeration",
                          (void *) &mc_delete_aspell_string_enumeration))
        goto error_ret;

    if (!g_module_symbol (spell_module, "get_aspell_dict_info_list",
                          (void *) &mc_get_aspell_dict_info_list))
        goto error_ret;

    if (!g_module_symbol (spell_module, "delete_aspell_can_have_error",
                          (void *) &mc_delete_aspell_can_have_error))
        goto error_ret;

    if (!g_module_symbol (spell_module, "delete_aspell_dict_info_enumeration",
                          (void *) &mc_delete_aspell_dict_info_enumeration))
        goto error_ret;

    if (!g_module_symbol
        (spell_module, "aspell_config_retrieve", (void *) &mc_aspell_config_retrieve))
        goto error_ret;

    if (!g_module_symbol
        (spell_module, "aspell_word_list_size", (void *) &mc_aspell_word_list_size))
        goto error_ret;

    if (!g_module_symbol (spell_module, "aspell_speller_add_to_personal",
                          (void *) &mc_aspell_speller_add_to_personal))
        goto error_ret;

    if (!g_module_symbol (spell_module, "aspell_speller_save_all_word_lists",
                          (void *) &mc_aspell_speller_save_all_word_lists))
        goto error_ret;

    ret = TRUE;

  error_ret:
    if (!ret)
    {
        g_module_close (spell_module);
        spell_module = NULL;
    }
    return ret;
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
        g_free (global_speller);
        global_speller = NULL;
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

    g_free (global_speller);
    global_speller = NULL;

    g_module_close (spell_module);
    spell_module = NULL;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get array of available languages.
 *
 * @param lang_list Array of languages. Must be cleared before use
 * @return language list length
 */

unsigned int
aspell_get_lang_list (GArray * lang_list)
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
    {
        if (entry->name != NULL)
        {
            char *tmp;

            tmp = g_strdup (entry->name);
            g_array_append_val (lang_list, tmp);
            i++;
        }
    }

    mc_delete_aspell_dict_info_enumeration (elem);

    return i;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Clear the array of languages.
 *
 * @param array Array of languages
 */

void
aspell_array_clean (GArray * array)
{
    if (array != NULL)
    {
        guint i = 0;

        for (i = 0; i < array->len; ++i)
        {
            char *tmp;

            tmp = g_array_index (array, char *, i);
            g_free (tmp);
        }
        g_array_free (array, TRUE);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get the current language name.
 *
 * @return language name
 */

const char *
aspell_get_lang (void)
{
    const char *code;

    code = mc_aspell_config_retrieve (global_speller->config, "lang");
    return spell_decode_lang (code);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Set the language.
 *
 * @param lang Language name
 * @return FALSE or error
 */

gboolean
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
 * Check word.
 *
 * @param word Word for spell check
 * @param word_size Word size (in bytes)
 * @return FALSE if word is not in the dictionary
 */

gboolean
aspell_check (const char *word, const int word_size)
{
    int res = 0;

    if (word != NULL && global_speller != NULL && global_speller->speller != NULL)
        res = mc_aspell_speller_check (global_speller->speller, word, word_size);

    return (res == 1);
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

unsigned int
aspell_suggest (GArray * suggest, const char *word, const int word_size)
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

                cur_sugg_word = g_strdup (mc_aspell_string_enumeration_next (elements));
                if (cur_sugg_word != NULL)
                    g_array_append_val (suggest, cur_sugg_word);
            }

            mc_delete_aspell_string_enumeration (elements);
        }
    }

    return size;
}

/* --------------------------------------------------------------------------------------------- */
/*
 * Add word to personal dictionary.
 *
 * @param word Word for spell check
 * @param word_size  Word size (in bytes)
 * @return FALSE or error
 */
gboolean
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
