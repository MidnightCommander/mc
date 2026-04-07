/*
 * mc-syntax-dump.c - Dump MC syntax highlighting as ANSI-colored text.
 *
 * Uses MC's actual syntax engine internals for exact color output.
 *
 * Usage: mc-syntax-dump [--ts|--legacy] <source-file>
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
    /* no-op: skip SLang/ncurses initialization */
}

void __wrap_tty_color_try_alloc_lib_pair (tty_color_lib_pair_t *mc_color_pair)
{
    /* no-op: we don't need actual terminal color pairs */
    (void) mc_color_pair;
}

void __wrap_tty_color_deinit_lib (void)
{
    /* no-op */
}

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

/* Cache for resolved pair_index -> fg_index mappings */
#define CACHE_SIZE 4096
static struct { int pair_index; int fg_index; } pair_cache[CACHE_SIZE];
static int pair_cache_count = 0;

/*
 * Resolve a color pair index to its foreground ANSI code.
 * Uses tty_try_alloc_color_pair as a reverse lookup: since it returns
 * existing pairs for duplicate (fg, bg) combos, we try all fg*bg combos
 * until we find one matching the target pair_index.
 */
static const char *
pair_index_to_fg_ansi (int pair_idx)
{
    int i, fg, bg;

    if (pair_idx < 0)
        return "";

    /* Check cache first */
    for (i = 0; i < pair_cache_count; i++)
        if (pair_cache[i].pair_index == pair_idx)
            return ansi_codes[pair_cache[i].fg_index];

    /* Brute force: try all fg/bg combinations */
    for (fg = 0; fg < 16; fg++)
    {
        for (bg = 0; bg < 17; bg++)  /* 16 colors + NULL bg */
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
                /* Cache the result */
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
/* Main                                                               */
/* ------------------------------------------------------------------ */

int
main (int argc, char *argv[])
{
    const char *source_path = NULL;
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
        else
            source_path = argv[i];
    }

    if (source_path == NULL)
    {
        fprintf (stderr, "Usage: %s [--ts|--legacy] <source-file>\n", argv[0]);
        return 1;
    }

    str_init_strings (NULL);
    mc_config_init_config_paths (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    mc_global.share_data_dir = g_strdup ("/usr/share/mc");
    mc_global.sysconfig_dir = g_strdup ("/usr/share/mc");

    /* Initialize colors - tty_color_init_lib is wrapped to avoid needing a terminal */
    tty_init_colors (FALSE, TRUE, 256);
    /* tty_color_init_lib (wrapped) didn't set use_colors, set it manually */
    use_colors = TRUE;

    edit_options.syntax_highlighting = TRUE;
    edit_options.filesize_threshold = (char *) "64M";

    rect_init (&rect, 0, 0, 24, 80);
    memset (&arg, 0, sizeof (arg));
    arg.file_vpath = vfs_path_from_str (source_path);
    arg.line_number = 0;

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
    if (force_legacy && edit->ts.active)
    {
        ts_free (edit);
        edit->ts.active = FALSE;
        /* Reload with TS disabled (ts_init_for_file is wrapped to fail) */
        ts_init_disabled = TRUE;
        edit_load_syntax (edit, NULL, NULL);
        ts_init_disabled = FALSE;
    }
    else if (force_ts && !edit->ts.active)
        fprintf (stderr, "Tree-sitter failed to initialize for this file\n");
#else
    (void) force_ts;
    (void) force_legacy;
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

    /* Output with ANSI colors.
       Color pairs >= TTY_COLOR_MAP_OFFSET are role-based (e.g. EDITOR_NORMAL_COLOR)
       and represent the default color. Pair indices 0..N are direct pairs allocated
       by the syntax engine. */
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
