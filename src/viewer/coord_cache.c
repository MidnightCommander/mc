/*
   Internal file viewer for the Midnight Commander
   Function for work with coordinate cache (ccache)

   Copyright (C) 1994-2024
   Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995, 1998
   Janne Kukonlehto, 1994, 1995
   Jakub Jelinek, 1995
   Joseph M. Hinkle, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Roland Illig <roland.illig@gmx.de>, 2004, 2005
   Slava Zanko <slavazanko@google.com>, 2009
   Andrew Borodin <aborodin@vmail.ru>, 2009-2022
   Ilia Maslakov <il.smind@gmail.com>, 2009

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

/*
   This cache provides you with a fast lookup to map file offsets into
   line/column pairs and vice versa. The interface to the mapping is
   provided by the functions mcview_coord_to_offset() and
   mcview_offset_to_coord().

   The cache is implemented as a simple sorted array holding entries
   that map some of the offsets to their line/column pair. Entries that
   are not cached themselves are interpolated (exactly) from their
   neighbor entries. The algorithm used for determining the line/column
   for a specific offset needs to be kept synchronized with the one used
   in display().
 */

#include <config.h>

#include <string.h>             /* memset() */
#ifdef MC_ENABLE_DEBUGGING_CODE
#include <inttypes.h>           /* uintmax_t */
#endif

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define VIEW_COORD_CACHE_GRANUL 1024
#define CACHE_CAPACITY_DELTA 64

#define coord_cache_index(c, i) ((coord_cache_entry_t *) g_ptr_array_index ((c), (i)))

/*** file scope type declarations ****************************************************************/

