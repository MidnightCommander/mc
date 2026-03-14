/*
   Editor syntax highlighting.

   Copyright (C) 1996-2026
   Free Software Foundation, Inc.

   Written by:
   Paul Sheer, 1998
   Leonard den Ottolander <leonard den ottolander nl>, 2005, 2006
   Egmont Koblinger <egmont@gmail.com>, 2010
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2013, 2014, 2021

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

/** \file
 *  \brief Source: editor syntax highlighting
 *  \author Paul Sheer
 *  \date 1996, 1997
 *  \author Mikhail Pobolovets
 *  \date 2010
 *
 *  Misspelled words are flushed from the syntax highlighting rules
 *  when they have been around longer than
 *  TRANSIENT_WORD_TIME_OUT seconds. At a cursor rate of 30
 *  chars per second and say 3 chars + a space per word, we can
 *  accumulate 450 words absolute max with a value of 60. This is
 *  below this limit of 1024 words in a context.
 */

#include <config.h>

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "lib/global.h"
#include "lib/search.h"  // search engine
#include "lib/skin.h"
#include "lib/fileloc.h"  // EDIT_SYNTAX_DIR, EDIT_SYNTAX_FILE
#include "lib/strutil.h"  // utf string functions
#include "lib/util.h"
#include "lib/widget.h"  // Listbox, message()

#include "src/util.h"  // file_error_message()

#include "edit-impl.h"
#include "editwidget.h"

#ifdef HAVE_TREE_SITTER
#include <tree_sitter/api.h>
#ifdef TREE_SITTER_STATIC
#include "ts-grammars/ts-grammar-registry.h"
#else
#include "ts-grammar-loader.h"
#endif
#endif

/*** global variables ****************************************************************************/

gboolean auto_syntax = TRUE;

/*** file scope macro definitions ****************************************************************/

/* bytes */
#define SYNTAX_MARKER_DENSITY 512

#define RULE_ON_LEFT_BORDER   1
#define RULE_ON_RIGHT_BORDER  2

#define SYNTAX_TOKEN_STAR     '\001'
#define SYNTAX_TOKEN_PLUS     '\002'
#define SYNTAX_TOKEN_BRACKET  '\003'
#define SYNTAX_TOKEN_BRACE    '\004'

#define break_a                                                                                    \
    {                                                                                              \
        result = line;                                                                             \
        break;                                                                                     \
    }
#define check_a                                                                                    \
    {                                                                                              \
        if (*a == NULL)                                                                            \
        {                                                                                          \
            result = line;                                                                         \
            break;                                                                                 \
        }                                                                                          \
    }
#define check_not_a                                                                                \
    {                                                                                              \
        if (*a != NULL)                                                                            \
        {                                                                                          \
            result = line;                                                                         \
            break;                                                                                 \
        }                                                                                          \
    }

#define SYNTAX_KEYWORD(x) ((syntax_keyword_t *) (x))
#define CONTEXT_RULE(x)   ((context_rule_t *) (x))

#define ARGS_LEN          1024

#define MAX_ENTRY_LEN     40
#define LIST_LINES        14
#define N_DFLT_ENTRIES    2

/*** file scope type declarations ****************************************************************/

typedef struct
{
    GString *keyword;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
    gboolean line_start;
    int color;
} syntax_keyword_t;

typedef struct
{
    GString *left;
    unsigned char first_left;
    GString *right;
    unsigned char first_right;
    gboolean line_start_left;
    gboolean line_start_right;
    gboolean between_delimiters;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
    char *keyword_first_chars;
    gboolean spelling;
    // first word is word[1]
    GPtrArray *keyword;
} context_rule_t;

typedef struct
{
    off_t offset;
    edit_syntax_rule_t rule;
} syntax_marker_t;

#ifdef HAVE_TREE_SITTER
// tree-sitter highlight cache entry: a range with an associated color
typedef struct
{
    uint32_t start_byte;
    uint32_t end_byte;
    int color;
} ts_highlight_entry_t;

// Cached parser+query for a dynamically-injected language
typedef struct
{
    void *parser;           // TSParser*
    void *query;            // TSQuery*
} ts_dynamic_lang_t;

// A single injection context: parser + query + node types to inject into
typedef struct
{
    gboolean dynamic;       // FALSE = static injection, TRUE = dynamic (per-block language)

    // Static injection fields (dynamic == FALSE):
    void *parser;           // TSParser* for the injected language
    void *query;            // TSQuery* for the injected language
    char **node_types;      // NULL-terminated list of parent node types to inject into

    // Dynamic injection fields (dynamic == TRUE):
    char *block_type;       // parent node type (e.g. "fenced_code_block")
    char *lang_type;        // child node containing language name (e.g. "info_string")
    char *content_type;     // child node containing code (e.g. "code_fence_content")
    GHashTable *lang_cache; // language name -> ts_dynamic_lang_t*
} ts_injection_t;

// mapping from tree-sitter capture name to MC color
typedef struct
{
    const char *capture_name;
    const char *fg;
    const char *attrs;
} ts_color_mapping_t;

#endif

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static char *error_file_name = NULL;

#ifdef HAVE_TREE_SITTER
// Default color theme for tree-sitter highlight capture names.
// Colors are chosen to match MC's default syntax highlighting (blue background skin).
static const ts_color_mapping_t ts_default_colors[] = {
    { "keyword", "yellow", NULL },
    { "keyword.other", "white", NULL },            // Lua, AWK, Pascal keywords
    { "keyword.control", "brightmagenta", NULL },  // PHP keywords
    { "keyword.directive", "magenta", NULL },      // Makefile directives
    { "function", "brightcyan", NULL },
    { "function.special", "brightred", NULL },     // preprocessor macros
    { "function.builtin", "brown", NULL },         // Go builtins
    { "function.macro", "brightmagenta", NULL },   // Rust macros
    { "type", "yellow", NULL },
    { "type.builtin", "brightgreen", NULL },       // Go builtin types
    { "string", "green", NULL },
    { "string.special", "brightgreen", NULL },     // char literals, escape sequences
    { "number", "lightgray", NULL },
    { "number.builtin", "brightgreen", NULL },     // JSON/JS/Rust/Go numbers
    { "comment", "brown", NULL },
    { "comment.special", "brightgreen", NULL },    // XML comments
    { "comment.error", "red", NULL },              // Swift comments
    { "constant", "lightgray", NULL },
    { "constant.builtin", "brightmagenta", NULL },
    { "variable", NULL, NULL },                    // default color
    { "variable.builtin", "brightred", NULL },     // self, $vars
    { "variable.special", "brightgreen", NULL },   // shell $() expansions
    { "operator", "brightcyan", NULL },
    { "operator.word", "yellow", NULL },           // Python/Ruby word operators
    { "delimiter", "brightcyan", NULL },
    { "delimiter.special", "brightmagenta", NULL }, // semicolons
    { "property", "brightcyan", NULL },
    { "property.key", "yellow", NULL },            // INI/properties keys
    { "label", "cyan", NULL },
    { "tag", "brightcyan", NULL },                 // HTML tags
    { "tag.special", "white", NULL },              // XML tags
    { "markup.bold", "brightmagenta", NULL },        // markdown bold
    { "markup.italic", "magenta", NULL },            // markdown italic
    { "markup.addition", "brightgreen", NULL },    // diff additions
    { "markup.deletion", "brightred", NULL },      // diff deletions
    { "markup.heading", "brightmagenta", NULL },   // diff file headers
    { NULL, NULL, NULL }
};

// Config file name for tree-sitter grammar mappings
#define TS_GRAMMARS_FILE "grammars"
#endif

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
syntax_keyword_free (gpointer keyword)
{
    syntax_keyword_t *k = SYNTAX_KEYWORD (keyword);

    g_string_free (k->keyword, TRUE);
    g_free (k->whole_word_chars_left);
    g_free (k->whole_word_chars_right);
    g_free (k);
}

/* --------------------------------------------------------------------------------------------- */

static void
context_rule_free (gpointer rule)
{
    context_rule_t *r = CONTEXT_RULE (rule);

    g_string_free (r->left, TRUE);
    g_string_free (r->right, TRUE);
    g_free (r->whole_word_chars_left);
    g_free (r->whole_word_chars_right);
    g_free (r->keyword_first_chars);

    if (r->keyword != NULL)
        g_ptr_array_free (r->keyword, TRUE);

    g_free (r);
}

/* --------------------------------------------------------------------------------------------- */

