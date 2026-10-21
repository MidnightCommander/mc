/*
 * mc-syntax-dump.c - Dump MC syntax highlighting as ANSI-colored text.
 *
 * Uses MC's actual syntax engine internals for exact color output.
 *
 * Usage:
 *   mc-syntax-dump [--ts|--legacy] <source-file>
 *   mc-syntax-dump --ts --grammar-dir DIR --lib-dir DIR <source-file>
 *
 * When --grammar-dir and --lib-dir are given, loads the grammar directly
 * from those paths instead of using MC's installed grammar discovery.
 * This allows testing queries from a development checkout without
 * installing them.
 *
 *   --grammar-dir DIR  Path to a per-grammar directory containing
 *                      highlights.scm, config.ini, and optionally
 *                      injections.scm.
 *   --lib-dir DIR      Path to directory containing <grammar>.so files.
 */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <signal.h>
#include <execinfo.h>

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/fileloc.h"
#include "lib/skin.h"
#include "lib/tty/color.h"
#include "lib/tty/color-internal.h"
#include "lib/tty/tty.h"
#include "lib/vfs/vfs.h"
#include "lib/widget.h"
#include "lib/mcconfig.h"

#include "src/vfs/local/local.c"

#include "src/editor/editwidget.h"
#include "src/editor/edit-impl.h"
#ifdef HAVE_TREE_SITTER
#include "src/editor/syntax_ts.h"
#include <gmodule.h>
#include <tree_sitter/api.h>
#endif

static void
crash_handler (int sig)
{
    void *bt[30];
    int n = backtrace (bt, 30);
    fprintf (stderr, "Signal %d, backtrace:\n", sig);
    backtrace_symbols_fd (bt, n, 2);
    _exit (1);
}

/* Mocks */
void mc_refresh (void) {}
gboolean edit_load_macro_cmd (WEdit *_edit) { (void) _edit; return FALSE; }

/* Wrap tty_color_init_lib and tty_color_try_alloc_lib_pair to avoid
   needing a real terminal (SLang/ncurses). */
void __wrap_tty_color_init_lib (gboolean disable, gboolean force)
{
    (void) disable;
    (void) force;
}

void __wrap_tty_color_try_alloc_lib_pair (tty_color_lib_pair_t *mc_color_pair)
{
    (void) mc_color_pair;
}

void __wrap_tty_color_deinit_lib (void)
{
}

#ifdef HAVE_TREE_SITTER
/* Duplicated from syntax_ts.c (file-scope types not visible to us) */
typedef struct
{
    uint32_t start_byte;
    uint32_t end_byte;
    int color;
} ts_highlight_entry_t;

/*
 * TSInput read callback: reads chunks of text from the edit buffer.
 * Duplicated from syntax_ts.c because the original is static.
 */
static const char *
ts_input_read_cb (void *payload, uint32_t byte_index, TSPoint position, uint32_t *bytes_read)
{
    static char buf[4096];
    WEdit *edit_buf = (WEdit *) payload;
    uint32_t i;

    (void) position;

    for (i = 0; i < sizeof (buf) && (off_t) (byte_index + i) < edit_buf->buffer.size; i++)
        buf[i] = edit_buffer_get_byte (&edit_buf->buffer, (off_t) (byte_index + i));

    *bytes_read = i;
    return (i > 0) ? buf : NULL;
}
#endif

/* Wrap ts_init_for_file so we can disable it for --legacy mode */
static gboolean ts_init_disabled = FALSE;
extern gboolean __real_ts_init_for_file (WEdit *edit, const char *forced_grammar);

gboolean __wrap_ts_init_for_file (WEdit *edit, const char *forced_grammar)
{
    if (ts_init_disabled)
        return FALSE;
    return __real_ts_init_for_file (edit, forced_grammar);
}

/* ------------------------------------------------------------------ */
/* Color pair reverse mapping                                         */
/* ------------------------------------------------------------------ */