typedef gboolean (*cmp_func_t) (const coord_cache_entry_t * a, const coord_cache_entry_t * b);

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* insert new cache entry into the cache */
static inline void
mcview_ccache_add_entry (GPtrArray *cache, const coord_cache_entry_t *entry)
{
#if GLIB_CHECK_VERSION (2, 68, 0)
    g_ptr_array_add (cache, g_memdup2 (entry, sizeof (*entry)));
#else
    g_ptr_array_add (cache, g_memdup (entry, sizeof (*entry)));
#endif
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_coord_cache_entry_less_offset (const coord_cache_entry_t *a, const coord_cache_entry_t *b)
{
    return (a->cc_offset < b->cc_offset);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_coord_cache_entry_less_plain (const coord_cache_entry_t *a, const coord_cache_entry_t *b)
{
    if (a->cc_line < b->cc_line)
        return TRUE;

    if (a->cc_line == b->cc_line)
        return (a->cc_column < b->cc_column);

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_coord_cache_entry_less_nroff (const coord_cache_entry_t *a, const coord_cache_entry_t *b)
{
    if (a->cc_line < b->cc_line)
        return TRUE;

    if (a->cc_line == b->cc_line)
        return (a->cc_nroff_column < b->cc_nroff_column);

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/** Find and return the index of the last cache entry that is
 * smaller than ''coord'', according to the criterion ''sort_by''. */

static inline size_t
mcview_ccache_find (WView *view, const coord_cache_entry_t *coord, cmp_func_t cmp_func)
{
    size_t base = 0;
    size_t limit = view->coord_cache->len;

    g_assert (limit != 0);

    while (limit > 1)
    {
        size_t i;

        i = base + limit / 2;
        if (cmp_func (coord, coord_cache_index (view->coord_cache, i)))
        {
            /* continue the search in the lower half of the cache */
            ;
        }
        else
        {
            /* continue the search in the upper half of the cache */
            base = i;
        }

        limit = (limit + 1) / 2;
    }

    return base;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

#ifdef MC_ENABLE_DEBUGGING_CODE

void
mcview_ccache_dump (WView *view)
{
    FILE *f;
    off_t offset, line, column, nextline_offset, filesize;
    guint i;
    const GPtrArray *cache = view->coord_cache;

    g_assert (cache != NULL);

    filesize = mcview_get_filesize (view);

    f = fopen ("mcview-ccache.out", "w");
    if (f == NULL)
        return;

    (void) setvbuf (f, NULL, _IONBF, 0);

    /* cache entries */
    for (i = 0; i < cache->len; i++)
    {
        coord_cache_entry_t *e;

        e = coord_cache_index (cache, i);
        (void) fprintf (f,
                        "entry %8u  offset %8" PRIuMAX
                        "  line %8" PRIuMAX "  column %8" PRIuMAX
                        "  nroff_column %8" PRIuMAX "\n",
                        (unsigned int) i,
                        (uintmax_t) e->cc_offset, (uintmax_t) e->cc_line, (uintmax_t) e->cc_column,
                        (uintmax_t) e->cc_nroff_column);
    }
    (void) fprintf (f, "\n");

    /* offset -> line/column translation */
    for (offset = 0; offset < filesize; offset++)
    {
        mcview_offset_to_coord (view, &line, &column, offset);
        (void) fprintf (f,
                        "offset %8" PRIuMAX "  line %8" PRIuMAX "  column %8" PRIuMAX "\n",
                        (uintmax_t) offset, (uintmax_t) line, (uintmax_t) column);
    }

    /* line/column -> offset translation */
    for (line = 0; TRUE; line++)
    {
        mcview_coord_to_offset (view, &nextline_offset, line + 1, 0);
        (void) fprintf (f, "nextline_offset %8" PRIuMAX "\n", (uintmax_t) nextline_offset);

        for (column = 0; TRUE; column++)
        {
            mcview_coord_to_offset (view, &offset, line, column);
            if (offset >= nextline_offset)
                break;

            (void) fprintf (f,
                            "line %8" PRIuMAX "  column %8" PRIuMAX "  offset %8" PRIuMAX "\n",
                            (uintmax_t) line, (uintmax_t) column, (uintmax_t) offset);
        }

        if (nextline_offset >= filesize - 1)
            break;
    }

    (void) fclose (f);
}
#endif

/* --------------------------------------------------------------------------------------------- */
/** Look up the missing components of ''coord'', which are given by
 * ''lookup_what''. The function returns the smallest value that
 * matches the existing components of ''coord''.
 */

void
mcview_ccache_lookup (WView *view, coord_cache_entry_t *coord, enum ccache_type lookup_what)
{
    size_t i;
    GPtrArray *cache;
    coord_cache_entry_t current, next, entry;
    enum ccache_type sorter;
    off_t limit;
    cmp_func_t cmp_func;

    enum
    {
        NROFF_START,
        NROFF_BACKSPACE,
        NROFF_CONTINUATION
    } nroff_state;

    if (view->coord_cache == NULL)
        view->coord_cache = g_ptr_array_new_full (CACHE_CAPACITY_DELTA, g_free);

    cache = view->coord_cache;

    if (cache->len == 0)
    {
        memset (&current, 0, sizeof (current));
        mcview_ccache_add_entry (cache, &current);
    }

    sorter = (lookup_what == CCACHE_OFFSET) ? CCACHE_LINECOL : CCACHE_OFFSET;

    if (sorter == CCACHE_OFFSET)
        cmp_func = mcview_coord_cache_entry_less_offset;
    else if (view->mode_flags.nroff)
        cmp_func = mcview_coord_cache_entry_less_nroff;
    else
        cmp_func = mcview_coord_cache_entry_less_plain;

    tty_enable_interrupt_key ();

  retry:
    /* find the two neighbor entries in the cache */
    i = mcview_ccache_find (view, coord, cmp_func);
    /* now i points to the lower neighbor in the cache */

    current = *coord_cache_index (cache, i);
    if (i + 1 < view->coord_cache->len)
        limit = coord_cache_index (cache, i + 1)->cc_offset;
    else
        limit = current.cc_offset + VIEW_COORD_CACHE_GRANUL;

    entry = current;
    nroff_state = NROFF_START;
    for (; current.cc_offset < limit; current = next)
    {
        int c;

        if (!mcview_get_byte (view, current.cc_offset, &c))
            break;

        if (!cmp_func (&current, coord) &&
            (lookup_what != CCACHE_OFFSET || !view->mode_flags.nroff || nroff_state == NROFF_START))
            break;

        /* Provide useful default values for 'next' */
        next.cc_offset = current.cc_offset + 1;
        next.cc_line = current.cc_line;
        next.cc_column = current.cc_column + 1;
        next.cc_nroff_column = current.cc_nroff_column + 1;

        /* and override some of them as necessary. */
        if (c == '\r')
        {
            int nextc = -1;

            mcview_get_byte_indexed (view, current.cc_offset, 1, &nextc);

            /* Ignore '\r' if it is followed by '\r' or '\n'. If it is
             * followed by anything else, it is a Mac line ending and
             * produces a line break.
             */
            if (nextc == '\r' || nextc == '\n')
            {
                next.cc_column = current.cc_column;
                next.cc_nroff_column = current.cc_nroff_column;
            }
            else
            {
                next.cc_line = current.cc_line + 1;
                next.cc_column = 0;
                next.cc_nroff_column = 0;
            }
        }
        else if (nroff_state == NROFF_BACKSPACE)
            next.cc_nroff_column = current.cc_nroff_column - 1;
        else if (c == '\t')
        {
            next.cc_column = mcview_offset_rounddown (current.cc_column, 8) + 8;
            next.cc_nroff_column = mcview_offset_rounddown (current.cc_nroff_column, 8) + 8;
        }
        else if (c == '\n')
        {
            next.cc_line = current.cc_line + 1;
            next.cc_column = 0;
            next.cc_nroff_column = 0;
        }
        else
        {
            ;                   /* Use all default values from above */
        }

        switch (nroff_state)
        {
        case NROFF_START:
        case NROFF_CONTINUATION:
            nroff_state = mcview_is_nroff_sequence (view, current.cc_offset)
                ? NROFF_BACKSPACE : NROFF_START;
            break;
        case NROFF_BACKSPACE:
            nroff_state = NROFF_CONTINUATION;
            break;
        default:
            break;
        }

        /* Cache entries must guarantee that for each i < j,
         * line[i] <= line[j] and column[i] < column[j]. In the case of
         * nroff sequences and '\r' characters, this is not guaranteed,
         * so we cannot save them. */
        if (nroff_state == NROFF_START && c != '\r')
            entry = next;
    }

    if (i + 1 == cache->len && entry.cc_offset != coord_cache_index (cache, i)->cc_offset)
    {
        mcview_ccache_add_entry (cache, &entry);

        if (!tty_got_interrupt ())
            goto retry;
    }

    tty_disable_interrupt_key ();

    if (lookup_what == CCACHE_OFFSET)
        coord->cc_offset = current.cc_offset;
    else
    {
        coord->cc_line = current.cc_line;
        coord->cc_column = current.cc_column;
        coord->cc_nroff_column = current.cc_nroff_column;
    }
}

/* --------------------------------------------------------------------------------------------- */
