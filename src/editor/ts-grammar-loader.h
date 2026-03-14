/*
   Runtime loader for tree-sitter grammar shared modules.

   Used in shared mode (TREE_SITTER_SHARED): grammars are loaded on demand
   via g_module_open() from TS_GRAMMAR_LIBDIR (e.g. /usr/lib/mc/ts-grammars/).

   Provides ts_grammar_registry_lookup() with the same interface as the static
   registry in ts-grammars/ts-grammar-registry.h, so syntax.c works unchanged.
 */

#ifndef MC__TS_GRAMMAR_LOADER_H
#define MC__TS_GRAMMAR_LOADER_H

#include <gmodule.h>
#include <tree_sitter/api.h>

/*** Grammar name -> symbol name overrides for irregular naming ***/

static const struct
{
    const char *name;
    const char *symbol;
} ts_grammar_symbol_overrides[] = {
    { "cobol", "tree_sitter_COBOL" },
    { "commonlisp", "tree_sitter_commonlisp" },
    { NULL, NULL }
};

/*** Cached loaded modules ***/

typedef struct
{
    GModule *module;
    const TSLanguage *(*func) (void);
} ts_loaded_grammar_t;

static GHashTable *ts_loaded_grammars = NULL;

/* --------------------------------------------------------------------------------------------- */

/**
 * Look up a grammar by name: load the shared module on first use, cache the result.
 * Returns the TSLanguage* or NULL if the grammar module is not installed.
 */
static inline const TSLanguage *
ts_grammar_registry_lookup (const char *name)
{
    ts_loaded_grammar_t *entry;
    char *module_path;
    GModule *module;
    char *symbol_name;
    gpointer symbol;
    int i;

    if (ts_loaded_grammars == NULL)
        ts_loaded_grammars = g_hash_table_new (g_str_hash, g_str_equal);

    /* Check cache */
    entry = (ts_loaded_grammar_t *) g_hash_table_lookup (ts_loaded_grammars, name);
    if (entry != NULL)
        return entry->func != NULL ? entry->func () : NULL;

    /* Try to load the module.  Module files are named mc-ts-<name>.so.
       Check user-local path first (~/.local/lib/mc/ts-grammars/),
       then the system path (TS_GRAMMAR_LIBDIR). */
    {
        const char *home = g_get_home_dir ();

        if (home != NULL)
        {
            module_path = g_strdup_printf ("%s/.local/lib/mc/ts-grammars/mc-ts-%s.so", home, name);
            module = g_module_open (module_path, G_MODULE_BIND_LAZY);
            g_free (module_path);
        }
    }

    if (module == NULL)
    {
        module_path = g_strdup_printf ("%s/mc-ts-%s.so", TS_GRAMMAR_LIBDIR, name);
        module = g_module_open (module_path, G_MODULE_BIND_LAZY);
        g_free (module_path);
    }

    if (module == NULL)
    {
        /* Not found — cache a NULL entry so we don't retry */
        entry = g_new0 (ts_loaded_grammar_t, 1);
        g_hash_table_insert (ts_loaded_grammars, g_strdup (name), entry);
        return NULL;
    }

    /* Determine the symbol name */
    symbol_name = NULL;
    for (i = 0; ts_grammar_symbol_overrides[i].name != NULL; i++)
    {
        if (strcmp (ts_grammar_symbol_overrides[i].name, name) == 0)
        {
            symbol_name = g_strdup (ts_grammar_symbol_overrides[i].symbol);
            break;
        }
    }
    if (symbol_name == NULL)
        symbol_name = g_strdup_printf ("tree_sitter_%s", name);

    if (!g_module_symbol (module, symbol_name, &symbol))
    {
        g_free (symbol_name);
        g_module_close (module);
        entry = g_new0 (ts_loaded_grammar_t, 1);
        g_hash_table_insert (ts_loaded_grammars, g_strdup (name), entry);
        return NULL;
    }
    g_free (symbol_name);

    /* Cache the successful result */
    entry = g_new0 (ts_loaded_grammar_t, 1);
    entry->module = module;
    entry->func = (const TSLanguage * (*) (void)) symbol;
    g_hash_table_insert (ts_loaded_grammars, g_strdup (name), entry);

    return entry->func ();
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Clean up loaded grammar modules at shutdown.
 * We intentionally do NOT close the modules because TSLanguage pointers
 * may still be referenced by parsers. The OS reclaims them at exit.
 */
static inline void
ts_grammar_modules_cleanup (void)
{
    if (ts_loaded_grammars != NULL)
    {
        GHashTableIter iter;
        gpointer key, value;

        g_hash_table_iter_init (&iter, ts_loaded_grammars);
        while (g_hash_table_iter_next (&iter, &key, &value))
        {
            g_free (key);
            g_free (value);
        }
        g_hash_table_destroy (ts_loaded_grammars);
        ts_loaded_grammars = NULL;
    }
}

#endif /* MC__TS_GRAMMAR_LOADER_H */
