/*
   Editor tree-sitter syntax highlighting.

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
 *  \brief Source: editor tree-sitter syntax highlighting
 */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_TREE_SITTER

#include <tree_sitter/api.h>
#ifdef TREE_SITTER_STATIC
#include "ts-grammars/ts-grammar-registry.h"
#else
#include "ts-grammar-loader.h"
#endif

#include "lib/global.h"
#include "lib/skin.h"
#include "lib/fileloc.h"  // EDIT_SYNTAX_DIR, EDIT_SYNTAX_TS_DIR
#include "lib/tty/key.h"  // is_idle()
#include "lib/strutil.h"  // utf string functions
#include "lib/util.h"     // whiteness

#include "edit-impl.h"
#include "editwidget.h"
#include "syntax_ts.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

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
    void *query;            // TSQuery* -- highlight query
    void *injection_query;  // TSQuery* -- nested injection query (or NULL if none)
    gboolean injection_query_loaded;  // TRUE once we tried to load it (success or fail)
} ts_dynamic_lang_t;


/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* Color mappings loaded from colors.ini.
   Key: "section:capture_name" (e.g., "default:keyword", "markdown:comment")
   Value: GINT_TO_POINTER(color_pair_id) */
static GHashTable *ts_color_map = NULL;

/* Color pair for ERROR nodes (red foreground).  Allocated on first use, -1 = not yet allocated. */
static int ts_error_color = -1;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */


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
 * Parse a color spec "foreground;background" and allocate an MC color pair.
 * A foreground of "-" means use the default color. Returns the color pair ID.
 */
static int
ts_alloc_color_from_spec (const char *spec)
{
    char *buf, *fg, *bg, *semi;
    tty_color_pair_t color;
    int result;

    buf = g_strdup (spec);
    g_strstrip (buf);

    semi = strchr (buf, ';');
    if (semi != NULL)
    {
        *semi = '\0';
        fg = buf;
        bg = semi + 1;
        if (*bg == '\0')
            bg = NULL;
    }
    else
    {
        fg = buf;
        bg = NULL;
    }

    if (fg != NULL && strcmp (fg, "-") == 0)
        fg = NULL;

    if (fg == NULL && bg == NULL)
        result = EDITOR_NORMAL_COLOR;
    else
    {
        color.fg = fg;
        color.bg = bg;
        color.attrs = NULL;
        result = this_try_alloc_color_pair (&color);
    }

    g_free (buf);
    return result;
}

/**
 * Load the colors.ini config file. Uses GKeyFile (INI format).
 * Sections: [default] for global colors, [grammar_name] for overrides.
 * User file takes precedence over system file (loaded first).
 */
