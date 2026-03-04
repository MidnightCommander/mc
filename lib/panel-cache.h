/** \file panel-cache.h
 *  \brief Header: shared directory listing cache for panel plugins
 *
 *  Provides a reusable, TTL-aware directory listing cache that can be
 *  embedded into any network panel plugin (FTP, SFTP, Samba, etc.).
 *  Header-only: all functions are static and defined inline.
 */

#ifndef MC__PANEL_CACHE_H
#define MC__PANEL_CACHE_H

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <glib.h>

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/** A single directory entry (file or subdirectory). */
typedef struct
{
    char *name;
    struct stat st;
    gboolean is_dir;
} mc_pp_dir_entry_t;

/** Cached listing for one directory path. */
typedef struct
{
    GPtrArray *entries; /* mc_pp_dir_entry_t* */
    time_t fetch_time;  /* when the listing was fetched */
} mc_pp_dir_cache_entry_t;

/** Directory cache instance — embed in plugin data. */
typedef struct
{
    GHashTable *table; /* path (char*) -> mc_pp_dir_cache_entry_t* */
    int ttl;           /* seconds; 0 = cache disabled */
} mc_pp_dir_cache_t;

/*** static functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

static void
mc_pp_dir_entry_free (gpointer p)
{
    mc_pp_dir_entry_t *e = (mc_pp_dir_entry_t *) p;

    g_free (e->name);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_dir_entry_t *
mc_pp_dir_entry_clone (const mc_pp_dir_entry_t *src)
{
    mc_pp_dir_entry_t *dst;

    dst = g_new0 (mc_pp_dir_entry_t, 1);
    dst->name = g_strdup (src->name);
    dst->st = src->st;
    dst->is_dir = src->is_dir;
    return dst;
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
mc_pp_dir_entries_clone (const GPtrArray *src)
{
    GPtrArray *dst;
    guint i;

    dst = g_ptr_array_new_with_free_func (mc_pp_dir_entry_free);
    for (i = 0; i < src->len; i++)
    {
        const mc_pp_dir_entry_t *e = (const mc_pp_dir_entry_t *) g_ptr_array_index (src, i);
        g_ptr_array_add (dst, mc_pp_dir_entry_clone (e));
    }
    return dst;
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_pp_dir_cache_entry_free (gpointer p)
{
    mc_pp_dir_cache_entry_t *c = (mc_pp_dir_cache_entry_t *) p;

    if (c->entries != NULL)
        g_ptr_array_free (c->entries, TRUE);
    g_free (c);
}

/* --------------------------------------------------------------------------------------------- */

/** Initialize an embedded cache structure. */
static void
mc_pp_dir_cache_init (mc_pp_dir_cache_t *cache, int ttl)
{
    cache->table =
        g_hash_table_new_full (g_str_hash, g_str_equal, g_free, mc_pp_dir_cache_entry_free);
    cache->ttl = ttl;
}

/* --------------------------------------------------------------------------------------------- */

/** Destroy all resources held by the cache. */
static void
mc_pp_dir_cache_destroy (mc_pp_dir_cache_t *cache)
{
    if (cache->table != NULL)
    {
        g_hash_table_destroy (cache->table);
        cache->table = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

/** Remove all cached entries. */
static void
mc_pp_dir_cache_clear (mc_pp_dir_cache_t *cache)
{
    if (cache->table != NULL)
        g_hash_table_remove_all (cache->table);
}

/* --------------------------------------------------------------------------------------------- */

/** Invalidate (remove) the cached listing for a single path. */
static gboolean
mc_pp_dir_cache_invalidate (mc_pp_dir_cache_t *cache, const char *path)
{
    if (cache->table == NULL || path == NULL)
        return FALSE;
    return g_hash_table_remove (cache->table, path);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Look up a cached listing for \a path.
 * Returns a cloned GPtrArray of mc_pp_dir_entry_t* on hit, or NULL on miss / expired.
 * The caller owns the returned array.
 */
static GPtrArray *
mc_pp_dir_cache_lookup (mc_pp_dir_cache_t *cache, const char *path)
{
    mc_pp_dir_cache_entry_t *cached;

    if (cache->table == NULL || cache->ttl <= 0 || path == NULL)
        return NULL;

    cached = (mc_pp_dir_cache_entry_t *) g_hash_table_lookup (cache->table, path);
    if (cached == NULL)
        return NULL;

    if (time (NULL) - cached->fetch_time > cache->ttl)
    {
        g_hash_table_remove (cache->table, path);
        return NULL;
    }

    return mc_pp_dir_entries_clone (cached->entries);
}

/* --------------------------------------------------------------------------------------------- */

/** Store a cloned copy of \a entries in the cache under \a path. */
static void
mc_pp_dir_cache_store (mc_pp_dir_cache_t *cache, const char *path, const GPtrArray *entries)
{
    mc_pp_dir_cache_entry_t *cached;

    if (cache->table == NULL || cache->ttl <= 0 || path == NULL)
        return;

    cached = g_new0 (mc_pp_dir_cache_entry_t, 1);
    cached->entries = mc_pp_dir_entries_clone (entries);
    cached->fetch_time = time (NULL);

    g_hash_table_replace (cache->table, g_strdup (path), cached);
}

/* --------------------------------------------------------------------------------------------- */

#endif /* MC__PANEL_CACHE_H */
