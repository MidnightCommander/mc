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
#include "lib/search.h"   // mc_search
#include "lib/skin.h"
#include "lib/fileloc.h"  // EDIT_SYNTAX_DIR, EDIT_SYNTAX_TS_DIR
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


/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

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
gboolean
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
void
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
void
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
int
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
