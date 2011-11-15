/*
   Editor book mark handling

   Copyright (C) 2001, 2002, 2003, 2005, 2007, 2011
   The Free Software Foundation, Inc.

   Written by:
   Paul Sheer, 1996, 1997

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
 *  \brief Source: editor book mark handling
 *  \author Paul Sheer
 *  \date 1996, 1997
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/util.h"           /* MAX_SAVED_BOOKMARKS */

#include "editwidget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** note, if there is more than one bookmark on a line, then they are
   appended after each other and the last one is always the one found
   by book_mark_found() i.e. last in is the one seen */

static struct _book_mark *
double_marks (WEdit * edit, struct _book_mark *p)
{
    (void) edit;

    if (p->next != NULL)
        while (p->next->line == p->line)
            p = p->next;
    return p;
}

/* --------------------------------------------------------------------------------------------- */
/** returns the first bookmark on or before this line */

struct _book_mark *
book_mark_find (WEdit * edit, int line)
{
    struct _book_mark *p;

    if (edit->book_mark == NULL)
    {
        /* must have an imaginary top bookmark at line -1 to make things less complicated  */
        edit->book_mark = g_malloc0 (sizeof (struct _book_mark));
        edit->book_mark->line = -1;
        return edit->book_mark;
    }

    for (p = edit->book_mark; p != NULL; p = p->next)
    {
        if (p->line > line)
            break;              /* gone past it going downward */

        if (p->next != NULL)
        {
            if (p->next->line > line)
            {
                edit->book_mark = p;
                return double_marks (edit, p);
            }
        }
        else
        {
            edit->book_mark = p;
            return double_marks (edit, p);
        }
    }

    for (p = edit->book_mark; p != NULL; p = p->prev)
    {
        if (p->next != NULL && p->next->line <= line)
            break;              /* gone past it going upward */

        if (p->line <= line)
        {
            if (p->next != NULL)
            {
                if (p->next->line > line)
                {
                    edit->book_mark = p;
                    return double_marks (edit, p);
                }
            }
            else
            {
                edit->book_mark = p;
                return double_marks (edit, p);
            }
        }
    }

    return NULL;                /* can't get here */
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** returns true if a bookmark exists at this line of color c */

int
book_mark_query_color (WEdit * edit, int line, int c)
{
    struct _book_mark *p;

    if (edit->book_mark == NULL)
        return 0;

    for (p = book_mark_find (edit, line); p != NULL; p = p->prev)
    {
        if (p->line != line)
            return 0;
        if (p->c == c)
            return 1;
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/** insert a bookmark at this line */

void
book_mark_insert (WEdit * edit, size_t line, int c)
{
    struct _book_mark *p, *q;

    p = book_mark_find (edit, line);
#if 0
    if (p->line == line)
    {
        /* already exists, so just change the color */
        if (p->c != c)
        {
            p->c = c;
            edit->force |= REDRAW_LINE;
        }
        return;
    }
#endif
    edit->force |= REDRAW_LINE;
    /* create list entry */
    q = g_malloc0 (sizeof (struct _book_mark));
    q->line = (int) line;
    q->c = c;
    q->next = p->next;
    /* insert into list */
    q->prev = p;
    if (p->next != NULL)
        p->next->prev = q;
    p->next = q;
}

/* --------------------------------------------------------------------------------------------- */
/** remove a bookmark if there is one at this line matching this color - c of -1 clear all
 * @returns non-zero on not-found
 */

int
book_mark_clear (WEdit * edit, int line, int c)
{
    struct _book_mark *p, *q;
    int r = 1;

    if (edit->book_mark == NULL)
        return r;

    for (p = book_mark_find (edit, line); p != NULL; p = q)
    {
        q = p->prev;
        if (p->line == line && (p->c == c || c == -1))
        {
            r = 0;
            edit->book_mark = p->prev;
            p->prev->next = p->next;
            if (p->next != NULL)
                p->next->prev = p->prev;
            g_free (p);
            edit->force |= REDRAW_LINE;
            break;
        }
    }
    /* if there is only our dummy book mark left, clear it for speed */
    if (edit->book_mark->line == -1 && !edit->book_mark->next)
    {
        g_free (edit->book_mark);
        edit->book_mark = NULL;
    }
    return r;
}

/* --------------------------------------------------------------------------------------------- */
/** clear all bookmarks matching this color, if c is -1 clears all */

void
book_mark_flush (WEdit * edit, int c)
{
    struct _book_mark *p, *q;

    if (edit->book_mark == NULL)
        return;

    while (edit->book_mark->prev != NULL)
        edit->book_mark = edit->book_mark->prev;

    for (q = edit->book_mark->next; q != NULL; q = p)
    {
        p = q->next;
        if (q->c == c || c == -1)
        {
            q->prev->next = q->next;
            if (p != NULL)
                p->prev = q->prev;
            g_free (q);
        }
    }
    if (edit->book_mark->next == NULL)
    {
        g_free (edit->book_mark);
        edit->book_mark = NULL;
    }

    edit->force |= REDRAW_PAGE;
}

/* --------------------------------------------------------------------------------------------- */
/** shift down bookmarks after this line */

void
book_mark_inc (WEdit * edit, int line)
{
    if (edit->book_mark)
    {
        struct _book_mark *p;
        p = book_mark_find (edit, line);
        for (p = p->next; p != NULL; p = p->next)
            p->line++;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** shift up bookmarks after this line */

void
book_mark_dec (WEdit * edit, int line)
{
    if (edit->book_mark != NULL)
    {
        struct _book_mark *p;
        p = book_mark_find (edit, line);
        for (p = p->next; p != NULL; p = p->next)
            p->line--;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** prepare line positions of bookmarks to be saved to file */

void
book_mark_serialize (WEdit * edit, int color)
{
    struct _book_mark *p;

    if (edit->serialized_bookmarks != NULL)
        g_array_set_size (edit->serialized_bookmarks, 0);

    if (edit->book_mark != NULL)
    {
        if (edit->serialized_bookmarks == NULL)
            edit->serialized_bookmarks = g_array_sized_new (FALSE, FALSE, sizeof (size_t),
                                                            MAX_SAVED_BOOKMARKS);

        for (p = book_mark_find (edit, 0); p != NULL; p = p->next)
            if (p->c == color && p->line >= 0)
                g_array_append_val (edit->serialized_bookmarks, p->line);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** restore bookmarks from saved line positions */

void
book_mark_restore (WEdit * edit, int color)
{
    if (edit->serialized_bookmarks != NULL)
    {
        size_t i;

        for (i = 0; i < edit->serialized_bookmarks->len; i++)
            book_mark_insert (edit, g_array_index (edit->serialized_bookmarks, size_t, i), color);
    }
}

/* --------------------------------------------------------------------------------------------- */