static void
ts_load_color_config (void)
{
    const char *dirs[2];
    int d;

    if (ts_color_map != NULL)
        return;

    /* Allocate red color for ERROR nodes (parse failures) */
    ts_error_color = ts_alloc_color_from_spec ("red;");

    ts_color_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    dirs[0] = mc_config_get_data_path ();
    dirs[1] = mc_global.share_data_dir;

    /* Load system file first, then user file overwrites */
    for (d = 1; d >= 0; d--)
    {
        GKeyFile *kf;
        char *path;
        gchar **groups;
        gsize g_count;
        gsize gi;

        path = g_build_filename (dirs[d], EDIT_SYNTAX_TS_DIR, "colors.ini", (char *) NULL);
        kf = g_key_file_new ();

        if (!g_key_file_load_from_file (kf, path, G_KEY_FILE_NONE, NULL))
        {
            g_key_file_free (kf);
            g_free (path);
            continue;
        }
        g_free (path);

        groups = g_key_file_get_groups (kf, &g_count);
        for (gi = 0; gi < g_count; gi++)
        {
            gchar **keys;
            gsize k_count;
            gsize ki;

            keys = g_key_file_get_keys (kf, groups[gi], &k_count, NULL);
            if (keys == NULL)
                continue;

            for (ki = 0; ki < k_count; ki++)
            {
                gchar *value;
                char *map_key;
                int color_pair;

                value = g_key_file_get_value (kf, groups[gi], keys[ki], NULL);
                if (value == NULL)
                    continue;

                color_pair = ts_alloc_color_from_spec (value);

                map_key = g_strdup_printf ("%s:%s", groups[gi], keys[ki]);
                g_free (value);
                g_hash_table_insert (ts_color_map, map_key, GINT_TO_POINTER (color_pair));
            }

            g_strfreev (keys);
        }

        g_strfreev (groups);
        g_key_file_free (kf);
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Look up the color for a tree-sitter capture name.
 * Uses longest-prefix matching: tries the full name first, then strips
 * the last ".suffix" and retries until a match is found or exhausted.
 *
 * Lookup order for each prefix:
 *   1. [grammar_name]:capture (per-grammar override)
 *   2. [default]:capture (global default)
 */
static int
ts_capture_name_to_color (const char *capture_name, const char *grammar_name)
{
    char *name;
    char *dot;

    if (ts_color_map == NULL)
        return EDITOR_NORMAL_COLOR;

    name = g_strdup (capture_name);

    for (;;)
    {
        gpointer value;
        char *key;

        /* Try grammar-specific override first */
        if (grammar_name != NULL)
        {
            key = g_strdup_printf ("%s:%s", grammar_name, name);
            if (g_hash_table_lookup_extended (ts_color_map, key, NULL, &value))
            {
                g_free (key);
                g_free (name);
                return GPOINTER_TO_INT (value);
            }
            g_free (key);
        }

        /* Try default section */
        key = g_strdup_printf ("default:%s", name);
        if (g_hash_table_lookup_extended (ts_color_map, key, NULL, &value))
        {
            g_free (key);
            g_free (name);
            return GPOINTER_TO_INT (value);
        }
        g_free (key);

        /* Strip the last .suffix and retry */
        dot = strrchr (name, '.');
        if (dot == NULL)
            break;
        *dot = '\0';
    }

    g_free (name);

    {
        FILE *dbg = fopen ("/tmp/ts_color_debug.txt", "a");
        if (dbg != NULL)
        {
            fprintf (dbg, "UNRESOLVED: @%s grammar=%s hash_size=%u\n",
                     capture_name, grammar_name ? grammar_name : "NULL",
                     g_hash_table_size (ts_color_map));
            fclose (dbg);
        }
    }

    return EDITOR_NORMAL_COLOR;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Look up a config file by matching a value in the value list.
 * Reads <config_name> file where each line is: <key> <val1> <val2> ...
 * If value matches any val, returns g_strdup(key). Otherwise NULL.
 * User file is searched first; if found there, system file is not searched.
 * An empty value list means the grammar is disabled (skip it).
 */
static char *
ts_config_lookup_by_value (const char *config_name, const char *value)
{
    const char *dirs[2];
    int d;

    dirs[0] = mc_config_get_data_path ();
    dirs[1] = mc_global.share_data_dir;

    for (d = 0; d < 2; d++)
    {
        char *path;
        FILE *f;
        char *line = NULL;

        path = g_build_filename (dirs[d], EDIT_SYNTAX_TS_DIR, config_name, (char *) NULL);
        f = fopen (path, "r");
        g_free (path);

        if (f == NULL)
            continue;

        while (read_one_line (&line, f) != 0)
        {
            char *p, *key;

            p = line;

            /* Skip whitespace */
            while (*p != '\0' && whiteness (*p))
                p++;

            /* Skip comments and empty lines */
            if (*p == '#' || *p == '\0')
            {
                g_free (line);
                line = NULL;
                continue;
            }

            /* Extract key (first word) */
            key = p;
            while (*p != '\0' && !whiteness (*p))
                p++;

            /* If no values follow the key, grammar is disabled -- skip */
            if (*p == '\0')
            {
                g_free (line);
                line = NULL;
                continue;
            }
            *p++ = '\0';

            /* Check each value on the line */
            while (*p != '\0')
            {
                char *val;

                /* Skip whitespace */
                while (*p != '\0' && whiteness (*p))
                    p++;

                if (*p == '\0')
                    break;

                val = p;
                while (*p != '\0' && !whiteness (*p))
                    p++;
                if (*p != '\0')
                    *p++ = '\0';

                if (strcmp (val, value) == 0)
                {
                    char *result = g_strdup (key);

                    g_free (line);
                    fclose (f);
                    return result;
                }
            }

            g_free (line);
            line = NULL;
        }

        g_free (line);
        fclose (f);
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Look up a config file by matching the key (first field).
 * Returns g_strdup of the rest of the line (trimmed). Otherwise NULL.
 * User file takes precedence per-record.
 */
char *
ts_config_lookup_by_grammar (const char *config_name, const char *grammar_name)
{
    const char *dirs[2];
    int d;

    dirs[0] = mc_config_get_data_path ();
    dirs[1] = mc_global.share_data_dir;

    for (d = 0; d < 2; d++)
    {
        char *path;
        FILE *f;
        char *line = NULL;

        path = g_build_filename (dirs[d], EDIT_SYNTAX_TS_DIR, config_name, (char *) NULL);
        f = fopen (path, "r");
        g_free (path);

        if (f == NULL)
            continue;

        while (read_one_line (&line, f) != 0)
        {
            char *p, *key;

            p = line;

            /* Skip whitespace */
            while (*p != '\0' && whiteness (*p))
                p++;

            /* Skip comments and empty lines */
            if (*p == '#' || *p == '\0')
            {
                g_free (line);
                line = NULL;
                continue;
            }

            /* Extract key (first word) */
            key = p;
            while (*p != '\0' && !whiteness (*p))
                p++;
            if (*p == '\0')
            {
                /* Key only, no value */
                if (strcmp (key, grammar_name) == 0)
                {
                    g_free (line);
                    fclose (f);
                    return g_strdup ("");
                }
                g_free (line);
                line = NULL;
                continue;
            }
            *p++ = '\0';

            if (strcmp (key, grammar_name) == 0)
            {
                char *rest, *end;

                /* Skip whitespace before the rest */
                while (*p != '\0' && whiteness (*p))
                    p++;
                rest = p;

                /* Trim trailing whitespace */
                end = rest + strlen (rest) - 1;
                while (end > rest && whiteness (*end))
                    *end-- = '\0';

                {
                    char *result = g_strdup (rest);

                    g_free (line);
                    fclose (f);
                    return result;
                }
            }

            g_free (line);
            line = NULL;
        }

        g_free (line);
        fclose (f);
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Reverse lookup: given a config file and a display value (rest of line),
 * return the key (first field).  For example, looking up "Go Template" in
 * display-names returns "gotmpl".
 * Returns newly allocated string or NULL.
 */
char *
ts_config_reverse_lookup (const char *config_name, const char *display_value)
{
    const char *dirs[2];
    int d;

    dirs[0] = mc_config_get_data_path ();
    dirs[1] = mc_global.share_data_dir;

    for (d = 0; d < 2; d++)
    {
        char *path;
        FILE *f;
        char *line = NULL;

        path = g_build_filename (dirs[d], EDIT_SYNTAX_TS_DIR, config_name, (char *) NULL);
        f = fopen (path, "r");
        g_free (path);

        if (f == NULL)
            continue;

        while (read_one_line (&line, f) != 0)
        {
            char *p, *key, *rest, *end;

            p = line;
            while (*p != '\0' && whiteness (*p))
                p++;
            if (*p == '#' || *p == '\0')
            {
                g_free (line);
                line = NULL;
                continue;
            }

            key = p;
            while (*p != '\0' && !whiteness (*p))
                p++;
            if (*p == '\0')
            {
                g_free (line);
                line = NULL;
                continue;
            }
            *p++ = '\0';

            while (*p != '\0' && whiteness (*p))
                p++;
            rest = p;

            end = rest + strlen (rest) - 1;
            while (end > rest && whiteness (*end))
                *end-- = '\0';

            if (strcmp (rest, display_value) == 0)
            {
                char *result = g_strdup (key);

                g_free (line);
                fclose (f);
                return result;
            }

            g_free (line);
            line = NULL;
        }

        g_free (line);
        fclose (f);
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Extract interpreter name from a shebang line.
 * "#!/usr/bin/env python3" -> "python3"
 * "#!/usr/bin/python" -> "python"
 * Returns newly allocated string or NULL.
 */
static char *
ts_extract_interpreter (const char *first_line)
{
    const char *p;
    const char *word_start;
    const char *word_end;
    const char *basename_start;

    if (first_line == NULL || first_line[0] != '#' || first_line[1] != '!')
        return NULL;

    p = first_line + 2;

    /* Skip whitespace after #! */
    while (*p != '\0' && whiteness (*p))
        p++;

    if (*p == '\0')
        return NULL;

    /* Extract the first word (path to interpreter) */
    word_start = p;
    while (*p != '\0' && !whiteness (*p))
        p++;
    word_end = p;

    /* Get basename of the path */
    basename_start = word_end - 1;
    while (basename_start > word_start && *(basename_start - 1) != PATH_SEP)
        basename_start--;

    /* If the basename is "env", skip to the next word */
    if ((word_end - basename_start) == 3 && strncmp (basename_start, "env", 3) == 0)
    {
        /* Skip whitespace */
        while (*p != '\0' && whiteness (*p))
            p++;

        if (*p == '\0')
            return NULL;

        /* Skip any flags (words starting with -) */
        while (*p == '-')
        {
            while (*p != '\0' && !whiteness (*p))
                p++;
            while (*p != '\0' && whiteness (*p))
                p++;
        }

        if (*p == '\0')
            return NULL;

        word_start = p;
        while (*p != '\0' && !whiteness (*p))
            p++;
        word_end = p;

        /* Get basename again */
        basename_start = word_end - 1;
        while (basename_start > word_start && *(basename_start - 1) != PATH_SEP)
            basename_start--;
    }

    if (basename_start >= word_end)
        return NULL;

    return g_strndup (basename_start, (gsize) (word_end - basename_start));
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Find a matching grammar for the given filename and first line.
 * Precedence: exact filename > shebang > extension.
 * On match, fills grammar_name and display_name (newly allocated strings).
 * Returns TRUE if a match was found.
 */
static gboolean
ts_find_grammar (const char *filename, const char *first_line,
                 char **grammar_name, char **display_name)
{
    const char *basename;

    if (filename == NULL)
        return FALSE;

    *grammar_name = NULL;
    *display_name = NULL;

    basename = strrchr (filename, PATH_SEP);
    basename = (basename != NULL) ? basename + 1 : filename;

    /* 1. Exact filename match (most specific) */
    *grammar_name = ts_config_lookup_by_value ("filenames", basename);
    if (*grammar_name != NULL)
    {
        *display_name = ts_config_lookup_by_grammar ("display-names", *grammar_name);
        return TRUE;
    }

    /* 2. Shebang match (content-based) */
    if (first_line != NULL && first_line[0] == '#' && first_line[1] == '!')
    {
        char *interpreter = ts_extract_interpreter (first_line);

        if (interpreter != NULL)
        {
            *grammar_name = ts_config_lookup_by_value ("shebangs", interpreter);
            g_free (interpreter);
            if (*grammar_name != NULL)
            {
                *display_name = ts_config_lookup_by_grammar ("display-names", *grammar_name);
                return TRUE;
            }
        }
    }

    /* 3. Extension match (least specific) */
    {
        const char *dot = strrchr (basename, '.');

        if (dot != NULL)
        {
            *grammar_name = ts_config_lookup_by_value ("extensions", dot);
            if (*grammar_name != NULL)
            {
                *display_name = ts_config_lookup_by_grammar ("display-names", *grammar_name);
                return TRUE;
            }
        }
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Look up the wrappers config file to find a wrapper grammar for a given host.
 * The wrappers file format is:
 *   wrapper_grammar  content_node  host1 host2 ...
 *
 * If host_grammar matches one of the hosts, returns the wrapper grammar name
 * (newly allocated) and fills content_node (newly allocated).
 * Returns NULL if no wrapper handles this host.
 */
static char *
ts_find_wrapper_for_host (const char *host_grammar, char **content_node)
{
    const char *dirs[2];
    int d;

    *content_node = NULL;
    dirs[0] = mc_config_get_data_path ();
    dirs[1] = mc_global.share_data_dir;

    for (d = 0; d < 2; d++)
    {
        char *path;
        FILE *f;
        char *line = NULL;

        path = g_build_filename (dirs[d], EDIT_SYNTAX_TS_DIR, "wrappers", (char *) NULL);
        f = fopen (path, "r");
        g_free (path);

        if (f == NULL)
            continue;

        while (read_one_line (&line, f) != 0)
        {
            char *p, *wrapper, *node;

            p = line;
            while (*p != '\0' && whiteness (*p))
                p++;
            if (*p == '#' || *p == '\0')
            {
                g_free (line);
                line = NULL;
                continue;
            }

            /* Field 1: wrapper grammar name */
            wrapper = p;
            while (*p != '\0' && !whiteness (*p))
                p++;
            if (*p == '\0')
            {
                g_free (line);
                line = NULL;
                continue;
            }
            *p++ = '\0';

            /* Field 2: content node name */
            while (*p != '\0' && whiteness (*p))
                p++;
            node = p;
            while (*p != '\0' && !whiteness (*p))
                p++;
            if (*p == '\0')
            {
                g_free (line);
                line = NULL;
                continue;
            }
            *p++ = '\0';

            /* Remaining fields: host grammar names */
            while (*p != '\0')
            {
                char *host;

                while (*p != '\0' && whiteness (*p))
                    p++;
                if (*p == '\0')
                    break;

                host = p;
                while (*p != '\0' && !whiteness (*p))
                    p++;
                if (*p != '\0')
                    *p++ = '\0';

                if (strcmp (host, host_grammar) == 0)
                {
                    char *result = g_strdup (wrapper);

                    *content_node = g_strdup (node);
                    g_free (line);
                    fclose (f);
                    return result;
                }
            }

            g_free (line);
            line = NULL;
        }

        g_free (line);
        fclose (f);
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Look up the wrappers config file to find the content node for a wrapper grammar.
 * Returns the content node name (newly allocated), or NULL if not a wrapper.
 */
static char *
ts_find_wrapper_content_node (const char *wrapper_grammar)
{
    const char *dirs[2];
    int d;

    dirs[0] = mc_config_get_data_path ();
    dirs[1] = mc_global.share_data_dir;

    for (d = 0; d < 2; d++)
    {
        char *path;
        FILE *f;
        char *line = NULL;

        path = g_build_filename (dirs[d], EDIT_SYNTAX_TS_DIR, "wrappers", (char *) NULL);
        f = fopen (path, "r");
        g_free (path);

        if (f == NULL)
            continue;

        while (read_one_line (&line, f) != 0)
        {
            char *p, *wrapper, *node;

            p = line;
            while (*p != '\0' && whiteness (*p))
                p++;
            if (*p == '#' || *p == '\0')
            {
                g_free (line);
                line = NULL;
                continue;
            }

            /* Field 1: wrapper grammar name */
            wrapper = p;
            while (*p != '\0' && !whiteness (*p))
                p++;
            if (*p == '\0')
            {
                g_free (line);
                line = NULL;
                continue;
            }
            *p++ = '\0';

            if (strcmp (wrapper, wrapper_grammar) == 0)
            {
                char *result;

                /* Field 2: content node name */
                while (*p != '\0' && whiteness (*p))
                    p++;
                node = p;
                while (*p != '\0' && !whiteness (*p))
                    p++;
                *p = '\0';

                result = g_strdup (node);
                g_free (line);
                fclose (f);
                return result;
            }

            g_free (line);
            line = NULL;
        }

        g_free (line);
        fclose (f);
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Build and install an injection query that injects host_grammar into
 * content_node nodes of the current wrapper grammar.
 * Example: inject "yaml" into "text" nodes produces:
 *   ((text) @injection.content (#set! injection.language "yaml"))
 */
static void
ts_setup_wrapper_injection (WEdit *edit, const TSLanguage *lang,
                            const char *content_node, const char *host_grammar)
{
    char *inj_src;
    uint32_t inj_len;
    uint32_t eo;
    TSQueryError et;
    TSQuery *inj_query;

    inj_src = g_strdup_printf ("((%s) @injection.content (#set! injection.language \"%s\"))",
                               content_node, host_grammar);
    inj_len = (uint32_t) strlen (inj_src);
    inj_query = ts_query_new (lang, inj_src, inj_len, &eo, &et);
    g_free (inj_src);

    if (inj_query != NULL)
    {
        edit->ts.injection_query = inj_query;
        edit->ts.injection_lang_cache = g_hash_table_new (g_str_hash, g_str_equal);
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Load a query file. Search order:
 *   1. User queries: ~/.local/share/mc/syntax-ts/queries/
 *   2. System queries: $(datadir)/mc/syntax-ts/queries/
 * Returns a newly allocated string with the file contents, or NULL on failure.
 */
static char *
ts_load_query_file (const char *query_filename, uint32_t *out_len)
{
    const char *base_dirs[2];
    char *contents = NULL;
    gsize len = 0;
    int b;

    base_dirs[0] = mc_config_get_data_path ();
    base_dirs[1] = mc_global.share_data_dir;

    for (b = 0; b < 2; b++)
    {
        char *path;

        path = g_build_filename (base_dirs[b], EDIT_SYNTAX_TS_DIR, "queries",
                                 query_filename, (char *) NULL);
        if (g_file_get_contents (path, &contents, &len, NULL))
        {
            g_free (path);
            *out_len = (uint32_t) len;
            return contents;
        }
        g_free (path);
    }

    *out_len = 0;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Extract the value of a #set! predicate for a given key from a query pattern.
 * Returns the value string (owned by the query, do not free), or NULL if not found.
 */
static const char *
ts_get_set_predicate (TSQuery *query, uint32_t pattern_index, const char *key)
{
    uint32_t step_count;
    const TSQueryPredicateStep *steps;
    uint32_t i;

    steps = ts_query_predicates_for_pattern (query, pattern_index, &step_count);

    for (i = 0; i + 3 < step_count; i++)
    {
        /* #set! pattern: String("set!") String(key) String(value) Done */
        if (steps[i].type == TSQueryPredicateStepTypeString
            && steps[i + 1].type == TSQueryPredicateStepTypeString
            && steps[i + 2].type == TSQueryPredicateStepTypeString
            && steps[i + 3].type == TSQueryPredicateStepTypeDone)
        {
            uint32_t len;
            const char *name;

            name = ts_query_string_value_for_id (query, steps[i].value_id, &len);
            if (strcmp (name, "set!") == 0)
            {
                const char *prop;

                prop = ts_query_string_value_for_id (query, steps[i + 1].value_id, &len);
                if (strcmp (prop, key) == 0)
                    return ts_query_string_value_for_id (query, steps[i + 2].value_id, &len);
            }
        }
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read capture text from the edit buffer for a given node.
 * Returns a newly allocated string, or NULL if the node is too large.
 */
static char *
ts_read_node_text (WEdit *edit, TSNode node)
{
    uint32_t start, end, len, i;
    char *buf;

    start = ts_node_start_byte (node);
    end = ts_node_end_byte (node);
    len = end - start;

    if (len == 0 || len > 256)
        return NULL;

    buf = g_new (char, len + 1);
    for (i = 0; i < len; i++)
        buf[i] = (char) edit_buffer_get_byte (&edit->buffer, (off_t) (start + i));
    buf[len] = '\0';

    return buf;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Evaluate filter predicates (#eq?, #any-of?, #not-any-of?, #match?) for a match.
 * Returns TRUE if the match passes all predicates (should be used).
 * Returns FALSE if any predicate rejects the match (should be skipped).
 *
 * Predicate format in TSQueryPredicateStep arrays:
 *   #eq?        @capture "value" Done
 *   #any-of?    @capture "v1" "v2" ... Done
 *   #not-any-of? @capture "v1" "v2" ... Done
 *   #match?     @capture "regex" Done
 */
static gboolean
ts_evaluate_match_predicates (TSQuery *query, const TSQueryMatch *match, WEdit *edit)
{
    uint32_t step_count;
    const TSQueryPredicateStep *steps;
    uint32_t i;

    steps = ts_query_predicates_for_pattern (query, match->pattern_index, &step_count);
    if (step_count == 0)
        return TRUE;

    for (i = 0; i < step_count;)
    {
        uint32_t len;
        const char *pred_name;

        /* Each predicate starts with a String (the predicate name) */
        if (steps[i].type != TSQueryPredicateStepTypeString)
        {
            /* Skip to next Done */
            while (i < step_count && steps[i].type != TSQueryPredicateStepTypeDone)
                i++;
            if (i < step_count)
                i++;    /* skip Done */
            continue;
        }

        pred_name = ts_query_string_value_for_id (query, steps[i].value_id, &len);

        if (strcmp (pred_name, "eq?") == 0 && i + 3 <= step_count)
        {
            /* #eq? @capture "value" Done */
            if (steps[i + 1].type == TSQueryPredicateStepTypeCapture
                && steps[i + 2].type == TSQueryPredicateStepTypeString)
            {
                uint32_t cap_idx = steps[i + 1].value_id;
                const char *expected;
                uint32_t ci;
                gboolean found = FALSE;

                expected = ts_query_string_value_for_id (query, steps[i + 2].value_id, &len);

                for (ci = 0; ci < match->capture_count; ci++)
                {
                    if (match->captures[ci].index == cap_idx)
                    {
                        char *text = ts_read_node_text (edit, match->captures[ci].node);
                        if (text != NULL)
                        {
                            found = (strcmp (text, expected) == 0);
                            g_free (text);
                        }
                        break;
                    }
                }

                if (!found)
                    return FALSE;
            }
        }
        else if (strcmp (pred_name, "any-of?") == 0 && i + 2 <= step_count)
        {
            /* #any-of? @capture "v1" "v2" ... Done */
            if (steps[i + 1].type == TSQueryPredicateStepTypeCapture)
            {
                uint32_t cap_idx = steps[i + 1].value_id;
                char *text = NULL;
                gboolean matched = FALSE;
                uint32_t ci, j;

                /* Find capture text */
                for (ci = 0; ci < match->capture_count; ci++)
                {
                    if (match->captures[ci].index == cap_idx)
                    {
                        text = ts_read_node_text (edit, match->captures[ci].node);
                        break;
                    }
                }

                if (text != NULL)
                {
                    /* Check against each value string */
                    for (j = i + 2; j < step_count && steps[j].type == TSQueryPredicateStepTypeString;
                         j++)
                    {
                        const char *val;
                        val = ts_query_string_value_for_id (query, steps[j].value_id, &len);
                        if (strcmp (text, val) == 0)
                        {
                            matched = TRUE;
                            break;
                        }
                    }
                    g_free (text);
                }

                if (!matched)
                    return FALSE;
            }
        }
        else if (strcmp (pred_name, "not-any-of?") == 0 && i + 2 <= step_count)
        {
            /* #not-any-of? @capture "v1" "v2" ... Done */
            if (steps[i + 1].type == TSQueryPredicateStepTypeCapture)
            {
                uint32_t cap_idx = steps[i + 1].value_id;
                char *text = NULL;
                gboolean rejected = FALSE;
                uint32_t ci, j;

                for (ci = 0; ci < match->capture_count; ci++)
                {
                    if (match->captures[ci].index == cap_idx)
                    {
                        text = ts_read_node_text (edit, match->captures[ci].node);
                        break;
                    }
                }

                if (text != NULL)
                {
                    for (j = i + 2; j < step_count && steps[j].type == TSQueryPredicateStepTypeString;
                         j++)
                    {
                        const char *val;
                        val = ts_query_string_value_for_id (query, steps[j].value_id, &len);
                        if (strcmp (text, val) == 0)
                        {
                            rejected = TRUE;
                            break;
                        }
                    }
                    g_free (text);
                }

                if (rejected)
                    return FALSE;
            }
        }
        else if (strcmp (pred_name, "match?") == 0 && i + 3 <= step_count)
        {
            /* #match? @capture "regex" Done */
            if (steps[i + 1].type == TSQueryPredicateStepTypeCapture
                && steps[i + 2].type == TSQueryPredicateStepTypeString)
            {
                uint32_t cap_idx = steps[i + 1].value_id;
                const char *pattern;
                gboolean matched = FALSE;
                uint32_t ci;

                pattern = ts_query_string_value_for_id (query, steps[i + 2].value_id, &len);

                for (ci = 0; ci < match->capture_count; ci++)
                {
                    if (match->captures[ci].index == cap_idx)
                    {
                        char *text = ts_read_node_text (edit, match->captures[ci].node);
                        if (text != NULL)
                        {
                            matched = g_regex_match_simple (pattern, text, 0, 0);
                            g_free (text);
                        }
                        break;
                    }
                }

                if (!matched)
                    return FALSE;
            }
        }
        /* Skip #set! and other predicates we don't filter on */

        /* Advance to next Done */
        while (i < step_count && steps[i].type != TSQueryPredicateStepTypeDone)
            i++;
        if (i < step_count)
            i++;    /* skip Done */
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Initialize injection query for a grammar by loading <grammar_name>-injections.scm.
 * Called after the primary grammar is initialized.
 * Failure is non-fatal — highlighting works without injections.
 */
static void
ts_init_injections (WEdit *edit, const char *grammar_name, const TSLanguage *lang)
{
    char *query_filename;
    char *query_src;
    uint32_t query_len;
    TSQuery *inj_query;
    uint32_t error_offset;
    TSQueryError error_type;

    query_filename = g_strdup_printf ("%s-injections.scm", grammar_name);
    query_src = ts_load_query_file (query_filename, &query_len);
    g_free (query_filename);

    if (query_src == NULL)
        return; /* no injections for this grammar */

    inj_query = ts_query_new (lang, query_src, query_len, &error_offset, &error_type);
    g_free (query_src);

    if (inj_query == NULL)
        return; /* query compilation failed */

    /* Store the injection query and a cache for dynamic languages */
    edit->ts.injection_query = inj_query;
    edit->ts.injection_lang_cache = g_hash_table_new (g_str_hash, g_str_equal);
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
gboolean
ts_init_for_file (WEdit *edit, const char *forced_grammar)
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

    /* Load color mappings from config on first use */
    ts_load_color_config ();

    filename = vfs_path_as_str (edit->filename_vpath);

    if (forced_grammar != NULL)
    {
        /* Manual grammar selection — skip auto-detection */
        grammar_name = g_strdup (forced_grammar);
        display_name = ts_config_lookup_by_grammar ("display-names", forced_grammar);
        if (display_name == NULL)
            display_name = g_strdup (forced_grammar);
    }
    else if (!ts_find_grammar (filename, get_first_editor_line (edit), &grammar_name,
                               &display_name))
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

    /* If the parser produced an ERROR root (catastrophic parse failure), check
       the wrappers config to see if a wrapper grammar can handle this file.
       For example, a .yaml file with Go template syntax fails to parse as YAML,
       so a wrapper like gotmpl is tried.  If it succeeds, the original grammar
       is injected into the wrapper's content nodes (e.g. "text"). */
    if (ts_node_is_error (ts_tree_root_node (tree)))
    {
        char *content_node = NULL;
        char *wrapper_name;

        wrapper_name = ts_find_wrapper_for_host (grammar_name, &content_node);
        if (wrapper_name != NULL)
        {
            const TSLanguage *wrapper_lang;

            wrapper_lang = ts_grammar_registry_lookup (wrapper_name);
            if (wrapper_lang != NULL && ts_parser_set_language (parser, wrapper_lang))
            {
                TSTree *wrapper_tree;

                wrapper_tree = ts_parser_parse (parser, NULL, input);
                if (wrapper_tree != NULL
                    && !ts_node_is_error (ts_tree_root_node (wrapper_tree)))
                {
                    char *wrapper_display;

                    /* Inject the original grammar into the wrapper's content nodes */
                    ts_setup_wrapper_injection (edit, wrapper_lang, content_node,
                                                grammar_name);

                    ts_tree_delete (tree);
                    tree = wrapper_tree;
                    lang = wrapper_lang;
                    g_free (grammar_name);
                    grammar_name = g_strdup (wrapper_name);
                    g_free (display_name);
                    wrapper_display =
                        ts_config_lookup_by_grammar ("display-names", wrapper_name);
                    display_name =
                        (wrapper_display != NULL) ? wrapper_display : g_strdup (wrapper_name);
                }
                else
                {
                    /* Wrapper didn't help — restore original language and tree */
                    if (wrapper_tree != NULL)
                        ts_tree_delete (wrapper_tree);
                    ts_parser_set_language (parser, lang);
                }
            }

            g_free (wrapper_name);
        }

        g_free (content_node);
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
    edit->ts.parser = parser;
    edit->ts.tree = tree;
    edit->ts.highlight_query = query;
    edit->ts.highlights = g_array_new (FALSE, FALSE, sizeof (ts_highlight_entry_t));
    edit->ts.highlights_start = -1;
    edit->ts.highlights_end = -1;
    edit->ts.grammar_name = g_strdup (grammar_name);
    edit->ts.active = TRUE;
    edit->ts.need_reparse = FALSE;

    // Try to initialize language injection (e.g., markdown inline within markdown block)
    // Failure is non-fatal — highlighting works without injection.
    // Skip if injection was already set up (e.g., by gotmpl fallback).
    if (edit->ts.injection_query == NULL)
        ts_init_injections (edit, grammar_name, lang);

    /* For wrapper grammars with a compound extension (e.g., .md.gotmpl, .yaml.tmpl),
       inject the host language into the wrapper's content nodes based on the
       preceding extension.  The wrappers config defines which grammars are wrappers
       and what content node they use. */
    if (edit->ts.injection_query == NULL)
    {
        char *content_node;

        content_node = ts_find_wrapper_content_node (grammar_name);
        if (content_node != NULL)
        {
            const char *base;
            const char *last_dot;

            base = strrchr (filename, PATH_SEP);
            base = (base != NULL) ? base + 1 : filename;
            last_dot = strrchr (base, '.');

            /* Look for a second extension before the wrapper extension */
            if (last_dot != NULL && last_dot > base)
            {
                const char *prev_dot;
                char ext_buf[32];
                ptrdiff_t ext_len;

                prev_dot = g_strrstr_len (base, (gssize) (last_dot - base), ".");
                if (prev_dot != NULL)
                {
                    ext_len = last_dot - prev_dot;
                    if (ext_len > 0 && ext_len < (ptrdiff_t) sizeof (ext_buf))
                    {
                        char *host_grammar;

                        memcpy (ext_buf, prev_dot, (size_t) ext_len);
                        ext_buf[ext_len] = '\0';

                        host_grammar = ts_config_lookup_by_value ("extensions", ext_buf);
                        if (host_grammar != NULL)
                        {
                            ts_setup_wrapper_injection (edit, lang, content_node,
                                                        host_grammar);
                            g_free (host_grammar);
                        }
                    }
                }
            }

            g_free (content_node);
        }
    }

    g_free (edit->syntax_type);
    edit->syntax_type = display_name;  // takes ownership

    g_free (grammar_name);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Free all tree-sitter resources associated with the edit widget.
 */
void
ts_free (WEdit *edit)
{
    // Free injection resources
    if (edit->ts.injection_query != NULL)
    {
        ts_query_delete ((TSQuery *) edit->ts.injection_query);
        edit->ts.injection_query = NULL;
    }

    if (edit->ts.injection_lang_cache != NULL)
    {
        GHashTableIter iter;
        gpointer key, value;

        g_hash_table_iter_init (&iter, edit->ts.injection_lang_cache);
        while (g_hash_table_iter_next (&iter, &key, &value))
        {
            ts_dynamic_lang_t *dl = (ts_dynamic_lang_t *) value;

            if (dl->query != NULL)
                ts_query_delete ((TSQuery *) dl->query);
            if (dl->injection_query != NULL)
                ts_query_delete ((TSQuery *) dl->injection_query);
            if (dl->parser != NULL)
                ts_parser_delete ((TSParser *) dl->parser);
            g_free (dl);
            g_free (key);
        }
        g_hash_table_destroy (edit->ts.injection_lang_cache);
        edit->ts.injection_lang_cache = NULL;
    }

    // Free primary resources
    if (edit->ts.highlight_query != NULL)
    {
        ts_query_delete ((TSQuery *) edit->ts.highlight_query);
        edit->ts.highlight_query = NULL;
    }

    if (edit->ts.tree != NULL)
    {
        ts_tree_delete ((TSTree *) edit->ts.tree);
        edit->ts.tree = NULL;
    }

    if (edit->ts.parser != NULL)
    {
        ts_parser_delete ((TSParser *) edit->ts.parser);
        edit->ts.parser = NULL;
    }

    if (edit->ts.highlights != NULL)
    {
        g_array_free (edit->ts.highlights, TRUE);
        edit->ts.highlights = NULL;
    }

    g_free (edit->ts.grammar_name);
    edit->ts.grammar_name = NULL;
    edit->ts.highlights_start = -1;
    edit->ts.highlights_end = -1;
    edit->ts.active = FALSE;

    /* Clear the global color map so it's reloaded on next init.
       This is needed because tty_color_free_temp() (called by
       edit_free_syntax_rules) invalidates all temporary color pairs,
       including the ones stored in ts_color_map. */
    if (ts_color_map != NULL)
    {
        g_hash_table_destroy (ts_color_map);
        ts_color_map = NULL;
        ts_error_color = -1;
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Run a highlight query on a tree and append results to the highlights array.
 * grammar_name is used for per-grammar color lookup in colors.ini.
 * Evaluates filter predicates (#eq?, #match?, etc.) to skip non-matching captures.
 */
static void
ts_run_query_into_highlights (TSQuery *query, TSTree *tree, uint32_t range_start,
                              uint32_t range_end, GArray *highlights,
                              const char *grammar_name, WEdit *edit)
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

        /* Evaluate filter predicates (#eq?, #match?, #any-of?, etc.) */
        if (edit != NULL && !ts_evaluate_match_predicates (query, &match, edit))
            continue;

        for (ci = 0; ci < match.capture_count; ci++)
        {
            TSQueryCapture cap = match.captures[ci];
            uint32_t cap_name_len;
            const char *cap_name;
            ts_highlight_entry_t entry;

            cap_name = ts_query_capture_name_for_id (query, cap.index, &cap_name_len);

            entry.start_byte = ts_node_start_byte (cap.node);
            entry.end_byte = ts_node_end_byte (cap.node);
            entry.color = ts_capture_name_to_color (cap_name, grammar_name);

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
ts_get_dynamic_lang (GHashTable *lang_cache, const char *lang_name)
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

    dl = (ts_dynamic_lang_t *) g_hash_table_lookup (lang_cache, lang_name);
    if (dl != NULL)
        return dl->parser != NULL ? dl : NULL;

    // Not cached yet — try to create
    lang = ts_grammar_registry_lookup (lang_name);
    if (lang == NULL)
    {
        // Cache a NULL entry so we don't retry
        dl = g_new0 (ts_dynamic_lang_t, 1);
        g_hash_table_insert (lang_cache, g_strdup (lang_name), dl);
        return NULL;
    }

    parser = ts_parser_new ();
    if (!ts_parser_set_language (parser, lang))
    {
        ts_parser_delete (parser);
        dl = g_new0 (ts_dynamic_lang_t, 1);
        g_hash_table_insert (lang_cache, g_strdup (lang_name), dl);
        return NULL;
    }

    query_filename = g_strdup_printf ("%s-highlights.scm", lang_name);
    query_src = ts_load_query_file (query_filename, &query_len);
    g_free (query_filename);

    if (query_src == NULL)
    {
        ts_parser_delete (parser);
        dl = g_new0 (ts_dynamic_lang_t, 1);
        g_hash_table_insert (lang_cache, g_strdup (lang_name), dl);
        return NULL;
    }

    query = ts_query_new (lang, query_src, query_len, &error_offset, &error_type);
    g_free (query_src);

    if (query == NULL)
    {
        ts_parser_delete (parser);
        dl = g_new0 (ts_dynamic_lang_t, 1);
        g_hash_table_insert (lang_cache, g_strdup (lang_name), dl);
        return NULL;
    }

    dl = g_new0 (ts_dynamic_lang_t, 1);
    dl->parser = parser;
    dl->query = query;
    g_hash_table_insert (lang_cache, g_strdup (lang_name), dl);
    return dl;
}

/**
 * Recursively collect ERROR nodes within a byte range and append red highlights.
 */
static void
ts_collect_error_highlights (TSNode node, uint32_t range_start, uint32_t range_end,
                             GArray *highlights)
{
    uint32_t start, end, i, child_count;

    start = ts_node_start_byte (node);
    end = ts_node_end_byte (node);

    /* Skip nodes entirely outside the range */
    if (end <= range_start || start >= range_end)
        return;

    if (ts_node_is_error (node))
    {
        ts_highlight_entry_t entry;

        entry.start_byte = start;
        entry.end_byte = end;
        entry.color = ts_error_color;
        g_array_append_val (highlights, entry);
        return;  /* Don't recurse into ERROR children */
    }

    child_count = ts_node_child_count (node);
    for (i = 0; i < child_count; i++)
        ts_collect_error_highlights (ts_node_child (node, i), range_start, range_end, highlights);
}

/* --------------------------------------------------------------------------------------------- */

/* Maximum nesting depth for recursive injection (e.g., gotmpl -> markdown -> python).
   Prevents infinite loops from circular injection configurations. */
#define TS_MAX_INJECTION_DEPTH 3

/**
 * Parse an injected language within a content node range and run its highlights.
 * If the injected language has its own injections.scm, recurse up to max_depth.
 */
static void
ts_inject_and_highlight (const char *lang_name, TSNode content_node, TSInput input,
                         uint32_t range_start, uint32_t range_end, GArray *highlights,
                         GHashTable *lang_cache, WEdit *edit, int depth)
{
    ts_dynamic_lang_t *dl;
    TSRange r;
    TSTree *inject_tree;

    dl = ts_get_dynamic_lang (lang_cache, lang_name);
    if (dl == NULL)
        return;

    r.start_point = ts_node_start_point (content_node);
    r.end_point = ts_node_end_point (content_node);
    r.start_byte = ts_node_start_byte (content_node);
    r.end_byte = ts_node_end_byte (content_node);

    ts_parser_set_included_ranges ((TSParser *) dl->parser, &r, 1);
    inject_tree = ts_parser_parse ((TSParser *) dl->parser, NULL, input);
    if (inject_tree == NULL)
        return;

    ts_run_query_into_highlights ((TSQuery *) dl->query, inject_tree,
                                 range_start, range_end, highlights, lang_name, edit);

    /* Recurse: if the injected language has its own injections.scm,
       process nested injections (e.g., markdown -> markdown_inline,
       markdown -> python for fenced code blocks). */
    if (depth > 0)
    {
        const TSLanguage *inj_lang;

        inj_lang = ts_grammar_registry_lookup (lang_name);
        if (inj_lang != NULL)
        {
            TSQuery *nested_inj_query;

            /* Lazy-load the nested injection query and cache it on the
               dynamic language entry to avoid recompiling on every call. */
            if (!dl->injection_query_loaded)
            {
                char *nested_inj_filename;
                char *nested_inj_src;
                uint32_t nested_inj_len;

                nested_inj_filename = g_strdup_printf ("%s-injections.scm", lang_name);
                nested_inj_src = ts_load_query_file (nested_inj_filename, &nested_inj_len);
                g_free (nested_inj_filename);

                if (nested_inj_src != NULL)
                {
                    uint32_t eo;
                    TSQueryError et;

                    dl->injection_query =
                        ts_query_new (inj_lang, nested_inj_src, nested_inj_len, &eo, &et);
                    g_free (nested_inj_src);
                }
                dl->injection_query_loaded = TRUE;
            }

            nested_inj_query = (TSQuery *) dl->injection_query;

            if (nested_inj_query != NULL)
            {
                TSNode inj_root;
                TSQueryCursor *nested_cursor;
                TSQueryMatch nested_match;

                inj_root = ts_tree_root_node (inject_tree);
                nested_cursor = ts_query_cursor_new ();
                ts_query_cursor_set_byte_range (nested_cursor, range_start, range_end);
                ts_query_cursor_exec (nested_cursor, nested_inj_query, inj_root);

                while (ts_query_cursor_next_match (nested_cursor, &nested_match))
                {
                    TSNode nested_content = { .id = NULL };
                    TSNode nested_lang_node = { .id = NULL };
                    const char *nested_static_lang = NULL;
                    uint32_t nci;

                    if (!ts_evaluate_match_predicates (nested_inj_query, &nested_match, edit))
                        continue;

                    for (nci = 0; nci < nested_match.capture_count; nci++)
                    {
                        uint32_t nlen;
                        const char *ncap;

                        ncap = ts_query_capture_name_for_id (nested_inj_query,
                                                              nested_match.captures[nci].index,
                                                              &nlen);
                        if (strcmp (ncap, "injection.content") == 0)
                            nested_content = nested_match.captures[nci].node;
                        else if (strcmp (ncap, "injection.language") == 0)
                            nested_lang_node = nested_match.captures[nci].node;
                    }

                    if (ts_node_is_null (nested_content))
                        continue;

                    nested_static_lang =
                        ts_get_set_predicate (nested_inj_query, nested_match.pattern_index,
                                              "injection.language");

                    if (nested_static_lang != NULL)
                    {
                        ts_inject_and_highlight (nested_static_lang, nested_content, input,
                                                 range_start, range_end, highlights,
                                                 lang_cache, edit, depth - 1);
                    }
                    else if (!ts_node_is_null (nested_lang_node))
                    {
                        uint32_t ls = ts_node_start_byte (nested_lang_node);
                        uint32_t le = ts_node_end_byte (nested_lang_node);
                        uint32_t ll = le - ls;

                        if (ll > 0 && ll < 64)
                        {
                            char lbuf[64];
                            uint32_t li;
                            char *s, *e;

                            for (li = 0; li < ll; li++)
                                lbuf[li] = (char) edit_buffer_get_byte (&edit->buffer,
                                                                        (off_t) (ls + li));
                            lbuf[ll] = '\0';

                            s = lbuf;
                            while (*s == ' ' || *s == '\t')
                                s++;
                            e = s + strlen (s);
                            while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n'
                                             || e[-1] == '\r'))
                                e--;
                            *e = '\0';

                            if (*s != '\0')
                                ts_inject_and_highlight (s, nested_content, input,
                                                         range_start, range_end, highlights,
                                                         lang_cache, edit, depth - 1);
                        }
                    }
                }

                ts_query_cursor_delete (nested_cursor);
            }
        }
    }

    ts_tree_delete (inject_tree);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Rebuild the highlight cache for the given byte range.
 * Runs the highlight query and collects (start_byte, end_byte, color) entries.
 * If injection is active, also runs the injection query on target node ranges.
 */
void
ts_rebuild_highlight_cache (WEdit *edit, off_t range_start, off_t range_end)
{
    TSTree *tree;
    TSInput input;
    gboolean had_edit;

    if (!edit->ts.active)
        return;

    input.payload = edit;
    input.read = ts_input_read;
    input.encoding = TSInputEncodingUTF8;

    /* Remember if this rebuild was triggered by an edit (vs scroll).
       During rapid editing, we skip the expensive injection processing
       to keep the render responsive.  Injections are only refreshed when
       rebuilding due to a scroll (cache invalidated by viewport change). */
    had_edit = edit->ts.need_reparse;

    // Perform deferred re-parse if the tree was edited since last parse
    if (edit->ts.need_reparse)
    {
        TSTree *new_tree;

        new_tree =
            ts_parser_parse ((TSParser *) edit->ts.parser, (TSTree *) edit->ts.tree, input);
        if (new_tree != NULL)
        {
            ts_tree_delete ((TSTree *) edit->ts.tree);
            edit->ts.tree = new_tree;
        }

        edit->ts.need_reparse = FALSE;
    }

    tree = (TSTree *) edit->ts.tree;

    g_array_set_size (edit->ts.highlights, 0);

    // Run the primary highlight query
    ts_run_query_into_highlights ((TSQuery *) edit->ts.highlight_query, tree,
                                 (uint32_t) range_start, (uint32_t) range_end,
                                 edit->ts.highlights, edit->ts.grammar_name, edit);

    /* Run injection queries if configured.  Skip during rapid edits to keep
       rendering responsive - injections are expensive (parse + query for each
       injected range).  Edits that don't change injection structure won't lose
       much; the next idle rebuild will refresh injections. */
    if (edit->ts.injection_query != NULL && (!had_edit || is_idle ()))
    {
        TSNode root;
        TSQuery *inj_query;
        TSQueryCursor *inj_cursor;
        TSQueryMatch match;

        root = ts_tree_root_node (tree);
        inj_query = (TSQuery *) edit->ts.injection_query;

        inj_cursor = ts_query_cursor_new ();
        ts_query_cursor_set_byte_range (inj_cursor, (uint32_t) range_start,
                                        (uint32_t) range_end);
        ts_query_cursor_exec (inj_cursor, inj_query, root);

        while (ts_query_cursor_next_match (inj_cursor, &match))
        {
            TSNode content_node = { .id = NULL };
            TSNode lang_node = { .id = NULL };
            const char *static_lang = NULL;
            uint32_t ci;

            if (!ts_evaluate_match_predicates (inj_query, &match, edit))
                continue;

            for (ci = 0; ci < match.capture_count; ci++)
            {
                uint32_t name_len;
                const char *cap_name;

                cap_name =
                    ts_query_capture_name_for_id (inj_query, match.captures[ci].index, &name_len);

                if (strcmp (cap_name, "injection.content") == 0)
                    content_node = match.captures[ci].node;
                else if (strcmp (cap_name, "injection.language") == 0)
                    lang_node = match.captures[ci].node;
            }

            if (ts_node_is_null (content_node))
                continue;

            static_lang = ts_get_set_predicate (inj_query, match.pattern_index,
                                                "injection.language");

            if (static_lang != NULL)
            {
                ts_inject_and_highlight (static_lang, content_node, input,
                                         (uint32_t) range_start, (uint32_t) range_end,
                                         edit->ts.highlights, edit->ts.injection_lang_cache,
                                         edit, TS_MAX_INJECTION_DEPTH);
            }
            else if (!ts_node_is_null (lang_node))
            {
                uint32_t lang_start = ts_node_start_byte (lang_node);
                uint32_t lang_end = ts_node_end_byte (lang_node);
                uint32_t lang_len = lang_end - lang_start;

                if (lang_len > 0 && lang_len < 64)
                {
                    char lang_buf[64];
                    uint32_t li;
                    char *s, *e;

                    for (li = 0; li < lang_len; li++)
                        lang_buf[li] =
                            (char) edit_buffer_get_byte (&edit->buffer,
                                                        (off_t) (lang_start + li));
                    lang_buf[lang_len] = '\0';

                    s = lang_buf;
                    while (*s == ' ' || *s == '\t')
                        s++;
                    e = s + strlen (s);
                    while (e > s
                           && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
                        e--;
                    *e = '\0';

                    if (*s != '\0')
                        ts_inject_and_highlight (s, content_node, input,
                                                 (uint32_t) range_start, (uint32_t) range_end,
                                                 edit->ts.highlights,
                                                 edit->ts.injection_lang_cache,
                                                 edit, TS_MAX_INJECTION_DEPTH);
                }
            }
        }

        ts_query_cursor_delete (inj_cursor);
    }

    /* Collect ERROR nodes and highlight them in red.  ERROR entries are appended
       last so that valid captures (which are narrower) take precedence via the
       "narrower wins" rule in ts_get_color_at(). */
    if (ts_error_color >= 0)
    {
        TSNode root;

        root = ts_tree_root_node (tree);

        /* Only collect ERROR highlights if the root is NOT itself an ERROR node.
           When the root is ERROR (e.g. macro-heavy C files like sqlite3.c), the
           entire file would be painted red, which is unhelpful.  In that case,
           let the highlight query captures show whatever they can and leave
           uncaptured regions as DEFAULT. */
        if (!ts_node_is_error (root))
        {
            ts_collect_error_highlights (root, (uint32_t) range_start,
                                         (uint32_t) range_end, edit->ts.highlights);

            /* If the tree root does not cover the full visible range (e.g. the
               parser gave up early), color the uncovered tail as error too. */
            if (ts_node_end_byte (root) < (uint32_t) range_end)
            {
                ts_highlight_entry_t entry;
                uint32_t gap_start = ts_node_end_byte (root);

                if (gap_start < (uint32_t) range_start)
                    gap_start = (uint32_t) range_start;

                entry.start_byte = gap_start;
                entry.end_byte = (uint32_t) range_end;
                entry.color = ts_error_color;
                g_array_append_val (edit->ts.highlights, entry);
            }
        }
    }

    edit->ts.highlights_start = range_start;
    edit->ts.highlights_end = range_end;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Look up the color for a byte index in the highlight cache.
 * Returns the color of the most specific (last) matching entry,
 * since tree-sitter queries return matches in order from general to specific.
 */
int
ts_get_color_at (WEdit *edit, off_t byte_index)
{
    guint i;
    int color = EDITOR_NORMAL_COLOR;
    uint32_t match_start = 0;
    uint32_t match_end = UINT32_MAX;

    if (edit->ts.highlights == NULL)
        return EDITOR_NORMAL_COLOR;

    for (i = 0; i < edit->ts.highlights->len; i++)
    {
        ts_highlight_entry_t *e;

        e = &g_array_index (edit->ts.highlights, ts_highlight_entry_t, i);

        if ((off_t) e->start_byte <= byte_index && byte_index < (off_t) e->end_byte)
        {
            /* For overlapping ranges, prefer the narrower (more specific) one.
               For equal ranges, keep the first match (more specific pattern). */
            uint32_t width = e->end_byte - e->start_byte;
            uint32_t cur_width = match_end - match_start;

            if (color == EDITOR_NORMAL_COLOR || width < cur_width)
            {
                color = e->color;
                match_start = e->start_byte;
                match_end = e->end_byte;
            }
        }
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

    if (!edit->ts.active || edit->ts.tree == NULL || edit->ts.parser == NULL)
        return;

    ts_edit.start_byte = (uint32_t) start_byte;
    ts_edit.old_end_byte = (uint32_t) old_end_byte;
    ts_edit.new_end_byte = (uint32_t) new_end_byte;
    ts_edit.start_point = (TSPoint){ 0, 0 };
    ts_edit.old_end_point = (TSPoint){ 0, 0 };
    ts_edit.new_end_point = (TSPoint){ 0, 0 };

    ts_tree_edit ((TSTree *) edit->ts.tree, &ts_edit);

    // Mark tree as needing re-parse; defer actual parsing to cache rebuild
    edit->ts.need_reparse = TRUE;

    // Invalidate highlight cache
    edit->ts.highlights_start = -1;
    edit->ts.highlights_end = -1;
}

#endif /* HAVE_TREE_SITTER */
