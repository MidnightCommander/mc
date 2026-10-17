/*
   Editor syntax highlighting: Sublime Text color schemes.

   Copyright (C) 2026
   Free Software Foundation, Inc.

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
 *  \brief Source: Sublime Text color schemes for editor syntax scopes
 *
 *  A .syntax file may use a scope token (e.g. KEYWORD_CONTROL) instead of literal colors.
 *  The token is translated to a Sublime scope (keyword.control) and its colors are taken
 *  from a .sublime-color-scheme file or a VS Code color theme (.json, tokenColors)
 *  using Sublime's selector rules:
 *  the most specific matching selector wins, separately for foreground, background
 *  and font style.
 */

#include <config.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/fileloc.h"  // EDIT_SYNTAX_DIR
#include "lib/mcconfig.h"
#include "lib/skin.h"
#include "lib/tty/color.h"
#include "lib/util.h"  // tilde_expand()

#include "edit-impl.h"  // edit_options

#include "syntax_scheme.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MAX_VAR_DEPTH 8

/*** file scope type declarations ****************************************************************/

typedef enum
{
    JV_NULL,
    JV_BOOL,
    JV_NUMBER,
    JV_STRING,
    JV_ARRAY,
    JV_OBJECT
} jv_type_t;

typedef struct jv_t
{
    jv_type_t type;
    char *str;         // JV_STRING
    GPtrArray *keys;   // JV_OBJECT: char *
    GPtrArray *items;  // JV_ARRAY, JV_OBJECT: jv_t *
} jv_t;

typedef struct
{
    const char *p;
    int line;
    GError **error;
} jv_parser_t;

typedef struct
{
    gboolean set;
    double r, g, b, a;
    int basic;  // index in basic_colors[] if given by MC name, -1 otherwise
} rgba_t;

typedef struct
{
    char *selector;
    rgba_t fg;
    rgba_t bg;
    char *font_style;  // NULL if not set
} scheme_rule_t;

struct syntax_scheme_t
{
    GHashTable *variables;  // name -> raw color string
    rgba_t fg;
    rgba_t bg;
    GPtrArray *rules;  // scheme_rule_t *
};

/*** forward declarations (file scope functions) *************************************************/

static jv_t *jv_parse_value (jv_parser_t *ps);

/*** file scope variables ************************************************************************/

/* Classic MC colors expressed as a scheme; used when no scheme is configured */
static const char builtin_scheme[] =
    "{\n"
    "  \"name\": \"MC classic\",\n"
    "  \"rules\": [\n"
    "    { \"scope\": \"comment\", \"foreground\": \"brown\" },\n"
    "    { \"scope\": \"keyword, storage, constant.language, variable.language\","
    " \"foreground\": \"yellow\" },\n"
    "    { \"scope\": \"keyword.other.todo\", \"foreground\": \"brightred\" },\n"
    "    { \"scope\": \"support\", \"foreground\": \"brightred\" },\n"
    "    { \"scope\": \"entity.name\", \"foreground\": \"brightcyan\" },\n"
    "    { \"scope\": \"string\", \"foreground\": \"green\" },\n"
    "    { \"scope\": \"constant\", \"foreground\": \"brightgreen\" },\n"
    "    { \"scope\": \"punctuation\", \"foreground\": \"brightcyan\" },\n"
    "    { \"scope\": \"punctuation.terminator\", \"foreground\": \"brightmagenta\" },\n"
    "    { \"scope\": \"meta.preprocessor\", \"foreground\": \"brightred\" },\n"
    "    { \"scope\": \"meta.preprocessor string\", \"foreground\": \"red\" },\n"
    "    { \"scope\": \"meta.preprocessor punctuation\", \"foreground\": \"yellow\" },\n"
    "    { \"scope\": \"meta.preprocessor comment\", \"foreground\": \"brown\" },\n"
    "    { \"scope\": \"markup.heading\", \"foreground\": \"brightcyan\" },\n"
    "    { \"scope\": \"markup.bold\", \"foreground\": \"white\" },\n"
    "    { \"scope\": \"markup.italic, markup.underline.link, markup.list\","
    " \"foreground\": \"yellow\" },\n"
    "    { \"scope\": \"markup.quote, markup.raw\", \"foreground\": \"green\" },\n"
    "    { \"scope\": \"markup.inserted\", \"foreground\": \"brightgreen\" },\n"
    "    { \"scope\": \"markup.deleted\", \"foreground\": \"brightred\" },\n"
    "    { \"scope\": \"markup.changed\", \"foreground\": \"yellow\" },\n"
    "    { \"scope\": \"meta.diff.range\", \"foreground\": \"brightcyan\" },\n"
    "    { \"scope\": \"meta.diff.header\", \"foreground\": \"brightmagenta\" },\n"
    "    { \"scope\": \"invalid\", \"foreground\": \"white\", \"background\": \"red\" },\n"
    "  ],\n"
    "}\n";

/* standard xterm values of the 16 basic colors, in MC names */
static const struct
{
    const char *name;
    int r, g, b;
} basic_colors[] = {
    { "black", 0, 0, 0 },          { "red", 205, 0, 0 },
    { "green", 0, 205, 0 },        { "brown", 205, 205, 0 },
    { "blue", 0, 0, 238 },         { "magenta", 205, 0, 205 },
    { "cyan", 0, 205, 205 },       { "lightgray", 229, 229, 229 },
    { "gray", 127, 127, 127 },     { "brightred", 255, 0, 0 },
    { "brightgreen", 0, 255, 0 },  { "yellow", 255, 255, 0 },
    { "brightblue", 92, 92, 255 }, { "brightmagenta", 255, 0, 255 },
    { "brightcyan", 0, 255, 255 }, { "white", 255, 255, 255 },
};