static gint
mc_defines_destroy (gpointer key, gpointer value, gpointer data)
{
    (void) data;

    g_free (key);
    g_strfreev ((char **) value);

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/** Completely destroys the defines tree */

static void
destroy_defines (GTree **defines)
{
    g_tree_foreach (*defines, mc_defines_destroy, NULL);
    g_tree_destroy (*defines);
    *defines = NULL;
}

/* --------------------------------------------------------------------------------------------- */

/** Wrapper for case insensitive mode */
inline static int
xx_tolower (const WEdit *edit, int c)
{
    return edit->is_case_insensitive ? tolower (c) : c;
}

/* --------------------------------------------------------------------------------------------- */

static void
subst_defines (GTree *defines, char **argv, char **argv_end)
{
    for (; *argv != NULL && argv < argv_end; argv++)
    {
        char **t;

        t = g_tree_lookup (defines, *argv);
        if (t != NULL)
        {
            int argc, count;
            char **p;

            // Count argv array members
            argc = g_strv_length (argv + 1);

            // Count members of definition array
            count = g_strv_length (t);

            p = argv + count + argc;
            // Buffer overflow or infinitive loop in define
            if (p >= argv_end)
                break;

            // Move rest of argv after definition members
            while (argc >= 0)
                *p-- = argv[argc-- + 1];

            // Copy definition members to argv
            for (p = argv; *t != NULL; *p++ = *t++)
                ;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static off_t
compare_word_to_right (const WEdit *edit, off_t i, const GString *text, const char *whole_left,
                       const char *whole_right, gboolean line_start)
{
    const unsigned char *p, *q;
    int c, d, j;

    c = edit_buffer_get_byte (&edit->buffer, i - 1);
    c = xx_tolower (edit, c);
    if ((line_start && c != '\n') || (whole_left != NULL && strchr (whole_left, c) != NULL))
        return -1;

    for (p = (const unsigned char *) text->str, q = p + text->len; p < q; p++, i++)
    {
        switch (*p)
        {
        case SYNTAX_TOKEN_STAR:
            if (++p > q)
                return -1;
            while (TRUE)
            {
                c = edit_buffer_get_byte (&edit->buffer, i);
                c = xx_tolower (edit, c);
                if (*p == '\0' && whole_right != NULL && strchr (whole_right, c) == NULL)
                    break;
                if (c == *p)
                    break;
                if (c == '\n')
                    return -1;
                i++;
            }
            break;
        case SYNTAX_TOKEN_PLUS:
            if (++p > q)
                return -1;
            j = 0;
            while (TRUE)
            {
                c = edit_buffer_get_byte (&edit->buffer, i);
                c = xx_tolower (edit, c);
                if (c == *p)
                {
                    j = i;
                    if (p[0] == text->str[0]
                        && p[1] == '\0')  // handle eg '+' and @+@ keywords properly
                        break;
                }
                if (j != 0
                    && strchr ((const char *) p + 1, c)
                        != NULL)  // c exists further down, so it will get matched later
                    break;
                if (whiteness (c) || (whole_right != NULL && strchr (whole_right, c) == NULL))
                {
                    if (*p == '\0')
                    {
                        i--;
                        break;
                    }
                    if (j == 0)
                        return -1;
                    i = j;
                    break;
                }
                i++;
            }
            break;
        case SYNTAX_TOKEN_BRACKET:
            if (++p > q)
                return -1;
            c = -1;
            while (TRUE)
            {
                d = c;
                c = edit_buffer_get_byte (&edit->buffer, i);
                c = xx_tolower (edit, c);
                for (j = 0; p[j] != SYNTAX_TOKEN_BRACKET && p[j] != '\0'; j++)
                    if (c == p[j])
                        goto found_char2;
                break;
            found_char2:
                i++;
            }
            i--;
            while (*p != SYNTAX_TOKEN_BRACKET && p <= q)
                p++;
            if (p > q)
                return -1;
            if (p[1] == d)
                i--;
            break;
        case SYNTAX_TOKEN_BRACE:
            if (++p > q)
                return -1;
            c = edit_buffer_get_byte (&edit->buffer, i);
            c = xx_tolower (edit, c);
            for (; *p != SYNTAX_TOKEN_BRACE && *p != '\0'; p++)
                if (c == *p)
                    goto found_char3;
            return -1;
        found_char3:
            while (*p != SYNTAX_TOKEN_BRACE && p < q)
                p++;
            break;
        default:
            c = edit_buffer_get_byte (&edit->buffer, i);
            if (*p != xx_tolower (edit, c))
                return -1;
        }
    }

    if (whole_right == NULL)
        return i;

    c = edit_buffer_get_byte (&edit->buffer, i);
    c = xx_tolower (edit, c);
    return strchr (whole_right, c) != NULL ? -1 : i;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
xx_strchr (const WEdit *edit, const unsigned char *s, int char_byte)
{
    while (*s >= '\005' && xx_tolower (edit, *s) != char_byte)
        s++;

    return (const char *) s;
}

/* --------------------------------------------------------------------------------------------- */

static void
apply_rules_going_right (WEdit *edit, off_t i)
{
    context_rule_t *r;
    int c;
    gboolean contextchanged = FALSE;
    gboolean found_left = FALSE, found_right = FALSE;
    gboolean keyword_foundleft = FALSE, keyword_foundright = FALSE;
    gboolean is_end;
    off_t end = 0;
    edit_syntax_rule_t _rule = edit->rule;

    c = edit_buffer_get_byte (&edit->buffer, i);
    c = xx_tolower (edit, c);
    if (c == 0)
        return;

    is_end = (edit->rule.end == i);

    // check to turn off a keyword
    if (_rule.keyword != 0)
    {
        if (edit_buffer_get_byte (&edit->buffer, i - 1) == '\n')
            _rule.keyword = 0;
        if (is_end)
        {
            _rule.keyword = 0;
            keyword_foundleft = TRUE;
        }
    }

    // check to turn off a context
    if (_rule.context != 0 && _rule.keyword == 0)
    {
        off_t e;

        r = CONTEXT_RULE (g_ptr_array_index (edit->rules, _rule.context));
        if (r->first_right == c && (edit->rule.border & RULE_ON_RIGHT_BORDER) == 0
            && r->right->len != 0
            && (e = compare_word_to_right (edit, i, r->right, r->whole_word_chars_left,
                                           r->whole_word_chars_right, r->line_start_right))
                > 0)
        {
            _rule.end = e;
            found_right = TRUE;
            _rule.border = RULE_ON_RIGHT_BORDER;
            if (r->between_delimiters)
                _rule.context = 0;
        }
        else if (is_end && (edit->rule.border & RULE_ON_RIGHT_BORDER) != 0)
        {
            // always turn off a context at 4
            found_left = TRUE;
            _rule.border = 0;
            if (!keyword_foundleft)
                _rule.context = 0;
        }
        else if (is_end && (edit->rule.border & RULE_ON_LEFT_BORDER) != 0)
        {
            // never turn off a context at 2
            found_left = TRUE;
            _rule.border = 0;
        }
    }

    // check to turn on a keyword
    if (_rule.keyword == 0)
    {
        const char *p;

        r = CONTEXT_RULE (g_ptr_array_index (edit->rules, _rule.context));
        p = r->keyword_first_chars;

        if (p != NULL)
            while (*(p = xx_strchr (edit, (const unsigned char *) p + 1, c)) != '\0')
            {
                syntax_keyword_t *k;
                int count;
                off_t e = -1;

                count = p - r->keyword_first_chars;
                k = SYNTAX_KEYWORD (g_ptr_array_index (r->keyword, count));
                if (k->keyword != 0)
                    e = compare_word_to_right (edit, i, k->keyword, k->whole_word_chars_left,
                                               k->whole_word_chars_right, k->line_start);
                if (e > 0)
                {
                    /* when both context and keyword terminate with a newline,
                       the context overflows to the next line and colorizes it incorrectly */
                    if (e > i + 1 && _rule._context != 0
                        && k->keyword->str[k->keyword->len - 1] == '\n')
                    {
                        r = CONTEXT_RULE (g_ptr_array_index (edit->rules, _rule._context));
                        if (r->right != NULL && r->right->len != 0
                            && r->right->str[r->right->len - 1] == '\n')
                            e--;
                    }

                    end = e;
                    _rule.end = e;
                    _rule.keyword = count;
                    keyword_foundright = TRUE;
                    break;
                }
            }
    }

    // check to turn on a context
    if (_rule.context == 0)
    {
        if (!found_left && is_end)
        {
            if ((edit->rule.border & RULE_ON_RIGHT_BORDER) != 0)
            {
                _rule.border = 0;
                _rule.context = 0;
                contextchanged = TRUE;
                _rule.keyword = 0;
            }
            else if ((edit->rule.border & RULE_ON_LEFT_BORDER) != 0)
            {
                r = CONTEXT_RULE (g_ptr_array_index (edit->rules, _rule._context));
                _rule.border = 0;
                if (r->between_delimiters)
                {
                    _rule.context = _rule._context;
                    contextchanged = TRUE;
                    _rule.keyword = 0;

                    if (r->first_right == c)
                    {
                        off_t e = -1;

                        if (r->right->len != 0)
                            e = compare_word_to_right (edit, i, r->right, r->whole_word_chars_left,
                                                       r->whole_word_chars_right,
                                                       r->line_start_right);
                        if (e >= end)
                        {
                            _rule.end = e;
                            found_right = TRUE;
                            _rule.border = RULE_ON_RIGHT_BORDER;
                            _rule.context = 0;
                        }
                    }
                }
            }
        }

        if (!found_right)
        {
            size_t count;

            for (count = 1; count < edit->rules->len; count++)
            {
                r = CONTEXT_RULE (g_ptr_array_index (edit->rules, count));
                if (r->first_left == c)
                {
                    off_t e = -1;

                    if (r->left->len != 0)
                        e = compare_word_to_right (edit, i, r->left, r->whole_word_chars_left,
                                                   r->whole_word_chars_right, r->line_start_left);
                    if (e >= end && (_rule.keyword == 0 || keyword_foundright))
                    {
                        _rule.end = e;
                        _rule.border = RULE_ON_LEFT_BORDER;
                        _rule._context = count;
                        if (!r->between_delimiters && _rule.keyword == 0)
                        {
                            _rule.context = count;
                            contextchanged = TRUE;
                        }
                        break;
                    }
                }
            }
        }
    }

    // check again to turn on a keyword if the context switched
    if (contextchanged && _rule.keyword == 0)
    {
        const char *p;

        r = CONTEXT_RULE (g_ptr_array_index (edit->rules, _rule.context));
        p = r->keyword_first_chars;

        while (*(p = xx_strchr (edit, (const unsigned char *) p + 1, c)) != '\0')
        {
            syntax_keyword_t *k;
            int count;
            off_t e = -1;

            count = p - r->keyword_first_chars;
            k = SYNTAX_KEYWORD (g_ptr_array_index (r->keyword, count));

            if (k->keyword->len != 0)
                e = compare_word_to_right (edit, i, k->keyword, k->whole_word_chars_left,
                                           k->whole_word_chars_right, k->line_start);
            if (e > 0)
            {
                _rule.end = e;
                _rule.keyword = count;
                break;
            }
        }
    }

    edit->rule = _rule;
}

/* --------------------------------------------------------------------------------------------- */

static void
edit_get_rule (WEdit *edit, off_t byte_index)
{
    off_t i;

    if (byte_index > edit->last_get_rule)
    {
        for (i = edit->last_get_rule + 1; i <= byte_index; i++)
        {
            off_t d = SYNTAX_MARKER_DENSITY;

            apply_rules_going_right (edit, i);

            if (edit->syntax_marker != NULL)
                d += ((syntax_marker_t *) edit->syntax_marker->data)->offset;

            if (i > d)
            {
                syntax_marker_t *s;

                s = g_new (syntax_marker_t, 1);
                s->offset = i;
                s->rule = edit->rule;
                edit->syntax_marker = g_slist_prepend (edit->syntax_marker, s);
            }
        }
    }
    else if (byte_index < edit->last_get_rule)
    {
        while (TRUE)
        {
            syntax_marker_t *s;

            if (edit->syntax_marker == NULL)
            {
                memset (&edit->rule, 0, sizeof (edit->rule));
                for (i = -1; i <= byte_index; i++)
                    apply_rules_going_right (edit, i);
                break;
            }

            s = (syntax_marker_t *) edit->syntax_marker->data;

            if (byte_index >= s->offset)
            {
                edit->rule = s->rule;
                for (i = s->offset + 1; i <= byte_index; i++)
                    apply_rules_going_right (edit, i);
                break;
            }

            g_free (s);
            edit->syntax_marker = g_slist_delete_link (edit->syntax_marker, edit->syntax_marker);
        }
    }
    edit->last_get_rule = byte_index;
}

/* --------------------------------------------------------------------------------------------- */

static int
translate_rule_to_color (const WEdit *edit, const edit_syntax_rule_t *rule)
{
    syntax_keyword_t *k;
    context_rule_t *r;

    r = CONTEXT_RULE (g_ptr_array_index (edit->rules, rule->context));
    k = SYNTAX_KEYWORD (g_ptr_array_index (r->keyword, rule->keyword));

    return k->color;
}

/* --------------------------------------------------------------------------------------------- */
/**
   Returns 0 on error/eof or a count of the number of bytes read
   including the newline. Result must be free'd.
   In case of an error, *line will not be modified.
 */

static size_t
read_one_line (char **line, FILE *f)
{
    GString *p;
    size_t r = 0;

    // not reallocate string too often
    p = g_string_sized_new (64);

    while (TRUE)
    {
        int c;

        c = fgetc (f);
        if (c == EOF)
        {
            if (ferror (f))
            {
                if (errno == EINTR)
                    continue;
                r = 0;
            }
            break;
        }
        r++;

        // handle all of \r\n, \r, \n correctly.
        if (c == '\n')
            break;
        if (c == '\r')
        {
            c = fgetc (f);
            if (c == '\n')
                r++;
            else
                ungetc (c, f);
            break;
        }

        g_string_append_c (p, c);
    }
    if (r != 0)
        *line = g_string_free (p, FALSE);
    else
        g_string_free (p, TRUE);

    return r;
}

/* --------------------------------------------------------------------------------------------- */

static char *
convert (char *s)
{
    char *r, *p;

    p = r = s;
    while (*s)
    {
        switch (*s)
        {
        case '\\':
            s++;
            switch (*s)
            {
            case ' ':
                *p = ' ';
                s--;
                break;
            case 'n':
                *p = '\n';
                break;
            case 'r':
                *p = '\r';
                break;
            case 't':
                *p = '\t';
                break;
            case 's':
                *p = ' ';
                break;
            case '*':
                *p = '*';
                break;
            case '\\':
                *p = '\\';
                break;
            case '[':
            case ']':
                *p = SYNTAX_TOKEN_BRACKET;
                break;
            case '{':
            case '}':
                *p = SYNTAX_TOKEN_BRACE;
                break;
            case 0:
                *p = *s;
                return r;
            default:
                *p = *s;
                break;
            }
            break;
        case '*':
            *p = SYNTAX_TOKEN_STAR;
            break;
        case '+':
            *p = SYNTAX_TOKEN_PLUS;
            break;
        default:
            *p = *s;
            break;
        }
        s++;
        p++;
    }
    *p = '\0';
    return r;
}

/* --------------------------------------------------------------------------------------------- */

static int
get_args (char *l, char **args, int args_size)
{
    int argc = 0;

    while (argc < args_size)
    {
        char *p = l;

        while (*p != '\0' && whiteness (*p))
            p++;
        if (*p == '\0')
            break;
        for (l = p + 1; *l != '\0' && !whiteness (*l); l++)
            ;
        if (*l != '\0')
            *l++ = '\0';
        args[argc++] = convert (p);
    }
    args[argc] = (char *) NULL;
    return argc;
}

/* --------------------------------------------------------------------------------------------- */

static int
this_try_alloc_color_pair (tty_color_pair_t *color)
{
    char f[80], b[80], a[80], *p;

    if (color->bg != NULL && *color->bg == '\0')
        color->bg = NULL;
    if (color->fg != NULL && *color->fg == '\0')
        color->fg = NULL;
    if (color->attrs != NULL && *color->attrs == '\0')
        color->attrs = NULL;

    if (color->fg == NULL && color->bg == NULL)
        return EDITOR_NORMAL_COLOR;

    if (color->fg != NULL)
    {
        g_strlcpy (f, color->fg, sizeof (f));
        p = strchr (f, '/');
        if (p != NULL)
            *p = '\0';
        color->fg = f;
    }
    if (color->bg != NULL)
    {
        g_strlcpy (b, color->bg, sizeof (b));
        p = strchr (b, '/');
        if (p != NULL)
            *p = '\0';
        color->bg = b;
    }
    if (color->fg == NULL || color->bg == NULL)
    {
        // get colors from skin
        char *editnormal;

        editnormal = mc_skin_get ("editor", "_default_", "default;default");

        if (color->fg == NULL)
        {
            g_strlcpy (f, editnormal, sizeof (f));
            p = strchr (f, ';');
            if (p != NULL)
                *p = '\0';
            if (f[0] == '\0')
                g_strlcpy (f, "default", sizeof (f));
            color->fg = f;
        }
        if (color->bg == NULL)
        {
            p = strchr (editnormal, ';');
            if ((p != NULL) && (*(++p) != '\0'))
                g_strlcpy (b, p, sizeof (b));
            else
                g_strlcpy (b, "default", sizeof (b));
            color->bg = b;
        }

        g_free (editnormal);
    }

    if (color->attrs != NULL)
    {
        g_strlcpy (a, color->attrs, sizeof (a));
        p = strchr (a, '/');
        if (p != NULL)
            *p = '\0';
        // get_args() mangles the + signs, unmangle 'em
        p = a;
        while ((p = strchr (p, SYNTAX_TOKEN_PLUS)) != NULL)
            *p++ = '+';
        color->attrs = a;
    }

    return tty_try_alloc_color_pair (color, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static FILE *
open_include_file (const char *filename)
{
    FILE *f;

    g_free (error_file_name);
    error_file_name = g_strdup (filename);
    if (g_path_is_absolute (filename))
        return fopen (filename, "r");

    g_free (error_file_name);
    error_file_name =
        g_build_filename (mc_config_get_data_path (), EDIT_SYNTAX_DIR, filename, (char *) NULL);
    f = fopen (error_file_name, "r");
    if (f != NULL)
        return f;

    g_free (error_file_name);
    error_file_name =
        g_build_filename (mc_global.share_data_dir, EDIT_SYNTAX_DIR, filename, (char *) NULL);

    return fopen (error_file_name, "r");
}

/* --------------------------------------------------------------------------------------------- */

inline static void
xx_lowerize_line (WEdit *edit, char *line, size_t len)
{
    if (edit->is_case_insensitive)
    {
        size_t i;

        for (i = 0; i < len; ++i)
            line[i] = tolower (line[i]);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** returns line number on error */

static int
edit_read_syntax_rules (WEdit *edit, FILE *f, char **args, int args_size)
{
    FILE *g = NULL;
    tty_color_pair_t color;
    char last_fg[BUF_TINY / 2] = "";
    char last_bg[BUF_TINY / 2] = "";
    char last_attrs[BUF_TINY] = "";
    char whole_right[BUF_MEDIUM];
    char whole_left[BUF_MEDIUM];
    char *l = NULL;
    int save_line = 0, line = 0;
    context_rule_t *c = NULL;
    gboolean no_words = TRUE;
    int result = 0;

    args[0] = NULL;
    edit->is_case_insensitive = FALSE;

    strcpy (whole_left, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");
    strcpy (whole_right, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");

    edit->rules = g_ptr_array_new_with_free_func (context_rule_free);

    if (edit->defines == NULL)
        edit->defines = g_tree_new ((GCompareFunc) strcmp);

    while (TRUE)
    {
        char **a;
        size_t len;
        int argc;

        line++;
        l = NULL;

        len = read_one_line (&l, f);
        if (len == 0)
        {
            if (g == NULL)
                break;

            fclose (f);
            f = g;
            g = NULL;
            line = save_line + 1;
            MC_PTR_FREE (error_file_name);
            MC_PTR_FREE (l);
            len = read_one_line (&l, f);
            if (len == 0)
                break;
        }

        xx_lowerize_line (edit, l, len);

        argc = get_args (l, args, args_size);
        a = args + 1;
        if (args[0] == NULL)
        {
            // do nothing
        }
        else if (strcmp (args[0], "include") == 0)
        {
            if (g != NULL || argc != 2)
            {
                result = line;
                break;
            }
            g = f;
            f = open_include_file (args[1]);
            if (f == NULL)
            {
                MC_PTR_FREE (error_file_name);
                result = line;
                break;
            }
            save_line = line;
            line = 0;
        }
        else if (strcmp (args[0], "caseinsensitive") == 0)
        {
            edit->is_case_insensitive = TRUE;
        }
        else if (strcmp (args[0], "wholechars") == 0)
        {
            check_a;
            if (strcmp (*a, "left") == 0)
            {
                a++;
                g_strlcpy (whole_left, *a, sizeof (whole_left));
            }
            else if (strcmp (*a, "right") == 0)
            {
                a++;
                g_strlcpy (whole_right, *a, sizeof (whole_right));
            }
            else
            {
                g_strlcpy (whole_left, *a, sizeof (whole_left));
                g_strlcpy (whole_right, *a, sizeof (whole_right));
            }
            a++;
            check_not_a;
        }
        else if (strcmp (args[0], "context") == 0)
        {
            syntax_keyword_t *k;

            check_a;
            if (edit->rules->len == 0)
            {
                // first context is the default
                if (strcmp (*a, "default") != 0)
                    break_a;

                a++;
                c = g_new0 (context_rule_t, 1);
                g_ptr_array_add (edit->rules, c);
                c->left = g_string_new (" ");
                c->right = g_string_new (" ");
            }
            else
            {
                // Start new context.
                c = g_new0 (context_rule_t, 1);
                g_ptr_array_add (edit->rules, c);
                if (strcmp (*a, "exclusive") == 0)
                {
                    a++;
                    c->between_delimiters = TRUE;
                }
                check_a;
                if (strcmp (*a, "whole") == 0)
                {
                    a++;
                    c->whole_word_chars_left = g_strdup (whole_left);
                    c->whole_word_chars_right = g_strdup (whole_right);
                }
                else if (strcmp (*a, "wholeleft") == 0)
                {
                    a++;
                    c->whole_word_chars_left = g_strdup (whole_left);
                }
                else if (strcmp (*a, "wholeright") == 0)
                {
                    a++;
                    c->whole_word_chars_right = g_strdup (whole_right);
                }
                check_a;
                if (strcmp (*a, "linestart") == 0)
                {
                    a++;
                    c->line_start_left = TRUE;
                }
                check_a;
                c->left = g_string_new (*a++);
                check_a;
                if (strcmp (*a, "linestart") == 0)
                {
                    a++;
                    c->line_start_right = TRUE;
                }
                check_a;
                c->right = g_string_new (*a++);
                c->first_left = c->left->str[0];
                c->first_right = c->right->str[0];
            }
            c->keyword = g_ptr_array_new_with_free_func (syntax_keyword_free);
            k = g_new0 (syntax_keyword_t, 1);
            g_ptr_array_add (c->keyword, k);
            no_words = FALSE;
            subst_defines (edit->defines, a, &args[ARGS_LEN]);
            color.fg = *a;
            if (*a != NULL)
                a++;
            color.bg = *a;
            if (*a != NULL)
                a++;
            color.attrs = *a;
            if (*a != NULL)
                a++;
            g_strlcpy (last_fg, color.fg != NULL ? color.fg : "", sizeof (last_fg));
            g_strlcpy (last_bg, color.bg != NULL ? color.bg : "", sizeof (last_bg));
            g_strlcpy (last_attrs, color.attrs != NULL ? color.attrs : "", sizeof (last_attrs));
            k->color = this_try_alloc_color_pair (&color);
            k->keyword = g_string_new (" ");
            check_not_a;
        }
        else if (strcmp (args[0], "spellcheck") == 0)
        {
            if (c == NULL)
            {
                result = line;
                break;
            }
            c->spelling = TRUE;
        }
        else if (strcmp (args[0], "keyword") == 0)
        {
            context_rule_t *last_rule;
            syntax_keyword_t *k;

            if (no_words)
                break_a;
            check_a;
            last_rule = CONTEXT_RULE (g_ptr_array_index (edit->rules, edit->rules->len - 1));
            k = g_new0 (syntax_keyword_t, 1);
            g_ptr_array_add (last_rule->keyword, k);
            if (strcmp (*a, "whole") == 0)
            {
                a++;
                k->whole_word_chars_left = g_strdup (whole_left);
                k->whole_word_chars_right = g_strdup (whole_right);
            }
            else if (strcmp (*a, "wholeleft") == 0)
            {
                a++;
                k->whole_word_chars_left = g_strdup (whole_left);
            }
            else if (strcmp (*a, "wholeright") == 0)
            {
                a++;
                k->whole_word_chars_right = g_strdup (whole_right);
            }
            check_a;
            if (strcmp (*a, "linestart") == 0)
            {
                a++;
                k->line_start = TRUE;
            }
            check_a;
            if (strcmp (*a, "whole") == 0)
                break_a;

            k->keyword = g_string_new (*a++);
            subst_defines (edit->defines, a, &args[ARGS_LEN]);
            color.fg = *a;
            if (*a != NULL)
                a++;
            color.bg = *a;
            if (*a != NULL)
                a++;
            color.attrs = *a;
            if (*a != NULL)
                a++;
            if (color.fg == NULL)
                color.fg = last_fg;
            if (color.bg == NULL)
                color.bg = last_bg;
            if (color.attrs == NULL)
                color.attrs = last_attrs;
            k->color = this_try_alloc_color_pair (&color);
            check_not_a;
        }
        else if (*(args[0]) == '#')
        {
            // do nothing for comment
        }
        else if (strcmp (args[0], "file") == 0)
        {
            break;
        }
        else if (strcmp (args[0], "define") == 0)
        {
            char *key = *a++;
            char **argv;

            if (argc < 3)
                break_a;
            argv = g_tree_lookup (edit->defines, key);
            if (argv != NULL)
                mc_defines_destroy (NULL, argv, NULL);
            else
                key = g_strdup (key);

            argv = g_new (char *, argc - 1);
            g_tree_insert (edit->defines, key, argv);
            while (*a != NULL)
                *argv++ = g_strdup (*a++);
            *argv = NULL;
        }
        else
        {
            // anything else is an error
            break_a;
        }
        MC_PTR_FREE (l);
    }
    MC_PTR_FREE (l);

    if (edit->rules->len == 0)
    {
        g_ptr_array_free (edit->rules, TRUE);
        edit->rules = NULL;
    }

    if (result == 0)
    {
        size_t i;
        GString *first_chars;

        if (edit->rules == NULL)
            return line;

        first_chars = g_string_sized_new (32);

        // collect first character of keywords
        for (i = 0; i < edit->rules->len; i++)
        {
            size_t j;

            g_string_set_size (first_chars, 0);
            c = CONTEXT_RULE (g_ptr_array_index (edit->rules, i));

            g_string_append_c (first_chars, (char) 1);
            for (j = 1; j < c->keyword->len; j++)
            {
                syntax_keyword_t *k;

                k = SYNTAX_KEYWORD (g_ptr_array_index (c->keyword, j));
                g_string_append_c (first_chars, k->keyword->str[0]);
            }

            c->keyword_first_chars = g_strndup (first_chars->str, first_chars->len);
        }

        g_string_free (first_chars, TRUE);
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */

/* returns -1 on file error, line number on error in file syntax */
static int
edit_read_syntax_file (WEdit *edit, GPtrArray *pnames, const char *syntax_file,
                       const char *editor_file, const char *first_line, const char *type)
{
    FILE *f, *g = NULL;
    char *args[ARGS_LEN], *l = NULL;
    long line = 0;
    int result = 0;
    gboolean found = FALSE;

    f = fopen (syntax_file, "r");
    if (f == NULL)
    {
        char *global_syntax_file;

        global_syntax_file =
            g_build_filename (mc_global.share_data_dir, EDIT_SYNTAX_FILE, (char *) NULL);
        f = fopen (global_syntax_file, "r");
        g_free (global_syntax_file);
        if (f == NULL)
            return -1;
    }

    args[0] = NULL;
    while (TRUE)
    {
        line++;
        MC_PTR_FREE (l);
        if (read_one_line (&l, f) == 0)
            break;
        (void) get_args (l, args, ARGS_LEN - 1);  // Final NULL
        if (args[0] == NULL)
            continue;

        // Looking for 'include ...' lines before first 'file ...' ones
        if (!found && strcmp (args[0], "include") == 0)
        {
            if (args[1] == NULL || (g = open_include_file (args[1])) == NULL)
            {
                result = line;
                break;
            }
            goto found_type;
        }

        // looking for 'file ...' lines only
        if (strcmp (args[0], "file") != 0)
            continue;

        found = TRUE;

        // must have two args or report error
        if (args[1] == NULL || args[2] == NULL)
        {
            result = line;
            break;
        }

        if (pnames != NULL)
        {
            // 1: just collecting a list of names of rule sets
            g_ptr_array_add (pnames, g_strdup (args[2]));
        }
        else if (type != NULL)
        {
            // 2: rule set was explicitly specified by the caller
            if (strcmp (type, args[2]) == 0)
                goto found_type;
        }
        else if (editor_file != NULL && edit != NULL)
        {
            // 3: auto-detect rule set from regular expressions
            gboolean q;

            q = mc_search (args[1], NULL, editor_file, MC_SEARCH_T_REGEX);
            // does filename match arg 1 ?
            if (!q && args[3] != NULL)
            {
                // does first line match arg 3 ?
                q = mc_search (args[3], NULL, first_line, MC_SEARCH_T_REGEX);
            }
            if (q)
            {
                int line_error;
                char *syntax_type;

            found_type:
                syntax_type = args[2];
                line_error = edit_read_syntax_rules (edit, g ? g : f, args, ARGS_LEN - 1);
                if (line_error != 0)
                {
                    if (error_file_name == NULL)  // an included file
                        result = line + line_error;
                    else
                        result = line_error;
                }
                else
                {
                    g_free (edit->syntax_type);
                    edit->syntax_type = g_strdup (syntax_type);
                    // if there are no rules then turn off syntax highlighting for speed
                    if (g == NULL && edit->rules->len == 1)
                    {
                        context_rule_t *r0;

                        r0 = CONTEXT_RULE (g_ptr_array_index (edit->rules, 0));
                        if (r0->keyword->len == 1 && !r0->spelling)
                        {
                            edit_free_syntax_rules (edit);
                            break;
                        }
                    }
                }

                if (g == NULL)
                    break;

                fclose (g);
                g = NULL;
            }
        }
    }
    g_free (l);
    fclose (f);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
get_first_editor_line (WEdit *edit)
{
    static char s[256];

    s[0] = '\0';

    if (edit != NULL)
    {
        size_t i;

        for (i = 0; i < sizeof (s) - 1; i++)
        {
            s[i] = edit_buffer_get_byte (&edit->buffer, i);
            if (s[i] == '\n')
            {
                s[i] = '\0';
                break;
            }
        }

        s[sizeof (s) - 1] = '\0';
    }

    return s;
}

/* --------------------------------------------------------------------------------------------- */

static int
pstrcmp (const void *p1, const void *p2)
{
    return strcmp (*(char *const *) p1, *(char *const *) p2);
}

/* --------------------------------------------------------------------------------------------- */

static int
exec_edit_syntax_dialog (const GPtrArray *names, const char *current_syntax)
{
    size_t i;
    Listbox *syntaxlist;

    syntaxlist =
        listbox_window_new (LIST_LINES, MAX_ENTRY_LEN, _ ("Choose syntax highlighting"), NULL);
    LISTBOX_APPEND_TEXT (syntaxlist, 'A', _ ("< Auto >"), NULL, FALSE);
    LISTBOX_APPEND_TEXT (syntaxlist, 'R', _ ("< Reload Current Syntax >"), NULL, FALSE);

    for (i = 0; i < names->len; i++)
    {
        const char *name;

        name = g_ptr_array_index (names, i);
        LISTBOX_APPEND_TEXT (syntaxlist, 0, name, NULL, FALSE);
        if (current_syntax != NULL && strcmp (name, current_syntax) == 0)
            listbox_set_current (syntaxlist->list, i + N_DFLT_ENTRIES);
    }

    return listbox_run (syntaxlist);
}

/* --------------------------------------------------------------------------------------------- */
/* Tree-sitter integration */
/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_TREE_SITTER

/**
 * TSInput read callback: reads chunks of text from the edit buffer.
 */
static const char *
ts_input_read (void *payload, uint32_t byte_index, TSPoint position, uint32_t *bytes_read)
{
    static char buf[4096];
    WEdit *edit = (WEdit *) payload;
    uint32_t i;

    (void) position;

    for (i = 0; i < sizeof (buf) && (off_t) (byte_index + i) < edit->buffer.size; i++)
        buf[i] = edit_buffer_get_byte (&edit->buffer, (off_t) (byte_index + i));

    *bytes_read = i;
    return (i > 0) ? buf : NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Allocate an MC color pair for a tree-sitter capture name.
 * Looks up the capture name in the default color mapping table.
 */
static int
ts_capture_name_to_color (const char *capture_name)
{
    const ts_color_mapping_t *m;
    tty_color_pair_t color;

    for (m = ts_default_colors; m->capture_name != NULL; m++)
    {
        if (strcmp (m->capture_name, capture_name) == 0)
        {
            if (m->fg == NULL)
                return EDITOR_NORMAL_COLOR;

            color.fg = m->fg;
            color.bg = NULL;
            color.attrs = m->attrs;
            return this_try_alloc_color_pair (&color);
        }
    }

    return EDITOR_NORMAL_COLOR;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Open the ts-grammars config file. Tries user config dir first, then system share dir.
 * Returns a FILE* on success, or NULL.
 */
static FILE *
ts_open_config (void)
{
    char *config_path;
    FILE *f;

    config_path = g_build_filename (mc_config_get_data_path (), EDIT_SYNTAX_TS_DIR, TS_GRAMMARS_FILE,
                                    (char *) NULL);
    f = fopen (config_path, "r");
    g_free (config_path);

    if (f == NULL)
    {
        config_path = g_build_filename (mc_global.share_data_dir, EDIT_SYNTAX_TS_DIR,
                                        TS_GRAMMARS_FILE, (char *) NULL);
        f = fopen (config_path, "r");
        g_free (config_path);
    }

    return f;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read the ts-grammars config file and find a matching grammar for the filename.
 * On match, fills grammar_name and syntax_name (newly allocated strings).
 * Returns TRUE if a match was found.
 */
static gboolean
ts_find_grammar (const char *filename, const char *first_line, char **grammar_name,
                 char **syntax_name)
{
    FILE *f;
    char *line = NULL;

    if (filename == NULL)
        return FALSE;

    *grammar_name = NULL;
    *syntax_name = NULL;

    f = ts_open_config ();
    if (f == NULL)
        return FALSE;

    while (read_one_line (&line, f) != 0)
    {
        char *p, *pattern, *name, *display;

        p = line;

        // Skip whitespace
        while (*p != '\0' && whiteness (*p))
            p++;

        // Skip comments and empty lines
        if (*p == '#' || *p == '\0')
        {
            g_free (line);
            line = NULL;
            continue;
        }

        // Must start with "file" or "shebang"
        gboolean is_shebang = FALSE;

        if (strncmp (p, "shebang", 7) == 0 && whiteness (p[7]))
        {
            is_shebang = TRUE;
            p += 7;
        }
        else if (strncmp (p, "file", 4) == 0 && whiteness (p[4]))
        {
            p += 4;
        }
        else
        {
            g_free (line);
            line = NULL;
            continue;
        }

        // Skip whitespace
        while (*p != '\0' && whiteness (*p))
            p++;
        pattern = p;

        // Find end of pattern (next whitespace)
        while (*p != '\0' && !whiteness (*p))
            p++;
        if (*p == '\0')
        {
            g_free (line);
            line = NULL;
            continue;
        }
        *p++ = '\0';

        // Skip whitespace
        while (*p != '\0' && whiteness (*p))
            p++;
        name = p;

        // Find end of grammar name (next whitespace)
        while (*p != '\0' && !whiteness (*p))
            p++;
        if (*p == '\0')
        {
            g_free (line);
            line = NULL;
            continue;
        }
        *p++ = '\0';

        // Skip whitespace - rest is display name (may contain backslash-escaped spaces)
        while (*p != '\0' && whiteness (*p))
            p++;
        display = p;

        // Unescape backslash-space in display name
        {
            char *src = display, *dst = display;

            while (*src != '\0')
            {
                if (*src == '\\' && *(src + 1) == ' ')
                {
                    *dst++ = ' ';
                    src += 2;
                }
                else
                    *dst++ = *src++;
            }
            *dst = '\0';
        }

        // Check if filename or first line matches this pattern
        const char *match_against = is_shebang ? first_line : filename;

        if (match_against != NULL && match_against[0] != '\0'
            && mc_search (pattern, NULL, match_against, MC_SEARCH_T_REGEX))
        {
            *grammar_name = g_strdup (name);
            *syntax_name = g_strdup (display);
            g_free (line);
            fclose (f);
            return TRUE;
        }

        g_free (line);
        line = NULL;
    }

    g_free (line);
    fclose (f);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Load a highlight query file. Searches in user data dir, then system share dir.
 * Returns a newly allocated string with the file contents, or NULL on failure.
 */
static char *
ts_load_query_file (const char *query_filename, uint32_t *out_len)
{
    char *path;
    char *contents = NULL;
    gsize len = 0;

    // Try user config dir first
    path = g_build_filename (mc_config_get_data_path (), EDIT_SYNTAX_TS_DIR, "queries",
                             query_filename, (char *) NULL);
    if (g_file_get_contents (path, &contents, &len, NULL))
    {
        g_free (path);
        *out_len = (uint32_t) len;
        return contents;
    }
    g_free (path);

    // Try system share dir
    path = g_build_filename (mc_global.share_data_dir, EDIT_SYNTAX_TS_DIR, "queries",
                             query_filename, (char *) NULL);
    if (g_file_get_contents (path, &contents, &len, NULL))
    {
        g_free (path);
        *out_len = (uint32_t) len;
        return contents;
    }
    g_free (path);

    *out_len = 0;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Find all injection configs for a grammar.
 * Searches for "inject <parent_grammar> <child_grammar> <node_type1>,<node_type2>,..."
 * lines in the config file.  Returns a GArray of ts_injection_t entries with only
 * the grammar_name (stored in parser as a temporary hack) and node_types filled in.
 * The caller is responsible for initializing the parser/query or freeing the results.
 *
 * Actually, we use a small helper struct for the config results to keep it clean.
 */

typedef struct
{
    gboolean dynamic;       // FALSE = static inject, TRUE = inject_dynamic

    // Static inject fields:
    char *grammar_name;
    char **node_types;

    // Dynamic inject fields:
    char *block_type;
    char *lang_type;
    char *content_type;
} ts_inject_config_t;

static GArray *
ts_find_injections (const char *grammar_name)
{
    FILE *f;
    char *line = NULL;
    GArray *configs;

    f = ts_open_config ();
    if (f == NULL)
        return NULL;

    configs = g_array_new (FALSE, TRUE, sizeof (ts_inject_config_t));

    while (read_one_line (&line, f) != 0)
    {
        char *p, *parent, *child, *nodes_str;

        p = line;

        // Skip whitespace
        while (*p != '\0' && whiteness (*p))
            p++;

        // Skip comments and empty lines
        if (*p == '#' || *p == '\0')
        {
            g_free (line);
            line = NULL;
            continue;
        }

        // Check for "inject_dynamic" or "inject"
        gboolean is_dynamic = FALSE;

        if (strncmp (p, "inject_dynamic", 14) == 0 && whiteness (p[14]))
        {
            is_dynamic = TRUE;
            p += 14;
        }
        else if (strncmp (p, "inject", 6) == 0 && whiteness (p[6]))
        {
            p += 6;
        }
        else
        {
            g_free (line);
            line = NULL;
            continue;
        }

        // Skip whitespace -> parent grammar name
        while (*p != '\0' && whiteness (*p))
            p++;
        parent = p;

        // Find end of parent grammar name
        while (*p != '\0' && !whiteness (*p))
            p++;
        if (*p == '\0')
        {
            g_free (line);
            line = NULL;
            continue;
        }
        *p++ = '\0';

        // Check if this injection is for our grammar
        if (strcmp (parent, grammar_name) != 0)
        {
            g_free (line);
            line = NULL;
            continue;
        }

        if (is_dynamic)
        {
            // Format: inject_dynamic <parent> <block_node> <lang_node> <content_node>
            char *block_type, *lang_type, *content_type;
            ts_inject_config_t cfg;

            // block_type
            while (*p != '\0' && whiteness (*p))
                p++;
            block_type = p;
            while (*p != '\0' && !whiteness (*p))
                p++;
            if (*p == '\0')
            {
                g_free (line);
                line = NULL;
                continue;
            }
            *p++ = '\0';

            // lang_type
            while (*p != '\0' && whiteness (*p))
                p++;
            lang_type = p;
            while (*p != '\0' && !whiteness (*p))
                p++;
            if (*p == '\0')
            {
                g_free (line);
                line = NULL;
                continue;
            }
            *p++ = '\0';

            // content_type
            while (*p != '\0' && whiteness (*p))
                p++;
            content_type = p;
            while (*p != '\0' && !whiteness (*p))
                p++;
            *p = '\0';

            memset (&cfg, 0, sizeof (cfg));
            cfg.dynamic = TRUE;
            cfg.block_type = g_strdup (block_type);
            cfg.lang_type = g_strdup (lang_type);
            cfg.content_type = g_strdup (content_type);
            g_array_append_val (configs, cfg);
        }
        else
        {
            // Format: inject <parent> <child_grammar> <node_type1>,<node_type2>,...
            char *child, *nodes_str;
            ts_inject_config_t cfg;
            gchar **parts;
            int count, i;

            // child grammar name
            while (*p != '\0' && whiteness (*p))
                p++;
            child = p;
            while (*p != '\0' && !whiteness (*p))
                p++;
            if (*p == '\0')
            {
                g_free (line);
                line = NULL;
                continue;
            }
            *p++ = '\0';

            // comma-separated node types
            while (*p != '\0' && whiteness (*p))
                p++;
            nodes_str = p;

            parts = g_strsplit (nodes_str, ",", -1);
            count = (int) g_strv_length (parts);

            memset (&cfg, 0, sizeof (cfg));
            cfg.dynamic = FALSE;
            cfg.grammar_name = g_strdup (child);
            cfg.node_types = g_new0 (char *, count + 1);
            for (i = 0; i < count; i++)
                cfg.node_types[i] = g_strstrip (g_strdup (parts[i]));
            cfg.node_types[count] = NULL;

            g_strfreev (parts);
            g_array_append_val (configs, cfg);
        }

        g_free (line);
        line = NULL;
    }

    g_free (line);
    fclose (f);

    if (configs->len == 0)
    {
        g_array_free (configs, TRUE);
        return NULL;
    }

    return configs;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Initialize injection parsers/queries for a grammar.
 * Called after the primary grammar is initialized.
 * Failure is non-fatal — highlighting works without injections.
 */
static void
ts_init_injections (WEdit *edit, const char *grammar_name)
{
    GArray *configs;
    guint i;

    configs = ts_find_injections (grammar_name);
    if (configs == NULL)
        return;

    edit->ts_injections = g_array_new (FALSE, TRUE, sizeof (ts_injection_t));

    for (i = 0; i < configs->len; i++)
    {
        ts_inject_config_t *cfg = &g_array_index (configs, ts_inject_config_t, i);
        ts_injection_t inj;

        memset (&inj, 0, sizeof (inj));

        if (cfg->dynamic)
        {
            // Dynamic injection: no parser/query created now — they're created
            // on demand per language in ts_rebuild_highlight_cache.
            inj.dynamic = TRUE;
            inj.block_type = cfg->block_type;
            inj.lang_type = cfg->lang_type;
            inj.content_type = cfg->content_type;
            inj.lang_cache = g_hash_table_new (g_str_hash, g_str_equal);
            cfg->block_type = NULL;     // ownership transferred
            cfg->lang_type = NULL;
            cfg->content_type = NULL;
            g_array_append_val (edit->ts_injections, inj);
        }
        else
        {
            // Static injection: create parser/query now
            const TSLanguage *lang;
            TSParser *parser;
            char *query_filename;
            char *query_src;
            uint32_t query_len;
            uint32_t error_offset;
            TSQueryError error_type;
            TSQuery *query;

            lang = ts_grammar_registry_lookup (cfg->grammar_name);
            if (lang == NULL)
                goto skip;

            parser = ts_parser_new ();
            if (!ts_parser_set_language (parser, lang))
            {
                ts_parser_delete (parser);
                goto skip;
            }

            ts_parser_set_timeout_micros (parser, 3000000);

            query_filename = g_strdup_printf ("%s-highlights.scm", cfg->grammar_name);
            query_src = ts_load_query_file (query_filename, &query_len);
            g_free (query_filename);

            if (query_src == NULL)
            {
                ts_parser_delete (parser);
                goto skip;
            }

            query = ts_query_new (lang, query_src, query_len, &error_offset, &error_type);
            g_free (query_src);

            if (query == NULL)
            {
                ts_parser_delete (parser);
                goto skip;
            }

            inj.parser = parser;
            inj.query = query;
            inj.node_types = cfg->node_types;
            cfg->node_types = NULL;     // ownership transferred
            g_array_append_val (edit->ts_injections, inj);
            g_free (cfg->grammar_name);
            continue;

          skip:
            g_free (cfg->grammar_name);
            g_strfreev (cfg->node_types);
        }
    }

    g_array_free (configs, TRUE);

    if (edit->ts_injections->len == 0)
    {
        g_array_free (edit->ts_injections, TRUE);
        edit->ts_injections = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Try to initialize tree-sitter for the given edit widget.
 * Returns TRUE on success, FALSE if we should fall back to legacy highlighting.
 *
 * Grammar discovery:
 *   1. Config file ts-grammars maps filename regex -> grammar_name
 *   2. Grammar looked up in static registry (compiled-in)
 *   3. Query file by convention: <name>-highlights.scm
 */
static gboolean
ts_init_for_file (WEdit *edit)
{
    const char *filename;
    char *grammar_name = NULL;
    char *display_name = NULL;
    char *query_filename = NULL;
    const TSLanguage *lang;
    TSParser *parser;
    TSTree *tree;
    TSInput input;
    char *query_src;
    uint32_t query_len;
    uint32_t error_offset;
    TSQueryError error_type;
    TSQuery *query;

    filename = vfs_path_as_str (edit->filename_vpath);

    if (!ts_find_grammar (filename, get_first_editor_line (edit), &grammar_name, &display_name))
        return FALSE;

    // Look up grammar in the static registry
    lang = ts_grammar_registry_lookup (grammar_name);
    if (lang == NULL)
    {
        g_free (grammar_name);
        g_free (display_name);
        return FALSE;
    }

    // Create parser and set language
    parser = ts_parser_new ();
    if (!ts_parser_set_language (parser, lang))
    {
        ts_parser_delete (parser);
        g_free (grammar_name);
        g_free (display_name);
        return FALSE;
    }

    // Set a timeout to prevent pathological grammars from freezing the editor
    ts_parser_set_timeout_micros (parser, 3000000);  // 3 seconds

    // Parse the buffer
    input.payload = edit;
    input.read = ts_input_read;
    input.encoding = TSInputEncodingUTF8;

    tree = ts_parser_parse (parser, NULL, input);
    if (tree == NULL)
    {
        ts_parser_delete (parser);
        g_free (grammar_name);
        g_free (display_name);
        return FALSE;
    }

    // Load and compile highlight query: <name>-highlights.scm
    query_filename = g_strdup_printf ("%s-highlights.scm", grammar_name);
    query_src = ts_load_query_file (query_filename, &query_len);
    g_free (query_filename);

    if (query_src == NULL)
    {
        ts_tree_delete (tree);
        ts_parser_delete (parser);
        g_free (grammar_name);
        g_free (display_name);
        return FALSE;
    }

    query = ts_query_new (lang, query_src, query_len, &error_offset, &error_type);
    g_free (query_src);

    if (query == NULL)
    {
        ts_tree_delete (tree);
        ts_parser_delete (parser);
        g_free (grammar_name);
        g_free (display_name);
        return FALSE;
    }

    // All good -- store in edit widget
    edit->ts_parser = parser;
    edit->ts_tree = tree;
    edit->ts_highlight_query = query;
    edit->ts_highlights = g_array_new (FALSE, FALSE, sizeof (ts_highlight_entry_t));
    edit->ts_highlights_start = -1;
    edit->ts_highlights_end = -1;
    edit->ts_active = TRUE;
    edit->ts_need_reparse = FALSE;

    // Try to initialize language injection (e.g., markdown inline within markdown block)
    // Failure is non-fatal — highlighting works without injection.
    ts_init_injections (edit, grammar_name);

    g_free (edit->syntax_type);
    edit->syntax_type = display_name;  // takes ownership

    g_free (grammar_name);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Free all tree-sitter resources associated with the edit widget.
 */
static void
ts_free (WEdit *edit)
{
    // Free injection resources
    if (edit->ts_injections != NULL)
    {
        guint i;

        for (i = 0; i < edit->ts_injections->len; i++)
        {
            ts_injection_t *inj = &g_array_index (edit->ts_injections, ts_injection_t, i);

            if (inj->dynamic)
            {
                g_free (inj->block_type);
                g_free (inj->lang_type);
                g_free (inj->content_type);
                if (inj->lang_cache != NULL)
                {
                    GHashTableIter iter;
                    gpointer key, value;

                    g_hash_table_iter_init (&iter, inj->lang_cache);
                    while (g_hash_table_iter_next (&iter, &key, &value))
                    {
                        ts_dynamic_lang_t *dl = (ts_dynamic_lang_t *) value;

                        if (dl->query != NULL)
                            ts_query_delete ((TSQuery *) dl->query);
                        if (dl->parser != NULL)
                            ts_parser_delete ((TSParser *) dl->parser);
                        g_free (dl);
                        g_free (key);
                    }
                    g_hash_table_destroy (inj->lang_cache);
                }
            }
            else
            {
                if (inj->query != NULL)
                    ts_query_delete ((TSQuery *) inj->query);
                if (inj->parser != NULL)
                    ts_parser_delete ((TSParser *) inj->parser);
                if (inj->node_types != NULL)
                    g_strfreev (inj->node_types);
            }
        }
        g_array_free (edit->ts_injections, TRUE);
        edit->ts_injections = NULL;
    }

    // Free primary resources
    if (edit->ts_highlight_query != NULL)
    {
        ts_query_delete ((TSQuery *) edit->ts_highlight_query);
        edit->ts_highlight_query = NULL;
    }

    if (edit->ts_tree != NULL)
    {
        ts_tree_delete ((TSTree *) edit->ts_tree);
        edit->ts_tree = NULL;
    }

    if (edit->ts_parser != NULL)
    {
        ts_parser_delete ((TSParser *) edit->ts_parser);
        edit->ts_parser = NULL;
    }

    if (edit->ts_highlights != NULL)
    {
        g_array_free (edit->ts_highlights, TRUE);
        edit->ts_highlights = NULL;
    }

    edit->ts_highlights_start = -1;
    edit->ts_highlights_end = -1;
    edit->ts_active = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Append a TSRange for the given node to the ranges array.
 */
static void
ts_append_node_range (TSNode node, GArray *ranges)
{
    TSRange r;

    r.start_point = ts_node_start_point (node);
    r.end_point = ts_node_end_point (node);
    r.start_byte = ts_node_start_byte (node);
    r.end_byte = ts_node_end_byte (node);
    g_array_append_val (ranges, r);
}

/**
 * Recursively collect byte ranges of nodes matching the injection node types.
 * Ranges are appended to the GArray of TSRange and are ordered by byte offset.
 *
 * Node types support two formats:
 *   "node_type"           — match nodes of this type directly
 *   "parent_type/child_type" — match child_type nodes inside parent_type nodes
 *     (used for HTML: "script_element/raw_text" targets the raw_text inside <script>)
 */
static void
ts_collect_injection_ranges (TSNode node, char **node_types, uint32_t range_start,
                             uint32_t range_end, GArray *ranges)
{
    const char *type;
    char **nt;
    uint32_t child_count, i;

    // Skip nodes entirely outside the range of interest
    if (ts_node_end_byte (node) <= range_start || ts_node_start_byte (node) >= range_end)
        return;

    type = ts_node_type (node);

    for (nt = node_types; *nt != NULL; nt++)
    {
        const char *slash = strchr (*nt, '/');

        if (slash != NULL)
        {
            // parent/child format: check if this node matches the parent type
            size_t parent_len = (size_t) (slash - *nt);

            if (strncmp (type, *nt, parent_len) == 0 && type[parent_len] == '\0')
            {
                const char *child_type = slash + 1;

                // Collect matching child nodes
                child_count = ts_node_child_count (node);
                for (i = 0; i < child_count; i++)
                {
                    TSNode child = ts_node_child (node, i);

                    if (strcmp (ts_node_type (child), child_type) == 0)
                        ts_append_node_range (child, ranges);
                }
                return;     // Don't recurse further into matched parent
            }
        }
        else if (strcmp (type, *nt) == 0)
        {
            // Simple match
            ts_append_node_range (node, ranges);
            return;         // Don't recurse into injection target nodes
        }
    }

    // Recurse into children
    child_count = ts_node_child_count (node);
    for (i = 0; i < child_count; i++)
    {
        TSNode child = ts_node_child (node, i);
        ts_collect_injection_ranges (child, node_types, range_start, range_end, ranges);
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Run a highlight query on a tree and append results to the highlights array.
 */
static void
ts_run_query_into_highlights (TSQuery *query, TSTree *tree, uint32_t range_start,
                              uint32_t range_end, GArray *highlights)
{
    TSNode root;
    TSQueryCursor *cursor;
    TSQueryMatch match;

    root = ts_tree_root_node (tree);

    cursor = ts_query_cursor_new ();
    ts_query_cursor_set_byte_range (cursor, range_start, range_end);
    ts_query_cursor_exec (cursor, query, root);

    while (ts_query_cursor_next_match (cursor, &match))
    {
        uint32_t ci;

        for (ci = 0; ci < match.capture_count; ci++)
        {
            TSQueryCapture cap = match.captures[ci];
            uint32_t cap_name_len;
            const char *cap_name;
            ts_highlight_entry_t entry;

            cap_name = ts_query_capture_name_for_id (query, cap.index, &cap_name_len);

            entry.start_byte = ts_node_start_byte (cap.node);
            entry.end_byte = ts_node_end_byte (cap.node);
            entry.color = ts_capture_name_to_color (cap_name);

            // Only include entries that overlap with our range
            if (entry.end_byte > range_start && entry.start_byte < range_end)
                g_array_append_val (highlights, entry);
        }
    }

    ts_query_cursor_delete (cursor);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Look up or create a cached parser+query for a dynamically-injected language.
 * Returns NULL if the language is unknown or the query fails to compile.
 */
static ts_dynamic_lang_t *
ts_get_dynamic_lang (ts_injection_t *inj, const char *lang_name)
{
    ts_dynamic_lang_t *dl;
    const TSLanguage *lang;
    TSParser *parser;
    char *query_filename;
    char *query_src;
    uint32_t query_len;
    uint32_t error_offset;
    TSQueryError error_type;
    TSQuery *query;

    dl = (ts_dynamic_lang_t *) g_hash_table_lookup (inj->lang_cache, lang_name);
    if (dl != NULL)
        return dl->parser != NULL ? dl : NULL;

    // Not cached yet — try to create
    lang = ts_grammar_registry_lookup (lang_name);
    if (lang == NULL)
    {
        // Cache a NULL entry so we don't retry
        dl = g_new0 (ts_dynamic_lang_t, 1);
        g_hash_table_insert (inj->lang_cache, g_strdup (lang_name), dl);
        return NULL;
    }

    parser = ts_parser_new ();
    if (!ts_parser_set_language (parser, lang))
    {
        ts_parser_delete (parser);
        dl = g_new0 (ts_dynamic_lang_t, 1);
        g_hash_table_insert (inj->lang_cache, g_strdup (lang_name), dl);
        return NULL;
    }

    ts_parser_set_timeout_micros (parser, 3000000);

    query_filename = g_strdup_printf ("%s-highlights.scm", lang_name);
    query_src = ts_load_query_file (query_filename, &query_len);
    g_free (query_filename);

    if (query_src == NULL)
    {
        ts_parser_delete (parser);
        dl = g_new0 (ts_dynamic_lang_t, 1);
        g_hash_table_insert (inj->lang_cache, g_strdup (lang_name), dl);
        return NULL;
    }

    query = ts_query_new (lang, query_src, query_len, &error_offset, &error_type);
    g_free (query_src);

    if (query == NULL)
    {
        ts_parser_delete (parser);
        dl = g_new0 (ts_dynamic_lang_t, 1);
        g_hash_table_insert (inj->lang_cache, g_strdup (lang_name), dl);
        return NULL;
    }

    dl = g_new0 (ts_dynamic_lang_t, 1);
    dl->parser = parser;
    dl->query = query;
    g_hash_table_insert (inj->lang_cache, g_strdup (lang_name), dl);
    return dl;
}

/**
 * Process a dynamic injection: walk the primary tree for block nodes,
 * read the language from a child node, parse the content with that language.
 */
static void
ts_run_dynamic_injection (ts_injection_t *inj, TSNode root, WEdit *edit, TSInput input,
                          uint32_t range_start, uint32_t range_end, GArray *highlights)
{
    GArray *stack;

    stack = g_array_new (FALSE, FALSE, sizeof (TSNode));
    g_array_append_val (stack, root);

    while (stack->len > 0)
    {
        TSNode node = g_array_index (stack, TSNode, stack->len - 1);
        const char *type;
        uint32_t child_count, ci;

        g_array_set_size (stack, stack->len - 1);

        // Skip nodes outside the range of interest
        if (ts_node_end_byte (node) <= range_start || ts_node_start_byte (node) >= range_end)
            continue;

        type = ts_node_type (node);

        if (strcmp (type, inj->block_type) == 0)
        {
            // Found a code block — extract language name and content range
            TSNode lang_node = { .id = NULL };
            TSNode content_node = { .id = NULL };

            child_count = ts_node_child_count (node);
            for (ci = 0; ci < child_count; ci++)
            {
                TSNode child = ts_node_child (node, ci);
                const char *ctype = ts_node_type (child);

                if (strcmp (ctype, inj->lang_type) == 0)
                    lang_node = child;
                else if (strcmp (ctype, inj->content_type) == 0)
                    content_node = child;
            }

            if (!ts_node_is_null (lang_node) && !ts_node_is_null (content_node))
            {
                uint32_t lang_start = ts_node_start_byte (lang_node);
                uint32_t lang_end = ts_node_end_byte (lang_node);
                uint32_t lang_len = lang_end - lang_start;

                if (lang_len > 0 && lang_len < 64)
                {
                    // Read the language name from the edit buffer
                    char lang_name[64];
                    uint32_t li;

                    for (li = 0; li < lang_len; li++)
                        lang_name[li] =
                            (char) edit_buffer_get_byte (&edit->buffer,
                                                        (off_t) (lang_start + li));
                    lang_name[lang_len] = '\0';

                    // Strip leading/trailing whitespace
                    {
                        char *s = lang_name;
                        char *e;

                        while (*s == ' ' || *s == '\t')
                            s++;
                        e = s + strlen (s);
                        while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
                            e--;
                        *e = '\0';

                        if (*s != '\0')
                        {
                            ts_dynamic_lang_t *dl = ts_get_dynamic_lang (inj, s);

                            if (dl != NULL)
                            {
                                TSRange r;
                                TSTree *inject_tree;

                                r.start_point = ts_node_start_point (content_node);
                                r.end_point = ts_node_end_point (content_node);
                                r.start_byte = ts_node_start_byte (content_node);
                                r.end_byte = ts_node_end_byte (content_node);

                                ts_parser_set_included_ranges ((TSParser *) dl->parser, &r, 1);
                                inject_tree =
                                    ts_parser_parse ((TSParser *) dl->parser, NULL, input);
                                if (inject_tree != NULL)
                                {
                                    ts_run_query_into_highlights ((TSQuery *) dl->query,
                                                                 inject_tree,
                                                                 range_start, range_end,
                                                                 highlights);
                                    ts_tree_delete (inject_tree);
                                }
                            }
                        }
                    }
                }
            }
            continue;       // Don't recurse into code blocks
        }

        // Recurse into children
        child_count = ts_node_child_count (node);
        for (ci = 0; ci < child_count; ci++)
        {
            TSNode child = ts_node_child (node, ci);
            g_array_append_val (stack, child);
        }
    }

    g_array_free (stack, TRUE);
}

/**
 * Rebuild the highlight cache for the given byte range.
 * Runs the highlight query and collects (start_byte, end_byte, color) entries.
 * If injection is active, also runs the injection query on target node ranges.
 */
static void
ts_rebuild_highlight_cache (WEdit *edit, off_t range_start, off_t range_end)
{
    TSTree *tree;
    TSInput input;

    if (!edit->ts_active)
        return;

    input.payload = edit;
    input.read = ts_input_read;
    input.encoding = TSInputEncodingUTF8;

    // Perform deferred re-parse if the tree was edited since last parse
    if (edit->ts_need_reparse)
    {
        TSTree *new_tree;

        new_tree =
            ts_parser_parse ((TSParser *) edit->ts_parser, (TSTree *) edit->ts_tree, input);
        if (new_tree != NULL)
        {
            ts_tree_delete ((TSTree *) edit->ts_tree);
            edit->ts_tree = new_tree;
        }

        edit->ts_need_reparse = FALSE;
    }

    tree = (TSTree *) edit->ts_tree;

    g_array_set_size (edit->ts_highlights, 0);

    // Run the primary highlight query
    ts_run_query_into_highlights ((TSQuery *) edit->ts_highlight_query, tree,
                                 (uint32_t) range_start, (uint32_t) range_end,
                                 edit->ts_highlights);

    // Run injection queries if configured.
    // Each injection range is parsed separately to avoid the inline parser's
    // scanner state leaking across disjoint ranges (e.g., backtick matching
    // crossing from one list item's inline content to another's).
    if (edit->ts_injections != NULL)
    {
        TSNode root;
        guint ji;

        root = ts_tree_root_node (tree);

        for (ji = 0; ji < edit->ts_injections->len; ji++)
        {
            ts_injection_t *inj = &g_array_index (edit->ts_injections, ts_injection_t, ji);

            if (inj->dynamic)
            {
                ts_run_dynamic_injection (inj, root, edit, input,
                                         (uint32_t) range_start, (uint32_t) range_end,
                                         edit->ts_highlights);
            }
            else
            {
                GArray *ranges;
                guint ri;

                ranges = g_array_new (FALSE, FALSE, sizeof (TSRange));

                ts_collect_injection_ranges (root, inj->node_types,
                                            (uint32_t) range_start, (uint32_t) range_end, ranges);

                for (ri = 0; ri < ranges->len; ri++)
                {
                    TSRange *r = &g_array_index (ranges, TSRange, ri);
                    TSTree *inject_tree;

                    ts_parser_set_included_ranges ((TSParser *) inj->parser, r, 1);

                    inject_tree = ts_parser_parse ((TSParser *) inj->parser, NULL, input);
                    if (inject_tree != NULL)
                    {
                        ts_run_query_into_highlights ((TSQuery *) inj->query,
                                                     inject_tree,
                                                     (uint32_t) range_start, (uint32_t) range_end,
                                                     edit->ts_highlights);
                        ts_tree_delete (inject_tree);
                    }
                }

                g_array_free (ranges, TRUE);
            }
        }
    }

    edit->ts_highlights_start = range_start;
    edit->ts_highlights_end = range_end;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Look up the color for a byte index in the highlight cache.
 * Returns the color of the most specific (last) matching entry,
 * since tree-sitter queries return matches in order from general to specific.
 */
static int
ts_get_color_at (WEdit *edit, off_t byte_index)
{
    guint i;
    int color = EDITOR_NORMAL_COLOR;

    if (edit->ts_highlights == NULL)
        return EDITOR_NORMAL_COLOR;

    for (i = 0; i < edit->ts_highlights->len; i++)
    {
        ts_highlight_entry_t *e;

        e = &g_array_index (edit->ts_highlights, ts_highlight_entry_t, i);

        if ((off_t) e->start_byte <= byte_index && byte_index < (off_t) e->end_byte)
            color = e->color;
    }

    return color;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Notify tree-sitter that the buffer was edited.
 * Called after insert/delete operations.
 *
 * Only records the edit in the tree (ts_tree_edit) and marks the tree as needing
 * re-parse.  The actual re-parse is deferred to ts_rebuild_highlight_cache() so
 * that bulk operations (block delete + insert) don't re-parse on every character.
 */
void
edit_syntax_ts_notify_edit (WEdit *edit, off_t start_byte, off_t old_end_byte,
                            off_t new_end_byte)
{
    TSInputEdit ts_edit;

    if (!edit->ts_active || edit->ts_tree == NULL || edit->ts_parser == NULL)
        return;

    ts_edit.start_byte = (uint32_t) start_byte;
    ts_edit.old_end_byte = (uint32_t) old_end_byte;
    ts_edit.new_end_byte = (uint32_t) new_end_byte;
    ts_edit.start_point = (TSPoint){ 0, 0 };
    ts_edit.old_end_point = (TSPoint){ 0, 0 };
    ts_edit.new_end_point = (TSPoint){ 0, 0 };

    ts_tree_edit ((TSTree *) edit->ts_tree, &ts_edit);

    // Mark tree as needing re-parse; defer actual parsing to cache rebuild
    edit->ts_need_reparse = TRUE;

    // Invalidate highlight cache
    edit->ts_highlights_start = -1;
    edit->ts_highlights_end = -1;
}

#endif /* HAVE_TREE_SITTER */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
edit_get_syntax_color (WEdit *edit, off_t byte_index)
{
    if (!tty_use_colors ())
        return 0;

#ifdef HAVE_TREE_SITTER
    if (edit_options.syntax_highlighting && edit->ts_active && byte_index < edit->buffer.size)
    {
        // Check if we need to rebuild the highlight cache
        // Use a window of +/- 8K around the requested byte
        if (edit->ts_highlights_start < 0 || byte_index < edit->ts_highlights_start
            || byte_index >= edit->ts_highlights_end)
        {
            off_t range_start = byte_index - 8192;
            off_t range_end = byte_index + 8192;

            if (range_start < 0)
                range_start = 0;
            if (range_end > edit->buffer.size)
                range_end = edit->buffer.size;

            ts_rebuild_highlight_cache (edit, range_start, range_end);
        }

        return ts_get_color_at (edit, byte_index);
    }
#endif

    // Legacy fallback
    if (edit_options.syntax_highlighting && edit->rules != NULL && byte_index < edit->buffer.size)
    {
        edit_get_rule (edit, byte_index);
        return translate_rule_to_color (edit, &edit->rule);
    }

    return EDITOR_NORMAL_COLOR;
}

/* --------------------------------------------------------------------------------------------- */

void
edit_free_syntax_rules (WEdit *edit)
{
    if (edit == NULL)
        return;

#ifdef HAVE_TREE_SITTER
    ts_free (edit);
#endif

    if (edit->defines != NULL)
        destroy_defines (&edit->defines);

    if (edit->rules == NULL)
    {
        MC_PTR_FREE (edit->syntax_type);
        return;
    }

    edit_get_rule (edit, -1);
    MC_PTR_FREE (edit->syntax_type);

    g_ptr_array_free (edit->rules, TRUE);
    edit->rules = NULL;
    g_clear_slist (&edit->syntax_marker, g_free);
    tty_color_free_temp ();
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Load rules into edit struct.  Either edit or *pnames must be NULL.  If
 * edit is NULL, a list of types will be stored into names.  If type is
 * NULL, then the type will be selected according to the filename.
 * type must be edit->syntax_type or NULL
 */
void
edit_load_syntax (WEdit *edit, GPtrArray *pnames, const char *type)
{
    int r;
    char *f = NULL;

    if (auto_syntax)
        type = NULL;

    if (edit != NULL)
    {
        char *saved_type;

        saved_type = g_strdup (type);  // save edit->syntax_type
        edit_free_syntax_rules (edit);
        edit->syntax_type = saved_type;  // restore edit->syntax_type
    }

    if (!tty_use_colors ())
        return;

    if (!edit_options.syntax_highlighting && (pnames == NULL || pnames->len == 0))
        return;

    if (edit != NULL && edit->filename_vpath == NULL)
        return;

#ifdef HAVE_TREE_SITTER
    // Try tree-sitter first (only for actual file loading, not name collection)
    if (edit != NULL && pnames == NULL && type == NULL)
    {
        if (ts_init_for_file (edit))
            return;  // tree-sitter successfully initialized
    }
#endif

    // Fall back to legacy syntax highlighting
    f = mc_config_get_full_path (EDIT_SYNTAX_FILE);
    if (edit != NULL)
        r = edit_read_syntax_file (edit, pnames, f, vfs_path_as_str (edit->filename_vpath),
                                   get_first_editor_line (edit),
                                   auto_syntax ? NULL : edit->syntax_type);
    else
        r = edit_read_syntax_file (NULL, pnames, f, NULL, "", NULL);
    if (r == -1)
    {
#ifdef HAVE_TREE_SITTER
        // When tree-sitter is active, silently skip if legacy Syntax file is not found
        edit_free_syntax_rules (edit);
#else
        edit_free_syntax_rules (edit);
        file_error_message (_ ("Cannot open file\n%s"), f);
#endif
    }
    else if (r != 0)
    {
        edit_free_syntax_rules (edit);
        message (D_ERROR, _ ("Load syntax file"), _ ("Error in file %s on line %d"),
                 error_file_name != NULL ? error_file_name : f, r);
        MC_PTR_FREE (error_file_name);
    }

    g_free (f);
}

/* --------------------------------------------------------------------------------------------- */

const char *
edit_get_syntax_type (const WEdit *edit)
{
    return edit->syntax_type;
}

/* --------------------------------------------------------------------------------------------- */

void
edit_syntax_dialog (WEdit *edit)
{
    GPtrArray *names;
    int syntax;

    names = g_ptr_array_new_with_free_func (g_free);

    /* We fill the list of syntax files every time the editor is invoked.
       Instead we could save the list to a file and update it once the syntax
       file gets updated (either by testing or by explicit user command). */
    edit_load_syntax (NULL, names, NULL);
    g_ptr_array_sort (names, pstrcmp);

    syntax = exec_edit_syntax_dialog (names, edit->syntax_type);
    if (syntax >= 0)
    {
        gboolean force_reload = FALSE;
        char *current_syntax;
        gboolean old_auto_syntax;

        current_syntax = g_strdup (edit->syntax_type);
        old_auto_syntax = auto_syntax;

        switch (syntax)
        {
        case 0:  // auto syntax
            auto_syntax = TRUE;
            break;
        case 1:  // reload current syntax
            force_reload = TRUE;
            break;
        default:
            auto_syntax = FALSE;
            g_free (edit->syntax_type);
            edit->syntax_type = g_strdup (g_ptr_array_index (names, syntax - N_DFLT_ENTRIES));
        }

        // Load or unload syntax rules if the option has changed
        if (force_reload || (auto_syntax && !old_auto_syntax) || old_auto_syntax
            || (current_syntax != NULL && edit->syntax_type != NULL
                && strcmp (current_syntax, edit->syntax_type) != 0))
            edit_load_syntax (edit, NULL, edit->syntax_type);

        g_free (current_syntax);
    }

    g_ptr_array_free (names, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