static const char *ansi_codes[16] = {
    "\033[30m", "\033[31m", "\033[32m", "\033[33m",
    "\033[34m", "\033[35m", "\033[36m", "\033[37m",
    "\033[90m", "\033[91m", "\033[92m", "\033[93m",
    "\033[94m", "\033[95m", "\033[96m", "\033[97m",
};

static const char *color_names[16] = {
    "black", "red", "green", "brown",
    "blue", "magenta", "cyan", "lightgray",
    "gray", "brightred", "brightgreen", "yellow",
    "brightblue", "brightmagenta", "brightcyan", "white",
};

#define CACHE_SIZE 4096
static struct { int pair_index; int fg_index; } pair_cache[CACHE_SIZE];
static int pair_cache_count = 0;

static const char *
pair_index_to_fg_ansi (int pair_idx)
{
    int i, fg, bg;

    if (pair_idx < 0)
        return "";

    for (i = 0; i < pair_cache_count; i++)
        if (pair_cache[i].pair_index == pair_idx)
            return ansi_codes[pair_cache[i].fg_index];

    for (fg = 0; fg < 16; fg++)
    {
        for (bg = 0; bg < 17; bg++)
        {
            tty_color_pair_t cp;
            int test_idx;

            cp.fg = (char *) color_names[fg];
            cp.bg = (bg < 16) ? (char *) color_names[bg] : NULL;
            cp.attrs = NULL;
            cp.pair_index = 0;

            test_idx = tty_try_alloc_color_pair (&cp, FALSE);
            if (test_idx == pair_idx)
            {
                if (pair_cache_count < CACHE_SIZE)
                {
                    pair_cache[pair_cache_count].pair_index = pair_idx;
                    pair_cache[pair_cache_count].fg_index = fg;
                    pair_cache_count++;
                }
                return ansi_codes[fg];
            }
        }
    }

    return "";
}

/* ------------------------------------------------------------------ */
/* Direct grammar loading (--grammar-dir / --lib-dir)                 */
/* ------------------------------------------------------------------ */

#ifdef HAVE_TREE_SITTER

/*
 * Convert config.ini [colors] section into a temporary colors.ini file
 * in the MC format (section = grammar name, keys = capture names).
 * This lets MC's existing ts_load_grammar_registry() pick up the colors
 * without modifying the static ts_color_map in syntax_ts.c.
 *
 * The temporary file is written into a temp directory, and MC's
 * data path is pointed there so ts_load_grammar_registry() finds it.
 */
static char *
create_temp_colors_ini (const char *config_path, const char *grammar_name)
{
    GKeyFile *kf;
    gchar **keys;
    gsize k_count, ki;
    char *tmpdir;
    char *ts_dir;
    char *colors_path;
    FILE *f;

    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, config_path, G_KEY_FILE_NONE, NULL))
    {
        g_key_file_free (kf);
        return NULL;
    }

    keys = g_key_file_get_keys (kf, "colors", &k_count, NULL);
    if (keys == NULL)
    {
        g_key_file_free (kf);
        return NULL;
    }

    tmpdir = g_dir_make_tmp ("mc-syntax-dump-XXXXXX", NULL);
    if (tmpdir == NULL)
    {
        g_strfreev (keys);
        g_key_file_free (kf);
        return NULL;
    }

    ts_dir = g_build_filename (tmpdir, "syntax-ts", (char *) NULL);
    g_mkdir_with_parents (ts_dir, 0700);

    colors_path = g_build_filename (ts_dir, "colors.ini", (char *) NULL);
    f = fopen (colors_path, "w");
    g_free (colors_path);
    g_free (ts_dir);

    if (f == NULL)
    {
        g_strfreev (keys);
        g_key_file_free (kf);
        g_free (tmpdir);
        return NULL;
    }

    fprintf (f, "[%s]\n", grammar_name);
    for (ki = 0; ki < k_count; ki++)
    {
        gchar *value = g_key_file_get_value (kf, "colors", keys[ki], NULL);
        if (value != NULL)
        {
            fprintf (f, "%s = %s\n", keys[ki], value);
            g_free (value);
        }
    }

    fclose (f);
    g_strfreev (keys);
    g_key_file_free (kf);

    return tmpdir;
}