/* CSS names commonly found in schemes */
static const struct
{
    const char *name;
    int r, g, b;
} css_colors[] = {
    { "black", 0, 0, 0 },       { "white", 255, 255, 255 },  { "red", 255, 0, 0 },
    { "green", 0, 128, 0 },     { "lime", 0, 255, 0 },       { "blue", 0, 0, 255 },
    { "yellow", 255, 255, 0 },  { "cyan", 0, 255, 255 },     { "aqua", 0, 255, 255 },
    { "magenta", 255, 0, 255 }, { "fuchsia", 255, 0, 255 },  { "gray", 128, 128, 128 },
    { "grey", 128, 128, 128 },  { "silver", 192, 192, 192 }, { "maroon", 128, 0, 0 },
    { "olive", 128, 128, 0 },   { "navy", 0, 0, 128 },       { "purple", 128, 0, 128 },
    { "teal", 0, 128, 128 },    { "orange", 255, 165, 0 },   { "pink", 255, 192, 203 },
    { "brown", 165, 42, 42 },
};

// Sublime color schemes first, then VS Code color themes
static const char *const scheme_exts[] = { SYNTAX_SCHEME_EXT, ".json" };

static syntax_scheme_t *current_scheme = NULL;
static char *current_name = NULL;

/* the skin's [editor] syntax_scheme, cached per skin: reading it from the skin config on
   every token lookup made loading a large syntax file noticeably slower */
static const mc_config_t *skin_config_seen = NULL;
static char *skin_name_seen = NULL;
static char *skin_scheme_name = NULL;

/* resolved styles of the current scheme, keyed by "depth \x1f context token \x1f token";
   a syntax file has thousands of keywords but only a few dozen distinct token pairs */
static GHashTable *lookup_cache = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/* Lenient JSON as accepted by Sublime: comments and trailing commas are allowed */

