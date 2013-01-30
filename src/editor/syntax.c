/*
   Editor syntax highlighting.

   Copyright (C) 1996, 1997, 1998, 2001, 2002, 2003, 2004, 2005, 2006,
   2007, 2010, 2011
   The Free Software Foundation, Inc.

   Written by:
   Paul Sheer, 1998
   Egmont Koblinger <egmont@gmail.com>, 2010

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
 *  \brief Source: editor syntax highlighting
 *  \author Paul Sheer
 *  \date 1996, 1997
 *  \author Mikhail Pobolovets
 *  \date 2010
 *
 *  Mispelled words are flushed from the syntax highlighting rules
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
#include "lib/search.h"         /* search engine */
#include "lib/skin.h"
#include "lib/strutil.h"        /* utf string functions */
#include "lib/util.h"
#include "lib/widget.h"         /* message() */

#include "edit-impl.h"
#include "editwidget.h"

/*** global variables ****************************************************************************/

int option_syntax_highlighting = 1;
int option_auto_syntax = 1;

/*** file scope macro definitions ****************************************************************/

/* bytes */
#define SYNTAX_MARKER_DENSITY 512

#define TRANSIENT_WORD_TIME_OUT 60

#define UNKNOWN_FORMAT "unknown"

#define MAX_WORDS_PER_CONTEXT   1024
#define MAX_CONTEXTS            128

#define RULE_ON_LEFT_BORDER 1
#define RULE_ON_RIGHT_BORDER 2

#define SYNTAX_TOKEN_STAR       '\001'
#define SYNTAX_TOKEN_PLUS       '\002'
#define SYNTAX_TOKEN_BRACKET    '\003'
#define SYNTAX_TOKEN_BRACE      '\004'

#define whiteness(x) ((x) == '\t' || (x) == '\n' || (x) == ' ')

#define free_args(x)
#define break_a {result=line;break;}
#define check_a {if(!*a){result=line;break;}}
#define check_not_a {if(*a){result=line;break;}}

/*** file scope type declarations ****************************************************************/

struct key_word
{
    char *keyword;
    unsigned char first;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
    long line_start;
    int color;
};

struct context_rule
{
    char *left;
    unsigned char first_left;
    char *right;
    unsigned char first_right;
    char line_start_left;
    char line_start_right;
    int between_delimiters;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
    char *keyword_first_chars;
    gboolean spelling;
    /* first word is word[1] */
    struct key_word **keyword;
};

typedef struct
{
    off_t offset;
    edit_syntax_rule_t rule;
} syntax_marker_t;

/*** file scope variables ************************************************************************/

static char *error_file_name = NULL;

/*** file scope functions ************************************************************************/
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
destroy_defines (GTree ** defines)
{
    g_tree_foreach (*defines, mc_defines_destroy, NULL);
    g_tree_destroy (*defines);
    *defines = NULL;
}

/* --------------------------------------------------------------------------------------------- */

/** Wrapper for case insensitive mode */
inline static int
xx_tolower (const WEdit * edit, int c)
{
    return edit->is_case_insensitive ? tolower (c) : c;
}

/* --------------------------------------------------------------------------------------------- */

static void
subst_defines (GTree * defines, char **argv, char **argv_end)
{
    char **t, **p;
    int argc;

    while (*argv != NULL && argv < argv_end)
    {
        t = g_tree_lookup (defines, *argv);
        if (t != NULL)
        {
            int count = 0;

            /* Count argv array members */
            argc = 0;
            for (p = &argv[1]; *p != NULL; p++)
                argc++;

            /* Count members of definition array */
            for (p = t; *p != NULL; p++)
                count++;
            p = &argv[count + argc];

            /* Buffer overflow or infinitive loop in define */
            if (p >= argv_end)
                break;

            /* Move rest of argv after definition members */
            while (argc >= 0)
                *p-- = argv[argc-- + 1];

            /* Copy definition members to argv */
            for (p = argv; *t != NULL; *p++ = *t++)
                ;
        }
        argv++;
    }
}

/* --------------------------------------------------------------------------------------------- */