/*
 * Read the symbol= field from config.ini [grammar] section.
 * Returns the symbol suffix or NULL if not specified.
 * Caller must free.
 */
static char *
read_symbol_from_config (const char *config_path)
{
    GKeyFile *kf;
    char *value;

    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, config_path, G_KEY_FILE_NONE, NULL))
    {
        g_key_file_free (kf);
        return NULL;
    }

    value = g_key_file_get_value (kf, "grammar", "symbol", NULL);
    g_key_file_free (kf);

    if (value != NULL)
        g_strstrip (value);

    return value;
}

/*
 * Initialize tree-sitter for an edit buffer by loading the grammar,
 * query, and colors directly from the specified directories.
 * Returns TRUE on success.
 */
static gboolean
ts_init_direct (WEdit *edit, const char *grammar_dir, const char *lib_dir)
{
    char *dir_basename;
    char *grammar_name;
    char *config_path;
    char *highlights_path;
    char *so_path;
    char *symbol_override;
    char *symbol_name;
    char *query_src;
    gsize query_len;
    GModule *module;
    gpointer symbol;
    const TSLanguage *(*lang_func) (void);
    const TSLanguage *lang;
    TSParser *parser;
    TSTree *tree;
    TSInput input;
    uint32_t error_offset;
    TSQueryError error_type;
    TSQuery *query;

    /* Grammar name from directory basename */
    dir_basename = g_path_get_basename (grammar_dir);
    grammar_name = g_strdup (dir_basename);
    g_free (dir_basename);

    config_path = g_build_filename (grammar_dir, "config.ini", (char *) NULL);
    highlights_path = g_build_filename (grammar_dir, "highlights.scm", (char *) NULL);

    /* Load colors from our temp colors.ini */
    {
        extern void ts_load_grammar_registry (void);
        ts_load_grammar_registry ();
    }

    /* Determine symbol name */
    symbol_override = read_symbol_from_config (config_path);
    if (symbol_override != NULL)
    {
        symbol_name = g_strdup_printf ("tree_sitter_%s", symbol_override);
        g_free (symbol_override);
    }
    else
        symbol_name = g_strdup_printf ("tree_sitter_%s", grammar_name);

    /* Load .so from lib_dir */
    so_path = g_strdup_printf ("%s/%s", lib_dir, grammar_name);
    module = g_module_open (so_path, G_MODULE_BIND_LAZY);
    g_free (so_path);

    if (module == NULL)
    {
        fprintf (stderr, "Failed to load grammar module: %s\n",
                 g_module_error ());
        g_free (config_path);
        g_free (highlights_path);
        g_free (symbol_name);
        g_free (grammar_name);
        return FALSE;
    }

    if (!g_module_symbol (module, symbol_name, &symbol))
    {
        fprintf (stderr, "Symbol '%s' not found in module\n", symbol_name);
        g_module_close (module);
        g_free (config_path);
        g_free (highlights_path);
        g_free (symbol_name);
        g_free (grammar_name);
        return FALSE;
    }
    g_free (symbol_name);

    lang_func = (const TSLanguage * (*) (void)) symbol;
    lang = lang_func ();

    /* Create parser */
    parser = ts_parser_new ();
    if (!ts_parser_set_language (parser, lang))
    {
        fprintf (stderr, "Failed to set parser language\n");
        ts_parser_delete (parser);
        g_module_close (module);
        g_free (config_path);
        g_free (highlights_path);
        g_free (grammar_name);
        return FALSE;
    }

    /* Parse the buffer */
    input.payload = edit;
    input.read = ts_input_read_cb;
    input.encoding = TSInputEncodingUTF8;

    tree = ts_parser_parse (parser, NULL, input);
    if (tree == NULL)
    {
        fprintf (stderr, "Failed to parse file\n");
        ts_parser_delete (parser);
        g_module_close (module);
        g_free (config_path);
        g_free (highlights_path);
        g_free (grammar_name);
        return FALSE;
    }

    /* Load and compile highlight query */
    if (!g_file_get_contents (highlights_path, &query_src, &query_len, NULL))
    {
        fprintf (stderr, "Failed to read %s\n", highlights_path);
        ts_tree_delete (tree);
        ts_parser_delete (parser);
        g_module_close (module);
        g_free (config_path);
        g_free (highlights_path);
        g_free (grammar_name);
        return FALSE;
    }

    query = ts_query_new (lang, query_src, (uint32_t) query_len, &error_offset, &error_type);
    g_free (query_src);

    if (query == NULL)
    {
        fprintf (stderr, "Query compilation failed at offset %u (error type %d)\n",
                 error_offset, error_type);
        ts_tree_delete (tree);
        ts_parser_delete (parser);
        g_module_close (module);
        g_free (config_path);
        g_free (highlights_path);
        g_free (grammar_name);
        return FALSE;
    }

    /* Store in edit widget */
    edit->ts.parser = parser;
    edit->ts.tree = tree;
    edit->ts.highlight_query = query;
    edit->ts.highlights = g_array_new (FALSE, FALSE, sizeof (ts_highlight_entry_t));
    edit->ts.highlights_start = -1;
    edit->ts.highlights_end = -1;
    edit->ts.grammar_name = grammar_name;
    edit->ts.active = TRUE;
    edit->ts.need_reparse = FALSE;

    g_free (config_path);
    g_free (highlights_path);

    return TRUE;
}