static void
jv_free (gpointer data)
{
    jv_t *v = (jv_t *) data;

    if (v == NULL)
        return;

    g_free (v->str);
    if (v->keys != NULL)
        g_ptr_array_free (v->keys, TRUE);
    if (v->items != NULL)
        g_ptr_array_free (v->items, TRUE);
    g_free (v);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
jv_error (jv_parser_t *ps, const char *msg)
{
    if (ps->error != NULL && *ps->error == NULL)
        g_set_error (ps->error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "line %d: %s", ps->line, msg);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
jv_skip_ws (jv_parser_t *ps)
{
    while (*ps->p != '\0')
    {
        if (*ps->p == '\n')
        {
            ps->line++;
            ps->p++;
        }
        else if (isspace ((unsigned char) *ps->p))
            ps->p++;
        else if (ps->p[0] == '/' && ps->p[1] == '/')
        {
            while (*ps->p != '\0' && *ps->p != '\n')
                ps->p++;
        }
        else if (ps->p[0] == '/' && ps->p[1] == '*')
        {
            ps->p += 2;
            while (*ps->p != '\0' && !(ps->p[0] == '*' && ps->p[1] == '/'))
            {
                if (*ps->p == '\n')
                    ps->line++;
                ps->p++;
            }
            if (*ps->p != '\0')
                ps->p += 2;
        }
        else
            break;
    }
}

/* --------------------------------------------------------------------------------------------- */

static char *
jv_parse_string (jv_parser_t *ps)
{
    GString *s;

    ps->p++;  // opening quote
    s = g_string_new (NULL);

    while (*ps->p != '"')
    {
        if (*ps->p == '\0' || *ps->p == '\n')
        {
            jv_error (ps, "unterminated string");
            g_string_free (s, TRUE);
            return NULL;
        }

        if (*ps->p != '\\')
        {
            g_string_append_c (s, *ps->p++);
            continue;
        }

        ps->p++;
        switch (*ps->p)
        {
        case 'b':
            g_string_append_c (s, '\b');
            break;
        case 'f':
            g_string_append_c (s, '\f');
            break;
        case 'n':
            g_string_append_c (s, '\n');
            break;
        case 'r':
            g_string_append_c (s, '\r');
            break;
        case 't':
            g_string_append_c (s, '\t');
            break;
        case 'u':
        {
            unsigned int u;

            if (sscanf (ps->p + 1, "%4x", &u) != 1)
            {
                jv_error (ps, "bad \\u escape");
                g_string_free (s, TRUE);
                return NULL;
            }
            g_string_append_unichar (s, (gunichar) u);
            ps->p += 4;
            break;
        }
        case '\0':
            continue;  // reported as unterminated string
        default:
            g_string_append_c (s, *ps->p);  // \" \\ \/
            break;
        }
        ps->p++;
    }

    ps->p++;  // closing quote
    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static jv_t *
jv_parse_container (jv_parser_t *ps, gboolean is_object)
{
    const char close = is_object ? '}' : ']';
    jv_t *v;

    v = g_new0 (jv_t, 1);
    v->type = is_object ? JV_OBJECT : JV_ARRAY;
    v->items = g_ptr_array_new_with_free_func (jv_free);
    if (is_object)
        v->keys = g_ptr_array_new_with_free_func (g_free);

    ps->p++;  // opening bracket

    while (TRUE)
    {
        jv_t *item;

        jv_skip_ws (ps);
        if (*ps->p == close)  // empty container or trailing comma
            break;

        if (is_object)
        {
            char *key;

            if (*ps->p != '"')
            {
                jv_error (ps, "object key expected");
                goto fail;
            }
            key = jv_parse_string (ps);
            if (key == NULL)
                goto fail;
            g_ptr_array_add (v->keys, key);

            jv_skip_ws (ps);
            if (*ps->p != ':')
            {
                jv_error (ps, "':' expected");
                goto fail;
            }
            ps->p++;
        }

        item = jv_parse_value (ps);
        if (item == NULL)
            goto fail;
        g_ptr_array_add (v->items, item);

        jv_skip_ws (ps);
        if (*ps->p == ',')
            ps->p++;
        else if (*ps->p != close)
        {
            jv_error (ps, is_object ? "',' or '}' expected" : "',' or ']' expected");
            goto fail;
        }
    }

    ps->p++;  // closing bracket
    return v;

fail:
    jv_free (v);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static jv_t *
jv_parse_value (jv_parser_t *ps)
{
    jv_t *v;

    jv_skip_ws (ps);

    switch (*ps->p)
    {
    case '{':
        return jv_parse_container (ps, TRUE);
    case '[':
        return jv_parse_container (ps, FALSE);
    case '"':
    {
        char *s;

        s = jv_parse_string (ps);
        if (s == NULL)
            return NULL;
        v = g_new0 (jv_t, 1);
        v->type = JV_STRING;
        v->str = s;
        return v;
    }
    default:
        break;
    }

    // literals and numbers: their values are of no interest
    v = g_new0 (jv_t, 1);
    if (strncmp (ps->p, "true", 4) == 0 || strncmp (ps->p, "false", 5) == 0)
    {
        v->type = JV_BOOL;
        ps->p += (*ps->p == 't') ? 4 : 5;
    }
    else if (strncmp (ps->p, "null", 4) == 0)
    {
        v->type = JV_NULL;
        ps->p += 4;
    }
    else if (*ps->p == '-' || isdigit ((unsigned char) *ps->p))
    {
        v->type = JV_NUMBER;
        ps->p++;
        while (*ps->p != '\0' && strchr ("0123456789.eE+-", *ps->p) != NULL)
            ps->p++;
    }
    else
    {
        g_free (v);
        jv_error (ps, "value expected");
        return NULL;
    }

    return v;
}

/* --------------------------------------------------------------------------------------------- */

static const jv_t *
jv_get (const jv_t *obj, const char *key, jv_type_t type)
{
    guint i;

    if (obj == NULL || obj->type != JV_OBJECT)
        return NULL;

    // last duplicate wins, as in Sublime
    for (i = obj->keys->len; i > 0; i--)
        if (strcmp (g_ptr_array_index (obj->keys, i - 1), key) == 0)
        {
            const jv_t *v = g_ptr_array_index (obj->items, i - 1);

            return v->type == type ? v : NULL;
        }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
/* Colors */

/* copy the argument list of "name(...)" with balanced parentheses; returns pointer after ')' */
static const char *
color_func_args (const char *s, char *buf, size_t size)
{
    int level = 1;
    size_t n = 0;

    s = strchr (s, '(');
    if (s == NULL)
        return NULL;

    for (s++; *s != '\0'; s++)
    {
        if (*s == '(')
            level++;
        else if (*s == ')' && --level == 0)
            break;
        if (n + 1 < size)
            buf[n++] = *s;
    }
    buf[n] = '\0';

    return *s == ')' ? s + 1 : NULL;
}

/* --------------------------------------------------------------------------------------------- */

static double
hue_to_rgb (double p, double q, double t)
{
    if (t < 0)
        t += 1;
    if (t > 1)
        t -= 1;
    if (t < 1.0 / 6)
        return p + (q - p) * 6 * t;
    if (t < 1.0 / 2)
        return q;
    if (t < 2.0 / 3)
        return p + (q - p) * (2.0 / 3 - t) * 6;
    return p;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
parse_color (const syntax_scheme_t *scheme, const char *text, int depth, rgba_t *out)
{
    char buf[BUF_SMALL];
    char *s;
    gboolean ok = FALSE;

    if (text == NULL || depth > MAX_VAR_DEPTH)
        return FALSE;

    s = g_strstrip (g_strdup (text));
    out->a = 1.0;
    out->basic = -1;

    if (strncmp (s, "var(", 4) == 0)
    {
        const char *raw;

        if (color_func_args (s, buf, sizeof (buf)) != NULL)
        {
            raw = g_hash_table_lookup (scheme->variables, g_strstrip (buf));
            ok = parse_color (scheme, raw, depth + 1, out);
        }
    }
    else if (strncmp (s, "color(", 6) == 0)
    {
        // color(<base> <adjuster>...): take the base, honor alpha(), ignore the rest
        if (color_func_args (s, buf, sizeof (buf)) != NULL)
        {
            char *p, *adj;
            int level = 0;

            for (p = buf; *p != '\0' && !(level == 0 && isspace ((unsigned char) *p)); p++)
                if (*p == '(')
                    level++;
                else if (*p == ')')
                    level--;
            adj = p;
            if (*p != '\0')
                *adj++ = '\0';

            ok = parse_color (scheme, buf, depth + 1, out);

            for (p = adj; ok && *p != '\0'; p++)
            {
                double alpha;

                if ((strncmp (p, "alpha(", 6) == 0 && sscanf (p + 6, "%lf", &alpha) == 1)
                    || (strncmp (p, "a(", 2) == 0 && sscanf (p + 2, "%lf", &alpha) == 1))
                {
                    if (strchr (p, '%') != NULL && strchr (p, '%') < strchr (p, ')'))
                        alpha /= 100.0;
                    out->a = CLAMP (alpha, 0.0, 1.0);
                    break;
                }
            }
        }
    }
    else if (s[0] == '#')
    {
        const size_t len = strlen (s + 1);
        unsigned int h[8];
        size_t i;

        ok = (len == 3 || len == 4 || len == 6 || len == 8);
        for (i = 0; ok && i < len; i++)
        {
            ok = g_ascii_isxdigit (s[i + 1]);
            if (ok)
                h[i] = (unsigned int) g_ascii_xdigit_value (s[i + 1]);
        }

        if (ok && len <= 4)
        {
            out->r = h[0] * 17;
            out->g = h[1] * 17;
            out->b = h[2] * 17;
            if (len == 4)
                out->a = h[3] * 17 / 255.0;
        }
        else if (ok)
        {
            out->r = h[0] * 16 + h[1];
            out->g = h[2] * 16 + h[3];
            out->b = h[4] * 16 + h[5];
            if (len == 8)
                out->a = (h[6] * 16 + h[7]) / 255.0;
        }
    }
    else if (strncmp (s, "rgb", 3) == 0 || strncmp (s, "hsl", 3) == 0)
    {
        if (color_func_args (s, buf, sizeof (buf)) != NULL)
        {
            double v[4] = { 0, 0, 0, 1 };
            char *p, *end;
            int n;

            for (n = 0, p = buf; n < 4 && *p != '\0'; n++)
            {
                while (*p != '\0' && (isspace ((unsigned char) *p) || *p == ',' || *p == '/'))
                    p++;
                v[n] = g_ascii_strtod (p, &end);
                if (end == p)
                    break;
                p = end;
                if (*p == '%')
                {
                    if (n == 3)
                        v[n] /= 100.0;
                    p++;
                }
            }

            ok = n >= 3;
            if (ok && s[0] == 'r')
            {
                out->r = CLAMP (v[0], 0, 255);
                out->g = CLAMP (v[1], 0, 255);
                out->b = CLAMP (v[2], 0, 255);
            }
            else if (ok)
            {
                const double hh = fmod (v[0], 360.0) / 360.0;
                const double ss = CLAMP (v[1], 0, 100) / 100.0;
                const double ll = CLAMP (v[2], 0, 100) / 100.0;
                const double q = ll < 0.5 ? ll * (1 + ss) : ll + ss - ll * ss;
                const double pp = 2 * ll - q;

                out->r = 255 * hue_to_rgb (pp, q, hh + 1.0 / 3);
                out->g = 255 * hue_to_rgb (pp, q, hh);
                out->b = 255 * hue_to_rgb (pp, q, hh - 1.0 / 3);
            }
            out->a = CLAMP (v[3], 0.0, 1.0);
        }
    }
    else
    {
        size_t i;

        // MC names first, so that the built-in scheme gets exact basic colors
        for (i = 0; !ok && i < G_N_ELEMENTS (basic_colors); i++)
            if (strcmp (s, basic_colors[i].name) == 0)
            {
                out->r = basic_colors[i].r;
                out->g = basic_colors[i].g;
                out->b = basic_colors[i].b;
                out->basic = (int) i;
                ok = TRUE;
            }
        for (i = 0; !ok && i < G_N_ELEMENTS (css_colors); i++)
            if (g_ascii_strcasecmp (s, css_colors[i].name) == 0)
            {
                out->r = css_colors[i].r;
                out->g = css_colors[i].g;
                out->b = css_colors[i].b;
                ok = TRUE;
            }
    }

    g_free (s);
    out->set = ok;
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static double
color_distance (double r1, double g1, double b1, double r2, double g2, double b2)
{
    // weighted euclidean distance, cheap approximation of perceived difference
    return 2 * (r1 - r2) * (r1 - r2) + 4 * (g1 - g2) * (g1 - g2) + 3 * (b1 - b2) * (b1 - b2);
}

/* --------------------------------------------------------------------------------------------- */

static int
cube_index (double v)
{
    // xterm cube levels: 0, 95, 135, 175, 215, 255
    return v < 48 ? 0 : v < 115 ? 1 : (int) ((v - 35) / 40);
}

/* --------------------------------------------------------------------------------------------- */

static void
format_color (const rgba_t *c, const rgba_t *base, syntax_scheme_depth_t depth, char *buf,
              size_t size)
{
    double r = c->r, g = c->g, b = c->b;

    // keep MC color names as is: they follow the terminal palette
    if (c->basic >= 0 && c->a >= 1.0)
    {
        g_strlcpy (buf, basic_colors[c->basic].name, size);
        return;
    }

    if (c->a < 1.0 && base != NULL && base->set)
    {
        r = r * c->a + base->r * (1 - c->a);
        g = g * c->a + base->g * (1 - c->a);
        b = b * c->a + base->b * (1 - c->a);
    }

    if (depth == SYNTAX_SCHEME_DEPTH_TRUECOLOR)
        g_snprintf (buf, size, "#%02x%02x%02x", (unsigned int) CLAMP (lround (r), 0, 255),
                    (unsigned int) CLAMP (lround (g), 0, 255),
                    (unsigned int) CLAMP (lround (b), 0, 255));
    else if (depth == SYNTAX_SCHEME_DEPTH_256)
    {
        static const int level[] = { 0, 95, 135, 175, 215, 255 };
        const int ri = cube_index (r), gi = cube_index (g), bi = cube_index (b);
        const int gray = CLAMP ((int) lround (((r + g + b) / 3 - 8) / 10), 0, 23);
        const int gv = 8 + gray * 10;

        if (color_distance (r, g, b, gv, gv, gv)
            < color_distance (r, g, b, level[ri], level[gi], level[bi]))
            g_snprintf (buf, size, "color%d", 232 + gray);
        else
            g_snprintf (buf, size, "color%d", 16 + 36 * ri + 6 * gi + bi);
    }
    else
    {
        size_t i, best = 0;
        double best_d = -1;

        for (i = 0; i < G_N_ELEMENTS (basic_colors); i++)
        {
            const double d =
                color_distance (r, g, b, basic_colors[i].r, basic_colors[i].g, basic_colors[i].b);

            if (best_d < 0 || d < best_d)
            {
                best_d = d;
                best = i;
            }
        }
        g_strlcpy (buf, basic_colors[best].name, size);
    }
}

/* --------------------------------------------------------------------------------------------- */
/* Selectors */

/* does atom "keyword.control" match scope "keyword.control.flow"? */
static gboolean
atom_matches (const char *atom, const char *scope)
{
    const size_t len = strlen (atom);

    return strncmp (atom, scope, len) == 0 && (scope[len] == '\0' || scope[len] == '.');
}

/* --------------------------------------------------------------------------------------------- */

static int
atom_components (const char *atom)
{
    int n = 1;

    for (; *atom != '\0'; atom++)
        if (*atom == '.')
            n++;

    return n;
}

/* --------------------------------------------------------------------------------------------- */

/* score of one alternative ("string constant - comment") against the stack; -1 if no match */
static int
score_path (char *path, const char *const *stack, int depth)
{
    gchar **words;
    int nwords, last, i, j, score = -1;
    int last_depth = -1, last_comps = 0, natoms = 0;

    words = g_strsplit_set (g_strstrip (path), " \t()&", -1);
    nwords = (int) g_strv_length (words);

    // exclusions: everything after a "-" word
    for (last = 0; last < nwords && strcmp (words[last], "-") != 0; last++)
        ;
    for (i = last + 1; i < nwords; i++)
        for (j = 0; j < depth; j++)
            if (*words[i] != '\0' && atom_matches (words[i], stack[j]))
                goto done;

    // atoms must match the stack as a subsequence; the rightmost atom as deep as possible
    j = depth - 1;
    for (i = last - 1; i >= 0; i--)
    {
        if (*words[i] == '\0')
            continue;

        while (j >= 0 && !atom_matches (words[i], stack[j]))
            j--;
        if (j < 0)
            goto done;

        if (last_depth < 0)
        {
            last_depth = j;
            last_comps = atom_components (words[i]);
        }
        natoms++;
        j--;
    }

    if (natoms > 0)
        score = (last_depth + 1) * 10000 + last_comps * 100 + natoms;

done:
    g_strfreev (words);
    return score;
}

/* --------------------------------------------------------------------------------------------- */

static int
score_selector (const char *selector, const char *const *stack, int depth)
{
    gchar **alts;
    int i, best = -1;

    alts = g_strsplit_set (selector, ",|", -1);
    for (i = 0; alts[i] != NULL; i++)
        best = MAX (best, score_path (alts[i], stack, depth));
    g_strfreev (alts);

    return best;
}

/* --------------------------------------------------------------------------------------------- */

static void
font_style_to_attrs (const char *font_style, char *buf, size_t size)
{
    static const char *const known[] = { "bold", "italic", "underline" };
    gchar **words;
    int i;
    size_t k;

    buf[0] = '\0';
    words = g_strsplit_set (font_style, " \t", -1);
    for (i = 0; words[i] != NULL; i++)
        for (k = 0; k < G_N_ELEMENTS (known); k++)
            if (strcmp (words[i], known[k]) == 0)
            {
                if (buf[0] != '\0')
                    g_strlcat (buf, "+", size);
                g_strlcat (buf, known[k], size);
            }
    g_strfreev (words);
}

/* --------------------------------------------------------------------------------------------- */

static void
scheme_rule_free (gpointer data)
{
    scheme_rule_t *rule = (scheme_rule_t *) data;

    g_free (rule->selector);
    g_free (rule->font_style);
    g_free (rule);
}

/* --------------------------------------------------------------------------------------------- */

static char *
find_scheme_file (const char *name)
{
    const char *const roots[] = { mc_config_get_data_path (), mc_global.share_data_dir };
    gboolean has_ext = FALSE;
    size_t r, e;

    if (strchr (name, PATH_SEP) != NULL)
        return tilde_expand (name);

    for (e = 0; e < G_N_ELEMENTS (scheme_exts); e++)
        if (g_str_has_suffix (name, scheme_exts[e]))
            has_ext = TRUE;

    for (r = 0; r < G_N_ELEMENTS (roots); r++)
        for (e = 0; e < G_N_ELEMENTS (scheme_exts); e++)
        {
            char *base, *path;

            base = has_ext ? g_strdup (name) : g_strconcat (name, scheme_exts[e], (char *) NULL);
            path = g_build_filename (roots[r], EDIT_SYNTAX_DIR, SYNTAX_SCHEME_DIR, base,
                                     (char *) NULL);
            g_free (base);
            if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
                return path;
            g_free (path);
            if (has_ext)
                break;
        }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* scheme name suggested by the current skin; re-read only when the skin changes */
static const char *
skin_scheme (void)
{
    const char *skin_name = mc_skin__default.name != NULL ? mc_skin__default.name : "";

    if (skin_scheme_name == NULL || skin_config_seen != mc_skin__default.config
        || strcmp (skin_name_seen, skin_name) != 0)
    {
        g_free (skin_scheme_name);
        g_free (skin_name_seen);
        skin_scheme_name = mc_skin_get ("editor", "syntax_scheme", "");
        skin_name_seen = g_strdup (skin_name);
        skin_config_seen = mc_skin__default.config;
    }

    return skin_scheme_name;
}

/* --------------------------------------------------------------------------------------------- */

static void
ensure_scheme (void)
{
    char *name, *path;

    // user's choice first, then the skin's suggestion
    if (edit_options.syntax_color_scheme != NULL && *edit_options.syntax_color_scheme != '\0')
        name = g_strdup (edit_options.syntax_color_scheme);
    else
        name = g_strdup (skin_scheme ());

    if (current_scheme != NULL && strcmp (name, current_name) == 0)
    {
        g_free (name);
        return;
    }

    syntax_scheme_done ();
    current_name = name;

    if (*name != '\0')
    {
        path = find_scheme_file (name);
        if (path != NULL)
            current_scheme = syntax_scheme_load (path, NULL);
        g_free (path);
    }

    // missing or broken scheme: keep classic colors
    if (current_scheme == NULL)
        current_scheme = syntax_scheme_builtin ();
}

/* --------------------------------------------------------------------------------------------- */

static scheme_rule_t *
new_rule (const syntax_scheme_t *scheme, const char *selector, const jv_t *settings,
          const char *font_style_key)
{
    scheme_rule_t *rule;
    const jv_t *v;

    rule = g_new0 (scheme_rule_t, 1);
    rule->selector = g_strdup (selector);
    v = jv_get (settings, "foreground", JV_STRING);
    if (v != NULL)
        parse_color (scheme, v->str, 0, &rule->fg);
    v = jv_get (settings, "background", JV_STRING);
    if (v != NULL)
        parse_color (scheme, v->str, 0, &rule->bg);
    v = jv_get (settings, font_style_key, JV_STRING);
    if (v != NULL)
        rule->font_style = g_strdup (v->str);

    return rule;
}

/* --------------------------------------------------------------------------------------------- */

/* .sublime-color-scheme: variables, globals, rules */
static void
parse_sublime (const jv_t *root, syntax_scheme_t *scheme)
{
    const jv_t *section, *v;
    guint i;

    section = jv_get (root, "variables", JV_OBJECT);
    for (i = 0; section != NULL && i < section->keys->len; i++)
    {
        const jv_t *val = g_ptr_array_index (section->items, i);

        if (val->type == JV_STRING)
            g_hash_table_insert (scheme->variables, g_strdup (g_ptr_array_index (section->keys, i)),
                                 g_strdup (val->str));
    }

    section = jv_get (root, "globals", JV_OBJECT);
    v = jv_get (section, "foreground", JV_STRING);
    if (v != NULL)
        parse_color (scheme, v->str, 0, &scheme->fg);
    v = jv_get (section, "background", JV_STRING);
    if (v != NULL)
        parse_color (scheme, v->str, 0, &scheme->bg);

    section = jv_get (root, "rules", JV_ARRAY);
    for (i = 0; section != NULL && i < section->items->len; i++)
    {
        const jv_t *item = g_ptr_array_index (section->items, i);

        v = jv_get (item, "scope", JV_STRING);
        if (v != NULL)
            g_ptr_array_add (scheme->rules, new_rule (scheme, v->str, item, "font_style"));
    }
}

/* --------------------------------------------------------------------------------------------- */

/* VS Code color theme: colors["editor.foreground"], tokenColors[{scope, settings}] */
static jv_t *
load_json (const char *dir, const char *name, char **dir_out)
{
    char *path, *text;
    jv_t *root = NULL;

    path =
        g_path_is_absolute (name) ? g_strdup (name) : g_build_filename (dir, name, (char *) NULL);
    if (g_file_get_contents (path, &text, NULL, NULL))
    {
        jv_parser_t ps = { text, 1, NULL };

        root = jv_parse_value (&ps);
        g_free (text);
    }

    if (root != NULL && dir_out != NULL)
        *dir_out = g_path_get_dirname (path);
    g_free (path);
    return root;
}

/* --------------------------------------------------------------------------------------------- */

static void
add_token_colors (const jv_t *section, syntax_scheme_t *scheme)
{
    const jv_t *v;
    guint i;

    for (i = 0; section != NULL && i < section->items->len; i++)
    {
        const jv_t *item = g_ptr_array_index (section->items, i);
        const jv_t *settings = jv_get (item, "settings", JV_OBJECT);
        const jv_t *scope;
        GString *selector;
        guint k;

        if (settings == NULL)
            continue;

        scope = jv_get (item, "scope", JV_STRING);
        if (scope == NULL)
            scope = jv_get (item, "scope", JV_ARRAY);
        if (scope == NULL)
        {
            // TextMate-style defaults entry
            v = jv_get (settings, "foreground", JV_STRING);
            if (v != NULL && !scheme->fg.set)
                parse_color (scheme, v->str, 0, &scheme->fg);
            v = jv_get (settings, "background", JV_STRING);
            if (v != NULL && !scheme->bg.set)
                parse_color (scheme, v->str, 0, &scheme->bg);
            continue;
        }

        if (scope->type == JV_STRING)
            selector = g_string_new (scope->str);
        else
        {
            selector = g_string_new (NULL);
            for (k = 0; k < scope->items->len; k++)
            {
                const jv_t *s = g_ptr_array_index (scope->items, k);

                if (s->type != JV_STRING)
                    continue;
                if (selector->len != 0)
                    g_string_append (selector, ", ");
                g_string_append (selector, s->str);
            }
        }

        if (selector->len != 0)
            g_ptr_array_add (scheme->rules,
                             new_rule (scheme, selector->str, settings, "fontStyle"));
        g_string_free (selector, TRUE);
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * VS Code color theme: colors["editor.foreground"], tokenColors[{scope, settings}].
 * "include" and a tokenColors file name are resolved relative to @dir (NULL: ignored);
 * included rules come first, so the theme's own rules win ties.
 */
static void
parse_vscode (const jv_t *root, syntax_scheme_t *scheme, const char *dir, int depth)
{
    const jv_t *section, *v;

    v = jv_get (root, "include", JV_STRING);
    if (v != NULL && dir != NULL && depth < MAX_VAR_DEPTH)
    {
        char *inc_dir = NULL;
        jv_t *inc;

        inc = load_json (dir, v->str, &inc_dir);
        if (inc != NULL && inc->type == JV_OBJECT)
            parse_vscode (inc, scheme, inc_dir, depth + 1);
        jv_free (inc);
        g_free (inc_dir);
    }

    section = jv_get (root, "colors", JV_OBJECT);
    v = jv_get (section, "editor.foreground", JV_STRING);
    if (v != NULL)
        parse_color (scheme, v->str, 0, &scheme->fg);
    v = jv_get (section, "editor.background", JV_STRING);
    if (v != NULL)
        parse_color (scheme, v->str, 0, &scheme->bg);

    v = jv_get (root, "tokenColors", JV_STRING);
    if (v != NULL && dir != NULL)
    {
        jv_t *list;

        list = load_json (dir, v->str, NULL);
        if (list != NULL && list->type == JV_ARRAY)
            add_token_colors (list, scheme);
        jv_free (list);
    }
    else
        add_token_colors (jv_get (root, "tokenColors", JV_ARRAY), scheme);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
is_vscode (const jv_t *root)
{
    return jv_get (root, "tokenColors", JV_ARRAY) != NULL
        || jv_get (root, "tokenColors", JV_STRING) != NULL
        || jv_get (root, "include", JV_STRING) != NULL;
}

/* --------------------------------------------------------------------------------------------- */

static syntax_scheme_t *
parse_text (const char *text, const char *dir, GError **error)
{
    jv_parser_t ps = { text, 1, error };
    syntax_scheme_t *scheme;
    jv_t *root;

    root = jv_parse_value (&ps);
    if (root == NULL)
        return NULL;
    if (root->type != JV_OBJECT)
    {
        jv_error (&ps, "top-level object expected");
        jv_free (root);
        return NULL;
    }

    scheme = g_new0 (syntax_scheme_t, 1);
    scheme->variables = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    scheme->rules = g_ptr_array_new_with_free_func (scheme_rule_free);

    if (is_vscode (root))
        parse_vscode (root, scheme, dir, 0);
    else
        parse_sublime (root, scheme);

    jv_free (root);
    return scheme;
}

/* --------------------------------------------------------------------------------------------- */

static int
compare_names (gconstpointer a, gconstpointer b)
{
    return g_ascii_strcasecmp (*(char *const *) a, *(char *const *) b);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* scope tokens are upper case: literal color names are always lower case */
gboolean
syntax_scheme_is_scope_token (const char *token)
{
    gboolean has_alpha = FALSE;

    if (token == NULL || *token == '\0')
        return FALSE;

    for (; *token != '\0'; token++)
    {
        if (g_ascii_isupper (*token))
            has_alpha = TRUE;
        else if (!g_ascii_isdigit (*token) && *token != '_')
            return FALSE;
    }

    return has_alpha;
}

/* --------------------------------------------------------------------------------------------- */

/* KEYWORD_CONTROL_FLOW -> keyword.control.flow */
char *
syntax_scheme_token_to_scope (const char *token)
{
    char *scope, *p;

    scope = g_ascii_strdown (token, -1);
    for (p = scope; *p != '\0'; p++)
        if (*p == '_')
            *p = '.';

    return scope;
}

/* --------------------------------------------------------------------------------------------- */

/* parse scheme text; files a VS Code theme includes are not followed */
syntax_scheme_t *
syntax_scheme_parse (const char *text, GError **error)
{
    return parse_text (text, NULL, error);
}

/* --------------------------------------------------------------------------------------------- */

/* load scheme file @path, following "include" of VS Code themes */
syntax_scheme_t *
syntax_scheme_load (const char *path, GError **error)
{
    syntax_scheme_t *scheme;
    char *text, *dir;

    if (!g_file_get_contents (path, &text, NULL, error))
        return NULL;

    dir = g_path_get_dirname (path);
    scheme = parse_text (text, dir, error);
    g_free (dir);
    g_free (text);
    return scheme;
}

/* --------------------------------------------------------------------------------------------- */

syntax_scheme_t *
syntax_scheme_builtin (void)
{
    return syntax_scheme_parse (builtin_scheme, NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_scheme_free (syntax_scheme_t *scheme)
{
    if (scheme == NULL)
        return;

    g_hash_table_destroy (scheme->variables);
    g_ptr_array_free (scheme->rules, TRUE);
    g_free (scheme);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Resolve style of @scope nested in @ctx_scope (may be NULL).
 * Foreground, background and font style are chosen independently: each comes from
 * the most specific rule that sets it; later rules win ties.
 * Background of the scheme globals is not used: the editor background stays the skin's.
 */

void
syntax_scheme_resolve (const syntax_scheme_t *scheme, const char *ctx_scope, const char *scope,
                       syntax_scheme_depth_t depth, syntax_scheme_style_t *style)
{
    const char *stack[2];
    int n = 0;
    const scheme_rule_t *fg = NULL, *bg = NULL, *fs = NULL;
    int fg_score = -1, bg_score = -1, fs_score = -1;
    guint i;

    memset (style, 0, sizeof (*style));

    if (scheme == NULL || scope == NULL)
        return;

    if (ctx_scope != NULL && *ctx_scope != '\0')
        stack[n++] = ctx_scope;
    stack[n++] = scope;

    for (i = 0; i < scheme->rules->len; i++)
    {
        const scheme_rule_t *rule = g_ptr_array_index (scheme->rules, i);
        const int score = score_selector (rule->selector, stack, n);

        if (score < 0)
            continue;
        if (rule->fg.set && score >= fg_score)
        {
            fg = rule;
            fg_score = score;
        }
        if (rule->bg.set && score >= bg_score)
        {
            bg = rule;
            bg_score = score;
        }
        if (rule->font_style != NULL && score >= fs_score)
        {
            fs = rule;
            fs_score = score;
        }
    }

    if (fg != NULL)
        format_color (&fg->fg, &scheme->bg, depth, style->fg, sizeof (style->fg));
    else if (scheme->fg.set)
        format_color (&scheme->fg, &scheme->bg, depth, style->fg, sizeof (style->fg));

    if (bg != NULL)
        format_color (&bg->bg, &scheme->bg, depth, style->bg, sizeof (style->bg));

    if (fs != NULL)
        font_style_to_attrs (fs->font_style, style->attrs, sizeof (style->attrs));
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Resolve scope token (e.g. KEYWORD_CONTROL) inside a context with scope token
 * @ctx_token (NULL if the context uses literal colors) using the configured scheme.
 */

void
syntax_scheme_lookup (const char *ctx_token, const char *token, syntax_scheme_style_t *style)
{
    syntax_scheme_depth_t depth;
    char *ctx_scope = NULL, *scope, *key;
    const syntax_scheme_style_t *cached;
    syntax_scheme_style_t *copy;

    ensure_scheme ();

    if (tty_use_truecolors (NULL))
        depth = SYNTAX_SCHEME_DEPTH_TRUECOLOR;
    else if (tty_use_256colors (NULL))
        depth = SYNTAX_SCHEME_DEPTH_256;
    else
        depth = SYNTAX_SCHEME_DEPTH_16;

    if (!syntax_scheme_is_scope_token (ctx_token))
        ctx_token = NULL;

    key =
        g_strdup_printf ("%d\037%s\037%s", (int) depth, ctx_token != NULL ? ctx_token : "", token);
    if (lookup_cache == NULL)
        lookup_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    cached = g_hash_table_lookup (lookup_cache, key);
    if (cached != NULL)
    {
        *style = *cached;
        g_free (key);
        return;
    }

    if (ctx_token != NULL)
        ctx_scope = syntax_scheme_token_to_scope (ctx_token);
    scope = syntax_scheme_token_to_scope (token);

    syntax_scheme_resolve (current_scheme, ctx_scope, scope, depth, style);
    copy = g_new (syntax_scheme_style_t, 1);
    *copy = *style;
    g_hash_table_insert (lookup_cache, key, copy);

    g_free (scope);
    g_free (ctx_scope);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Names (without extension) of schemes found in the user and system scheme directories,
 * sorted, without duplicates.
 */

GPtrArray *
syntax_scheme_list (void)
{
    const char *const roots[] = { mc_config_get_data_path (), mc_global.share_data_dir };
    GPtrArray *names;
    size_t i;

    names = g_ptr_array_new_with_free_func (g_free);

    for (i = 0; i < G_N_ELEMENTS (roots); i++)
    {
        char *dir_path;
        GDir *dir;
        const char *file;

        dir_path = g_build_filename (roots[i], EDIT_SYNTAX_DIR, SYNTAX_SCHEME_DIR, (char *) NULL);
        dir = g_dir_open (dir_path, 0, NULL);
        g_free (dir_path);
        if (dir == NULL)
            continue;

        while ((file = g_dir_read_name (dir)) != NULL)
        {
            size_t e;

            for (e = 0; e < G_N_ELEMENTS (scheme_exts); e++)
                if (g_str_has_suffix (file, scheme_exts[e]))
                    break;
            if (e < G_N_ELEMENTS (scheme_exts))
            {
                char *name;
                guint k;

                name = g_strndup (file, strlen (file) - strlen (scheme_exts[e]));
                for (k = 0; k < names->len; k++)
                    if (strcmp (g_ptr_array_index (names, k), name) == 0)
                        break;
                if (k < names->len)
                    g_free (name);
                else
                    g_ptr_array_add (names, name);
            }
        }

        g_dir_close (dir);
    }

    g_ptr_array_sort (names, (GCompareFunc) compare_names);
    return names;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Format of the file scheme @name resolves to: "Sublime" or "VS Code"; NULL if not found.
 */

const char *
syntax_scheme_type (const char *name)
{
    char *path;
    const char *type = NULL;

    path = find_scheme_file (name);
    if (path != NULL)
        type = g_str_has_suffix (path, SYNTAX_SCHEME_EXT) ? "Sublime" : "VS Code";
    g_free (path);
    return type;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_scheme_done (void)
{
    syntax_scheme_free (current_scheme);
    current_scheme = NULL;
    MC_PTR_FREE (current_name);
    MC_PTR_FREE (skin_scheme_name);
    MC_PTR_FREE (skin_name_seen);
    skin_config_seen = NULL;
    if (lookup_cache != NULL)
    {
        g_hash_table_destroy (lookup_cache);
        lookup_cache = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
