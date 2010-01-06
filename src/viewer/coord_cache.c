/*
   Internal file viewer for the Midnight Commander
   Function for work with coordinate cache (ccache)

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009 Free Software Foundation, Inc.

   Written by: 1994, 1995, 1998 Miguel de Icaza
	       1994, 1995 Janne Kukonlehto
	       1995 Jakub Jelinek
	       1996 Joseph M. Hinkle
	       1997 Norbert Warmuth
	       1998 Pavel Machek
	       2004 Roland Illig <roland.illig@gmx.de>
	       2005 Roland Illig <roland.illig@gmx.de>
	       2009 Slava Zanko <slavazanko@google.com>
	       2009 Andrew Borodin <aborodin@vmail.ru>
	       2009 Ilia Maslakov <il.smind@gmail.com>

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
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

#include "../src/global.h"
#include "../lib/tty/tty.h"
#include "internal.h"

#define VIEW_COORD_CACHE_GRANUL	1024

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

/* Find and return the index of the last cache entry that is
 * smaller than ''coord'', according to the criterion ''sort_by''. */
static guint
mcview_ccache_find (mcview_t * view, const struct coord_cache_entry *cache,
                    const struct coord_cache_entry *coord, enum ccache_type sort_by)
{
    guint base, i, limit;

    limit = view->coord_cache->len;
    assert (limit != 0);

    base = 0;
    while (limit > 1) {
        i = base + limit / 2;
        if (mcview_coord_cache_entry_less (coord, &cache[i], sort_by, view->text_nroff_mode)) {
            /* continue the search in the lower half of the cache */
        } else {
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

gboolean
mcview_coord_cache_entry_less (const struct coord_cache_entry *a,
                               const struct coord_cache_entry *b, enum ccache_type crit,
                               gboolean nroff_mode)
{
    if (crit == CCACHE_OFFSET)
        return (a->cc_offset < b->cc_offset);

    if (a->cc_line < b->cc_line)
        return TRUE;

    if (a->cc_line == b->cc_line) {
        if (nroff_mode) {
            return (a->cc_nroff_column < b->cc_nroff_column);
        } else {
            return (a->cc_column < b->cc_column);
        }
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef MC_ENABLE_DEBUGGING_CODE

void
mcview_ccache_dump (mcview_t * view)
{
    FILE *f;
    off_t offset, line, column, nextline_offset, filesize;
    guint i;
    const struct coord_cache_entry *cache;

    assert (view->coord_cache != NULL);

    filesize = mcview_get_filesize (view);
    cache = &(g_array_index (view->coord_cache, struct coord_cache_entry, 0));

    f = fopen ("mcview-ccache.out", "w");
    if (f == NULL)
        return;
    (void) setvbuf (f, NULL, _IONBF, 0);

    /* cache entries */
    for (i = 0; i < view->coord_cache->len; i++) {
        (void) fprintf (f,
                        "entry %8u  "
                        "offset %8" OFFSETTYPE_PRId "  "
                        "line %8" OFFSETTYPE_PRId "  "
                        "column %8" OFFSETTYPE_PRId "  "
                        "nroff_column %8" OFFSETTYPE_PRId "\n",
                        (unsigned int) i, cache[i].cc_offset, cache[i].cc_line,
                        cache[i].cc_column, cache[i].cc_nroff_column);
    }
    (void) fprintf (f, "\n");

    /* offset -> line/column translation */
    for (offset = 0; offset < filesize; offset++) {
        mcview_offset_to_coord (view, &line, &column, offset);
        (void) fprintf (f,
                        "offset %8" OFFSETTYPE_PRId "  "
                        "line %8" OFFSETTYPE_PRId "  "
                        "column %8" OFFSETTYPE_PRId "\n", offset, line, column);
    }

    /* line/column -> offset translation */
    for (line = 0; TRUE; line++) {
        mcview_coord_to_offset (view, &nextline_offset, line + 1, 0);
        (void) fprintf (f, "nextline_offset %8" OFFSETTYPE_PRId "\n", nextline_offset);

        for (column = 0; TRUE; column++) {
            mcview_coord_to_offset (view, &offset, line, column);
            if (offset >= nextline_offset)
                break;

            (void) fprintf (f,
                            "line %8" OFFSETTYPE_PRId "  column %8" OFFSETTYPE_PRId "  offset %8"
                            OFFSETTYPE_PRId "\n", line, column, offset);
        }

        if (nextline_offset >= filesize - 1)
            break;
    }

    (void) fclose (f);
}
#endif

/* --------------------------------------------------------------------------------------------- */


/* Look up the missing components of ''coord'', which are given by
 * ''lookup_what''. The function returns the smallest value that
 * matches the existing components of ''coord''.
 */
void
mcview_ccache_lookup (mcview_t * view, struct coord_cache_entry *coord,
                      enum ccache_type lookup_what)
{
    guint i;
    struct coord_cache_entry *cache, current, next, entry;
    enum ccache_type sorter;
    off_t limit;
    enum {
        NROFF_START,
        NROFF_BACKSPACE,
        NROFF_CONTINUATION
    } nroff_state;

    if (!view->coord_cache) {
        view->coord_cache = g_array_new (FALSE, FALSE, sizeof (struct coord_cache_entry));
        current.cc_offset = 0;
        current.cc_line = 0;
        current.cc_column = 0;
        current.cc_nroff_column = 0;
        g_array_append_val (view->coord_cache, current);
    }

    sorter = (lookup_what == CCACHE_OFFSET) ? CCACHE_LINECOL : CCACHE_OFFSET;

    tty_enable_interrupt_key ();

  retry:
    /* find the two neighbor entries in the cache */
    cache = &(g_array_index (view->coord_cache, struct coord_cache_entry, 0));
    i = mcview_ccache_find (view, cache, coord, sorter);
    /* now i points to the lower neighbor in the cache */

    current = cache[i];
    if (i + 1 < view->coord_cache->len)
        limit = cache[i + 1].cc_offset;
    else
        limit = current.cc_offset + VIEW_COORD_CACHE_GRANUL;

    entry = current;
    nroff_state = NROFF_START;
    for (; current.cc_offset < limit; current = next) {
        int c, nextc;

        if (! mcview_get_byte (view, current.cc_offset, &c))
            break;

        if (!mcview_coord_cache_entry_less (&current, coord, sorter, view->text_nroff_mode)) {
            if (lookup_what == CCACHE_OFFSET && view->text_nroff_mode && nroff_state != NROFF_START) {
                /* don't break here */
            } else {
                break;
            }
        }

        /* Provide useful default values for ''next'' */
        next.cc_offset = current.cc_offset + 1;
        next.cc_line = current.cc_line;
        next.cc_column = current.cc_column + 1;
        next.cc_nroff_column = current.cc_nroff_column + 1;

        /* and override some of them as necessary. */
        if (c == '\r') {
            mcview_get_byte_indexed (view, current.cc_offset, 1, &nextc);

            /* Ignore '\r' if it is followed by '\r' or '\n'. If it is
             * followed by anything else, it is a Mac line ending and
             * produces a line break.
             */
            if (nextc == '\r' || nextc == '\n') {
                next.cc_column = current.cc_column;
                next.cc_nroff_column = current.cc_nroff_column;
            } else {
                next.cc_line = current.cc_line + 1;
                next.cc_column = 0;
                next.cc_nroff_column = 0;
            }

        } else if (nroff_state == NROFF_BACKSPACE) {
            next.cc_nroff_column = current.cc_nroff_column - 1;

        } else if (c == '\t') {
            next.cc_column = mcview_offset_rounddown (current.cc_column, 8) + 8;
            next.cc_nroff_column = mcview_offset_rounddown (current.cc_nroff_column, 8) + 8;

        } else if (c == '\n') {
            next.cc_line = current.cc_line + 1;
            next.cc_column = 0;
            next.cc_nroff_column = 0;

        } else {
            /* Use all default values from above */
        }

        switch (nroff_state) {
        case NROFF_START:
        case NROFF_CONTINUATION:
            if (mcview_is_nroff_sequence (view, current.cc_offset))
                nroff_state = NROFF_BACKSPACE;
            else
                nroff_state = NROFF_START;
            break;
        case NROFF_BACKSPACE:
            nroff_state = NROFF_CONTINUATION;
            break;
        }

        /* Cache entries must guarantee that for each i < j,
         * line[i] <= line[j] and column[i] < column[j]. In the case of
         * nroff sequences and '\r' characters, this is not guaranteed,
         * so we cannot save them. */
        if (nroff_state == NROFF_START && c != '\r')
            entry = next;
    }

    if (i + 1 == view->coord_cache->len && entry.cc_offset != cache[i].cc_offset) {
        g_array_append_val (view->coord_cache, entry);

        if (!tty_got_interrupt ())
            goto retry;
    }

    tty_disable_interrupt_key ();

    if (lookup_what == CCACHE_OFFSET) {
        coord->cc_offset = current.cc_offset;
    } else {
        coord->cc_line = current.cc_line;
        coord->cc_column = current.cc_column;
        coord->cc_nroff_column = current.cc_nroff_column;
    }
}

/* --------------------------------------------------------------------------------------------- */