static off_t
compare_word_to_right (const WEdit * edit, off_t i, const char *text,
                       const char *whole_left, const char *whole_right, long line_start)
{
    const unsigned char *p, *q;
    int c, d, j;

    if (*text == '\0')
        return -1;

    c = xx_tolower (edit, edit_get_byte (edit, i - 1));
    if ((line_start != 0 && c != '\n') || (whole_left != NULL && strchr (whole_left, c) != NULL))
        return -1;

    for (p = (unsigned char *) text, q = p + str_term_width1 ((char *) p); p < q; p++, i++)
    {
        switch (*p)
        {
        case SYNTAX_TOKEN_STAR:
            if (++p > q)
                return -1;
            while (TRUE)
            {
                c = xx_tolower (edit, edit_get_byte (edit, i));
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
                c = xx_tolower (edit, edit_get_byte (edit, i));
                if (c == *p)
                {
                    j = i;
                    if (*p == *text && p[1] == '\0')    /* handle eg '+' and @+@ keywords properly */
                        break;
                }
                if (j != 0 && strchr ((char *) p + 1, c) != NULL)       /* c exists further down, so it will get matched later */
                    break;
                if (c == '\n' || c == '\t' || c == ' ' ||
                    (whole_right != NULL && strchr (whole_right, c) == NULL))
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
                c = xx_tolower (edit, edit_get_byte (edit, i));
                for (j = 0; p[j] != SYNTAX_TOKEN_BRACKET && p[j]; j++)
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
            c = xx_tolower (edit, edit_get_byte (edit, i));
            for (; *p != SYNTAX_TOKEN_BRACE && *p; p++)
                if (c == *p)
                    goto found_char3;
            return -1;
          found_char3:
            while (*p != SYNTAX_TOKEN_BRACE && p < q)
                p++;
            break;
        default:
            if (*p != xx_tolower (edit, edit_get_byte (edit, i)))
                return -1;
        }
    }
    return (whole_right != NULL &&
            strchr (whole_right, xx_tolower (edit, edit_get_byte (edit, i))) != NULL) ? -1 : i;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
xx_strchr (const WEdit * edit, const unsigned char *s, int char_byte)
{
    while (*s >= '\005' && xx_tolower (edit, *s) != char_byte)
        s++;

    return (const char *) s;
}

/* --------------------------------------------------------------------------------------------- */

static edit_syntax_rule_t
apply_rules_going_right (WEdit * edit, off_t i, edit_syntax_rule_t rule)
{
    struct context_rule *r;
    int c;
    gboolean contextchanged = FALSE;
    gboolean found_left = FALSE, found_right = FALSE;
    gboolean keyword_foundleft = FALSE, keyword_foundright = FALSE;
    gboolean is_end;
    off_t end = 0;
    edit_syntax_rule_t _rule = rule;

    c = xx_tolower (edit, edit_get_byte (edit, i));
    if (c == 0)
        return rule;
    is_end = (rule.end == i);

    /* check to turn off a keyword */
    if (_rule.keyword)
    {
        if (edit_get_byte (edit, i - 1) == '\n')
            _rule.keyword = 0;
        if (is_end)
        {
            _rule.keyword = 0;
            keyword_foundleft = TRUE;
        }
    }

    /* check to turn off a context */
    if (_rule.context && !_rule.keyword)
    {
        off_t e;

        r = edit->rules[_rule.context];
        if (r->first_right == c && !(rule.border & RULE_ON_RIGHT_BORDER)
            && (e =
                compare_word_to_right (edit, i, r->right, r->whole_word_chars_left,
                                       r->whole_word_chars_right, r->line_start_right)) > 0)
        {
            _rule.end = e;
            found_right = TRUE;
            _rule.border = RULE_ON_RIGHT_BORDER;
            if (r->between_delimiters)
                _rule.context = 0;
        }
        else if (is_end && rule.border & RULE_ON_RIGHT_BORDER)
        {
            /* always turn off a context at 4 */
            found_left = TRUE;
            _rule.border = 0;
            if (!keyword_foundleft)
                _rule.context = 0;
        }
        else if (is_end && rule.border & RULE_ON_LEFT_BORDER)
        {
            /* never turn off a context at 2 */
            found_left = TRUE;
            _rule.border = 0;
        }
    }

    /* check to turn on a keyword */
    if (!_rule.keyword)
    {
        const char *p;

        r = edit->rules[_rule.context];
        p = r->keyword_first_chars;

        if (p != NULL)
            while (*(p = xx_strchr (edit, (unsigned char *) p + 1, c)) != '\0')
            {
                struct key_word *k;
                int count;
                off_t e;

                count = p - r->keyword_first_chars;
                k = r->keyword[count];
                e = compare_word_to_right (edit, i, k->keyword, k->whole_word_chars_left,
                                           k->whole_word_chars_right, k->line_start);
                if (e > 0)
                {
                    end = e;
                    _rule.end = e;
                    _rule.keyword = count;
                    keyword_foundright = TRUE;
                    break;
                }
            }
    }

    /* check to turn on a context */
    if (!_rule.context)
    {
        if (!found_left && is_end)
        {
            if (rule.border & RULE_ON_RIGHT_BORDER)
            {
                _rule.border = 0;
                _rule.context = 0;
                contextchanged = TRUE;
                _rule.keyword = 0;

            }
            else if (rule.border & RULE_ON_LEFT_BORDER)
            {
                r = edit->rules[_rule._context];
                _rule.border = 0;
                if (r->between_delimiters)
                {
                    _rule.context = _rule._context;
                    contextchanged = TRUE;
                    _rule.keyword = 0;

                    if (r->first_right == c)
                    {
                        off_t e;

                        e = compare_word_to_right (edit, i, r->right, r->whole_word_chars_left,
                                                   r->whole_word_chars_right, r->line_start_right);
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
            int count;
            struct context_rule **rules = edit->rules;

            for (count = 1; rules[count]; count++)
            {
                r = rules[count];
                if (r->first_left == c)
                {
                    off_t e;

                    e = compare_word_to_right (edit, i, r->left, r->whole_word_chars_left,
                                               r->whole_word_chars_right, r->line_start_left);
                    if (e >= end && (!_rule.keyword || keyword_foundright))
                    {
                        _rule.end = e;
                        found_right = TRUE;
                        _rule.border = RULE_ON_LEFT_BORDER;
                        _rule._context = count;
                        if (!r->between_delimiters && !_rule.keyword)
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

    /* check again to turn on a keyword if the context switched */
    if (contextchanged && !_rule.keyword)
    {
        const char *p;

        r = edit->rules[_rule.context];
        p = r->keyword_first_chars;

        while (*(p = xx_strchr (edit, (unsigned char *) p + 1, c)) != '\0')
        {
            struct key_word *k;
            int count;
            off_t e;

            count = p - r->keyword_first_chars;
            k = r->keyword[count];
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

    return _rule;
}

/* --------------------------------------------------------------------------------------------- */

static edit_syntax_rule_t
edit_get_rule (WEdit * edit, off_t byte_index)
{
    off_t i;

    if (byte_index > edit->last_get_rule)
    {
        for (i = edit->last_get_rule + 1; i <= byte_index; i++)
        {
            off_t d = SYNTAX_MARKER_DENSITY;

            edit->rule = apply_rules_going_right (edit, i, edit->rule);

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
                    edit->rule = apply_rules_going_right (edit, i, edit->rule);
                break;
            }

            s = (syntax_marker_t *) edit->syntax_marker->data;

            if (byte_index >= s->offset)
            {
                edit->rule = s->rule;
                for (i = s->offset + 1; i <= byte_index; i++)
                    edit->rule = apply_rules_going_right (edit, i, edit->rule);
                break;
            }

            g_free (s);
            edit->syntax_marker = g_slist_delete_link (edit->syntax_marker, edit->syntax_marker);
        }
    }
    edit->last_get_rule = byte_index;
    return edit->rule;
}

/* --------------------------------------------------------------------------------------------- */

static inline int
translate_rule_to_color (const WEdit * edit, edit_syntax_rule_t rule)
{
    return edit->rules[rule.context]->keyword[rule.keyword]->color;
}

/* --------------------------------------------------------------------------------------------- */
/**
   Returns 0 on error/eof or a count of the number of bytes read
   including the newline. Result must be free'd.
   In case of an error, *line will not be modified.
 */

static size_t
read_one_line (char **line, FILE * f)
{
    GString *p;
    size_t r = 0;

    /* not reallocate string too often */
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

        /* handle all of \r\n, \r, \n correctly. */
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
this_try_alloc_color_pair (const char *fg, const char *bg, const char *attrs)
{
    char f[80], b[80], a[80], *p;

    if (bg != NULL && *bg == '\0')
        bg = NULL;
    if (fg != NULL && *fg == '\0')
        fg = NULL;
    if (attrs != NULL && *attrs == '\0')
        attrs = NULL;

    if ((fg == NULL) && (bg == NULL))
        return EDITOR_NORMAL_COLOR;

    if (fg != NULL)
    {
        g_strlcpy (f, fg, sizeof (f));
        p = strchr (f, '/');
        if (p != NULL)
            *p = '\0';
        fg = f;
    }
    if (bg != NULL)
    {
        g_strlcpy (b, bg, sizeof (b));
        p = strchr (b, '/');
        if (p != NULL)
            *p = '\0';
        bg = b;
    }
    if ((fg == NULL) || (bg == NULL))
    {
        /* get colors from skin */
        char *editnormal;

        editnormal = mc_skin_get ("editor", "_default_", "default;default");

        if (fg == NULL)
        {
            g_strlcpy (f, editnormal, sizeof (f));
            p = strchr (f, ';');
            if (p != NULL)
                *p = '\0';
            if (f[0] == '\0')
                g_strlcpy (f, "default", sizeof (f));
            fg = f;
        }
        if (bg == NULL)
        {
            p = strchr (editnormal, ';');
            if ((p != NULL) && (*(++p) != '\0'))
                g_strlcpy (b, p, sizeof (b));
            else
                g_strlcpy (b, "default", sizeof (b));
            bg = b;
        }

        g_free (editnormal);
    }

    if (attrs != NULL)
    {
        g_strlcpy (a, attrs, sizeof (a));
        p = strchr (a, '/');
        if (p != NULL)
            *p = '\0';
        /* get_args() mangles the + signs, unmangle 'em */
        p = a;
        while ((p = strchr (p, SYNTAX_TOKEN_PLUS)) != NULL)
            *p++ = '+';
        attrs = a;
    }
    return tty_try_alloc_color_pair (fg, bg, attrs);
}

/* --------------------------------------------------------------------------------------------- */

static FILE *
open_include_file (const char *filename)
{
    FILE *f;

    MC_PTR_FREE (error_file_name);
    error_file_name = g_strdup (filename);
    if (g_path_is_absolute (filename))
        return fopen (filename, "r");

    g_free (error_file_name);
    error_file_name =
        g_build_filename (mc_config_get_data_path (), EDIT_DIR, filename, (char *) NULL);
    f = fopen (error_file_name, "r");
    if (f != NULL)
        return f;

    g_free (error_file_name);
    error_file_name = g_build_filename (mc_global.sysconfig_dir, "syntax", filename, (char *) NULL);
    f = fopen (error_file_name, "r");
    if (f != NULL)
        return f;

    g_free (error_file_name);
    error_file_name =
        g_build_filename (mc_global.share_data_dir, "syntax", filename, (char *) NULL);

    return fopen (error_file_name, "r");
}

/* --------------------------------------------------------------------------------------------- */

inline static void
xx_lowerize_line (WEdit * edit, char *line, size_t len)
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
edit_read_syntax_rules (WEdit * edit, FILE * f, char **args, int args_size)
{
    FILE *g = NULL;
    char *fg, *bg, *attrs;
    char last_fg[32] = "", last_bg[32] = "", last_attrs[64] = "";
    char whole_right[512];
    char whole_left[512];
    char *l = 0;
    int save_line = 0, line = 0;
    struct context_rule **r, *c = NULL;
    int num_words = -1, num_contexts = -1;
    int result = 0;
    int argc;
    int i, j;
    int alloc_contexts = MAX_CONTEXTS,
        alloc_words_per_context = MAX_WORDS_PER_CONTEXT,
        max_alloc_words_per_context = MAX_WORDS_PER_CONTEXT;

    args[0] = NULL;
    edit->is_case_insensitive = FALSE;

    strcpy (whole_left, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");
    strcpy (whole_right, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");

    r = edit->rules = g_malloc0 (alloc_contexts * sizeof (struct context_rule *));

    if (!edit->defines)
        edit->defines = g_tree_new ((GCompareFunc) strcmp);

    while (TRUE)
    {
        char **a;
        size_t len;

        line++;
        l = 0;

        len = read_one_line (&l, f);
        if (len != 0)
            xx_lowerize_line (edit, l, len);
        else
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
            xx_lowerize_line (edit, l, len);
        }

        argc = get_args (l, args, args_size);
        a = args + 1;
        if (args[0] == NULL)
        {
            /* do nothing */
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
            check_a;
            if (num_contexts == -1)
            {
                if (strcmp (*a, "default") != 0)
                {               /* first context is the default */
                    break_a;
                }
                a++;
                c = r[0] = g_malloc0 (sizeof (struct context_rule));
                c->left = g_strdup (" ");
                c->right = g_strdup (" ");
                num_contexts = 0;
            }
            else
            {
                /* Terminate previous context.  */
                r[num_contexts - 1]->keyword[num_words] = NULL;
                c = r[num_contexts] = g_malloc0 (sizeof (struct context_rule));
                if (strcmp (*a, "exclusive") == 0)
                {
                    a++;
                    c->between_delimiters = 1;
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
                    c->line_start_left = 1;
                }
                check_a;
                c->left = g_strdup (*a++);
                check_a;
                if (strcmp (*a, "linestart") == 0)
                {
                    a++;
                    c->line_start_right = 1;
                }
                check_a;
                c->right = g_strdup (*a++);
                c->first_left = *c->left;
                c->first_right = *c->right;
            }
            c->keyword = g_malloc (alloc_words_per_context * sizeof (struct key_word *));
            num_words = 1;
            c->keyword[0] = g_malloc0 (sizeof (struct key_word));
            subst_defines (edit->defines, a, &args[1024]);
            fg = *a;
            if (*a != '\0')
                a++;
            bg = *a;
            if (*a != '\0')
                a++;
            attrs = *a;
            if (*a != '\0')
                a++;
            g_strlcpy (last_fg, fg != NULL ? fg : "", sizeof (last_fg));
            g_strlcpy (last_bg, bg != NULL ? bg : "", sizeof (last_bg));
            g_strlcpy (last_attrs, attrs != NULL ? attrs : "", sizeof (last_attrs));
            c->keyword[0]->color = this_try_alloc_color_pair (fg, bg, attrs);
            c->keyword[0]->keyword = g_strdup (" ");
            check_not_a;

            alloc_words_per_context = MAX_WORDS_PER_CONTEXT;
            if (++num_contexts >= alloc_contexts)
            {
                struct context_rule **tmp;

                alloc_contexts += 128;
                tmp = g_realloc (r, alloc_contexts * sizeof (struct context_rule *));
                r = tmp;
            }
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
            struct key_word *k;

            if (num_words == -1)
                break_a;
            check_a;
            k = r[num_contexts - 1]->keyword[num_words] = g_malloc0 (sizeof (struct key_word));
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
                k->line_start = 1;
            }
            check_a;
            if (strcmp (*a, "whole") == 0)
            {
                break_a;
            }
            k->keyword = g_strdup (*a++);
            k->first = *k->keyword;
            subst_defines (edit->defines, a, &args[1024]);
            fg = *a;
            if (*a != '\0')
                a++;
            bg = *a;
            if (*a != '\0')
                a++;
            attrs = *a;
            if (*a != '\0')
                a++;
            if (fg == NULL)
                fg = last_fg;
            if (bg == NULL)
                bg = last_bg;
            if (attrs == NULL)
                attrs = last_attrs;
            k->color = this_try_alloc_color_pair (fg, bg, attrs);
            check_not_a;

            if (++num_words >= alloc_words_per_context)
            {
                struct key_word **tmp;

                alloc_words_per_context += 1024;

                if (alloc_words_per_context > max_alloc_words_per_context)
                    max_alloc_words_per_context = alloc_words_per_context;

                tmp = g_realloc (c->keyword, alloc_words_per_context * sizeof (struct key_word *));
                c->keyword = tmp;
            }
        }
        else if (*(args[0]) == '#')
        {
            /* do nothing for comment */
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
        {                       /* anything else is an error */
            break_a;
        }
        free_args (args);
        MC_PTR_FREE (l);
    }
    free_args (args);
    MC_PTR_FREE (l);

    /* Terminate context array.  */
    if (num_contexts > 0)
    {
        r[num_contexts - 1]->keyword[num_words] = NULL;
        r[num_contexts] = NULL;
    }

    if (edit->rules[0] == NULL)
        MC_PTR_FREE (edit->rules);

    if (result == 0)
    {
        char *first_chars, *p;

        if (num_contexts == -1)
            return line;

        first_chars = g_malloc0 (max_alloc_words_per_context + 2);

        for (i = 0; edit->rules[i] != NULL; i++)
        {
            c = edit->rules[i];
            p = first_chars;
            *p++ = (char) 1;
            for (j = 1; c->keyword[j] != NULL; j++)
                *p++ = c->keyword[j]->first;
            *p = '\0';
            c->keyword_first_chars = g_strdup (first_chars);
        }

        g_free (first_chars);
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */

/* returns -1 on file error, line number on error in file syntax */
static int
edit_read_syntax_file (WEdit * edit, char ***pnames, const char *syntax_file,
                       const char *editor_file, const char *first_line, const char *type)
{
#define NENTRIES 30
    FILE *f, *g = NULL;
    char *args[1024], *l = NULL;
    long line = 0;
    int result = 0;
    int count = 0;
    char *lib_file;
    gboolean found = FALSE;
    char **tmpnames = NULL;

    f = fopen (syntax_file, "r");
    if (f == NULL)
    {
        lib_file = g_build_filename (mc_global.share_data_dir, "syntax", "Syntax", (char *) NULL);
        f = fopen (lib_file, "r");
        g_free (lib_file);
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
        (void) get_args (l, args, 1023);        /* Final NULL */
        if (args[0] == NULL)
            continue;

        /* Looking for `include ...` lines before first `file ...` ones */
        if (!found && strcmp (args[0], "include") == 0)
        {
            if (g != NULL)
                continue;

            if (!args[1] || !(g = open_include_file (args[1])))
            {
                result = line;
                break;
            }
            goto found_type;
        }

        /* looking for `file ...' lines only */
        if (strcmp (args[0], "file") != 0)
            continue;

        found = TRUE;

        /* must have two args or report error */
        if (!args[1] || !args[2])
        {
            result = line;
            break;
        }
        if (pnames && *pnames)
        {
            /* 1: just collecting a list of names of rule sets */
            /* Reallocate the list if required */
            if (count % NENTRIES == 0)
            {
                tmpnames =
                    (char **) g_try_realloc (*pnames, (count + NENTRIES + 1) * sizeof (char *));
                if (tmpnames == NULL)
                    break;
                *pnames = tmpnames;
            }
            (*pnames)[count++] = g_strdup (args[2]);
            (*pnames)[count] = NULL;
        }
        else if (type)
        {
            /* 2: rule set was explicitly specified by the caller */
            if (strcmp (type, args[2]) == 0)
                goto found_type;
        }
        else if (editor_file && edit)
        {
            /* 3: auto-detect rule set from regular expressions */
            int q;

            q = mc_search (args[1], editor_file, MC_SEARCH_T_REGEX);
            /* does filename match arg 1 ? */
            if (!q && args[3])
            {
                /* does first line match arg 3 ? */
                q = mc_search (args[3], first_line, MC_SEARCH_T_REGEX);
            }
            if (q)
            {
                int line_error;
                char *syntax_type;
              found_type:
                syntax_type = args[2];
                line_error = edit_read_syntax_rules (edit, g ? g : f, args, 1023);
                if (line_error)
                {
                    if (!error_file_name)       /* an included file */
                        result = line + line_error;
                    else
                        result = line_error;
                }
                else
                {
                    g_free (edit->syntax_type);
                    edit->syntax_type = g_strdup (syntax_type);
                    /* if there are no rules then turn off syntax highlighting for speed */
                    if (!g && !edit->rules[1])
                        if (!edit->rules[0]->keyword[1] && !edit->rules[0]->spelling)
                        {
                            edit_free_syntax_rules (edit);
                            break;
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
get_first_editor_line (WEdit * edit)
{
    static char s[256];

    s[0] = '\0';

    if (edit != NULL)
    {
        size_t i;

        for (i = 0; i < sizeof (s) - 1; i++)
        {
            s[i] = edit_get_byte (edit, i);
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
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
edit_get_syntax_color (WEdit * edit, off_t byte_index)
{
    if (!tty_use_colors ())
        return 0;

    if (edit->rules != NULL && byte_index < edit->last_byte && option_syntax_highlighting)
        return translate_rule_to_color (edit, edit_get_rule (edit, byte_index));

    return EDITOR_NORMAL_COLOR;
}

/* --------------------------------------------------------------------------------------------- */

void
edit_free_syntax_rules (WEdit * edit)
{
    size_t i, j;

    if (!edit)
        return;
    if (edit->defines)
        destroy_defines (&edit->defines);
    if (!edit->rules)
        return;

    edit_get_rule (edit, -1);
    MC_PTR_FREE (edit->syntax_type);

    for (i = 0; edit->rules[i]; i++)
    {
        if (edit->rules[i]->keyword)
        {
            for (j = 0; edit->rules[i]->keyword[j]; j++)
            {
                MC_PTR_FREE (edit->rules[i]->keyword[j]->keyword);
                MC_PTR_FREE (edit->rules[i]->keyword[j]->whole_word_chars_left);
                MC_PTR_FREE (edit->rules[i]->keyword[j]->whole_word_chars_right);
                MC_PTR_FREE (edit->rules[i]->keyword[j]);
            }
        }
        MC_PTR_FREE (edit->rules[i]->left);
        MC_PTR_FREE (edit->rules[i]->right);
        MC_PTR_FREE (edit->rules[i]->whole_word_chars_left);
        MC_PTR_FREE (edit->rules[i]->whole_word_chars_right);
        MC_PTR_FREE (edit->rules[i]->keyword);
        MC_PTR_FREE (edit->rules[i]->keyword_first_chars);
        MC_PTR_FREE (edit->rules[i]);
    }

    g_slist_foreach (edit->syntax_marker, (GFunc) g_free, NULL);
    g_slist_free (edit->syntax_marker);
    edit->syntax_marker = NULL;
    MC_PTR_FREE (edit->rules);
    tty_color_free_all_tmp ();
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Load rules into edit struct.  Either edit or *pnames must be NULL.  If
 * edit is NULL, a list of types will be stored into names.  If type is
 * NULL, then the type will be selected according to the filename.
 * type must be edit->syntax_type or NULL
 */
void
edit_load_syntax (WEdit * edit, char ***pnames, const char *type)
{
    int r;
    char *f = NULL;

    if (option_auto_syntax)
        type = NULL;

    if (edit != NULL)
    {
        char *saved_type;

        saved_type = g_strdup (type);   /* save edit->syntax_type */
        edit_free_syntax_rules (edit);
        edit->syntax_type = saved_type; /* restore edit->syntax_type */
    }

    if (!tty_use_colors ())
        return;

    if (!option_syntax_highlighting && (!pnames || !*pnames))
        return;

    if (edit != NULL && edit->filename_vpath == NULL)
        return;

    f = mc_config_get_full_path (EDIT_SYNTAX_FILE);
    if (edit != NULL)
    {
        char *tmp_f;

        tmp_f = vfs_path_to_str (edit->filename_vpath);
        r = edit_read_syntax_file (edit, pnames, f, tmp_f,
                                   get_first_editor_line (edit),
                                   option_auto_syntax ? NULL : edit->syntax_type);
        g_free (tmp_f);
    }
    else
        r = edit_read_syntax_file (NULL, pnames, f, NULL, "", NULL);
    if (r == -1)
    {
        edit_free_syntax_rules (edit);
        message (D_ERROR, _("Load syntax file"),
                 _("Cannot open file %s\n%s"), f, unix_error_string (errno));
    }
    else if (r != 0)
    {
        edit_free_syntax_rules (edit);
        message (D_ERROR, _("Load syntax file"),
                 _("Error in file %s on line %d"), error_file_name ? error_file_name : f, r);
        MC_PTR_FREE (error_file_name);
    }

    g_free (f);
}

/* --------------------------------------------------------------------------------------------- */

const char *
edit_get_syntax_type (const WEdit * edit)
{
    return edit->syntax_type;
}

/* --------------------------------------------------------------------------------------------- */