#endif /* HAVE_TREE_SITTER */

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int
main (int argc, char *argv[])
{
    const char *source_path = NULL;
    const char *grammar_dir = NULL;
    const char *lib_dir = NULL;
    gboolean force_legacy = FALSE;
    gboolean force_ts = FALSE;
    int i;
    WEdit *edit;
    WRect rect;
    edit_arg_t arg;
    off_t byte_idx;
    const char *reset = "\033[0m";
    static WGroup owner;

    signal (SIGSEGV, crash_handler);
    signal (SIGABRT, crash_handler);

    setlocale (LC_ALL, "");

    for (i = 1; i < argc; i++)
    {
        if (strcmp (argv[i], "--ts") == 0)
            force_ts = TRUE;
        else if (strcmp (argv[i], "--legacy") == 0)
            force_legacy = TRUE;
        else if (strcmp (argv[i], "--grammar-dir") == 0 && i + 1 < argc)
            grammar_dir = argv[++i];
        else if (strcmp (argv[i], "--lib-dir") == 0 && i + 1 < argc)
            lib_dir = argv[++i];
        else
            source_path = argv[i];
    }

    if (source_path == NULL)
    {
        fprintf (stderr,
                 "Usage: %s [--ts|--legacy] [--grammar-dir DIR --lib-dir DIR] <source-file>\n",
                 argv[0]);
        return 1;
    }

    /* --grammar-dir requires --lib-dir and implies --ts */
    if (grammar_dir != NULL)
    {
        if (lib_dir == NULL)
        {
            fprintf (stderr, "--grammar-dir requires --lib-dir\n");
            return 1;
        }
        force_ts = TRUE;
        force_legacy = FALSE;
    }

    str_init_strings (NULL);
    mc_config_init_config_paths (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    mc_global.share_data_dir = g_strdup ("/usr/share/mc");
    mc_global.sysconfig_dir = g_strdup ("/usr/share/mc");

    tty_init_colors (FALSE, TRUE, 256);
    use_colors = TRUE;

    edit_options.syntax_highlighting = TRUE;
    edit_options.filesize_threshold = (char *) "64M";

    rect_init (&rect, 0, 0, 24, 80);
    memset (&arg, 0, sizeof (arg));
    arg.file_vpath = vfs_path_from_str (source_path);
    arg.line_number = 0;

#ifdef HAVE_TREE_SITTER
    /* When using --grammar-dir, disable normal TS init during edit_init
       so we can do our own direct init afterwards. */
    if (grammar_dir != NULL)
    {
        char *config_path;
        char *dir_basename;
        char *grammar_name;
        char *tmpdir;

        ts_init_disabled = TRUE;

        /* Set up color config from config.ini BEFORE edit_init,
           because ts_load_grammar_registry() runs once and caches. */
        dir_basename = g_path_get_basename (grammar_dir);
        grammar_name = g_strdup (dir_basename);
        g_free (dir_basename);

        config_path = g_build_filename (grammar_dir, "config.ini", (char *) NULL);
        tmpdir = create_temp_colors_ini (config_path, grammar_name);
        g_free (config_path);
        g_free (grammar_name);

        if (tmpdir != NULL)
        {
            g_free (mc_global.share_data_dir);
            mc_global.share_data_dir = tmpdir;
            /* Also redirect user data path so the existing
               ~/.local/share/mc/syntax-ts/colors.ini doesn't
               override our temp colors. */
            setenv ("MC_XDG_DATA_HOME", tmpdir, 1);
            /* Re-initialize paths with the new env */
            mc_config_init_config_paths (NULL);
        }
    }
#endif

    edit = edit_init (NULL, &rect, &arg);
    if (edit == NULL)
    {
        fprintf (stderr, "Failed to open file: %s\n", source_path);
        vfs_path_free (arg.file_vpath, TRUE);
        return 1;
    }

    memset (&owner, 0, sizeof (owner));
    group_add_widget (&owner, WIDGET (edit));

#ifdef HAVE_TREE_SITTER
    if (grammar_dir != NULL)
    {
        /* Direct grammar loading from specified directories */
        ts_init_disabled = FALSE;
        if (!ts_init_direct (edit, grammar_dir, lib_dir))
        {
            fprintf (stderr, "Failed to initialize tree-sitter from %s\n", grammar_dir);
            /* Fall through to show file without highlighting */
        }
    }
    else if (force_legacy && edit->ts.active)
    {
        ts_free (edit);
        edit->ts.active = FALSE;
        ts_init_disabled = TRUE;
        edit_load_syntax (edit, NULL, NULL);
        ts_init_disabled = FALSE;
    }
    else if (force_ts && !edit->ts.active)
        fprintf (stderr, "Tree-sitter failed to initialize for this file\n");
#else
    (void) force_ts;
    (void) force_legacy;
    (void) grammar_dir;
    (void) lib_dir;
#endif

    fprintf (stderr, "File: %s (%ld bytes)\n", source_path, (long) edit->buffer.size);
    fprintf (stderr, "Rules: %p\n", (void *) edit->rules);
    fprintf (stderr, "Mode: %s\n",
#ifdef HAVE_TREE_SITTER
             edit->ts.active ? "tree-sitter" : "legacy"
#else
             "legacy"
#endif
    );

    {
        int prev_color = -1;

        for (byte_idx = 0; byte_idx < edit->buffer.size; byte_idx++)
        {
            int color;
            char ch;
            const char *ansi;

            color = edit_get_syntax_color (edit, byte_idx);
            ch = (char) edit_buffer_get_byte (&edit->buffer, byte_idx);

            if (byte_idx < 12)
                fprintf (stderr, "  [%ld] '%c' color=%d\n",
                         (long) byte_idx, (ch >= 32 && ch < 127) ? ch : '.', color);

            if (color != prev_color)
            {
                if (prev_color >= 0 && prev_color < TTY_COLOR_MAP_OFFSET)
                    fputs (reset, stdout);

                if (color >= 0 && color < TTY_COLOR_MAP_OFFSET)
                {
                    ansi = pair_index_to_fg_ansi (color);
                    if (ansi[0] != '\0')
                        fputs (ansi, stdout);
                }
                prev_color = color;
            }

            putchar (ch);
        }

        if (prev_color >= 0 && prev_color < TTY_COLOR_MAP_OFFSET)
            fputs (reset, stdout);
    }

    edit_clean (edit);
    g_free (edit);
    vfs_path_free (arg.file_vpath, TRUE);
    vfs_shut ();

    return 0;
}
