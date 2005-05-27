/* editor book mark handling

   Copyright (C) 1996, 1997 the Free Software Foundation

   Authors: 1996, 1997 Paul Sheer

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
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

#include "../src/global.h"

#include "edit.h"
#include "edit-widget.h"


/* note, if there is more than one bookmark on a line, then they are
   appended after each other and the last one is always the one found
   by book_mark_found() i.e. last in is the one seen */

static inline struct _book_mark *double_marks (WEdit * edit, struct _book_mark *p)
{
    (void) edit;

    if (p->next)
	while (p->next->line == p->line)
	    p = p->next;
    return p;
}

/* returns the first bookmark on or before this line */
struct _book_mark *book_mark_find (WEdit * edit, int line)
{
    struct _book_mark *p;
    if (!edit->book_mark) {
/* must have an imaginary top bookmark at line -1 to make things less complicated  */
	edit->book_mark = g_malloc0 (sizeof (struct _book_mark));
	edit->book_mark->line = -1;
	return edit->book_mark;
    }
    for (p = edit->book_mark; p; p = p->next) {
	if (p->line > line)
	    break;		/* gone past it going downward */
	if (p->line <= line) {
	    if (p->next) {
		if (p->next->line > line) {
		    edit->book_mark = p;
		    return double_marks (edit, p);
		}
	    } else {
		edit->book_mark = p;
		return double_marks (edit, p);
	    }
	}
    }
    for (p = edit->book_mark; p; p = p->prev) {
	if (p->next)
	    if (p->next->line <= line)
		break;		/* gone past it going upward */
	if (p->line <= line) {
	    if (p->next) {
		if (p->next->line > line) {
		    edit->book_mark = p;
		    return double_marks (edit, p);
		}
	    } else {
		edit->book_mark = p;
		return double_marks (edit, p);
	    }
	}
    }
    return 0;			/* can't get here */
}

/* returns true if a bookmark exists at this line of color c */
int book_mark_query_color (WEdit * edit, int line, int c)
{
    struct _book_mark *p;
    if (!edit->book_mark)
	return 0;
    for (p = book_mark_find (edit, line); p; p = p->prev) {
	if (p->line != line)
	    return 0;
	if (p->c == c)
	    return 1;
    }
    return 0;
}

/* insert a bookmark at this line */
void
book_mark_insert (WEdit *edit, int line, int c)
{
    struct _book_mark *p, *q;
    p = book_mark_find (edit, line);
#if 0
    if (p->line == line) {
	/* already exists, so just change the color */
	if (p->c != c) {
	    edit->force |= REDRAW_LINE;
	    p->c = c;
	}
	return;
    }
#endif
    edit->force |= REDRAW_LINE;
    /* create list entry */
    q = g_malloc0 (sizeof (struct _book_mark));
    q->line = line;
    q->c = c;
    q->next = p->next;
    /* insert into list */
    q->prev = p;
    if (p->next)
	p->next->prev = q;
    p->next = q;
}

/* remove a bookmark if there is one at this line matching this color - c of -1 clear all */
/* returns non-zero on not-found */
int book_mark_clear (WEdit * edit, int line, int c)
{
    struct _book_mark *p, *q;
    int r = 1;
    if (!edit->book_mark)
	return r;
    for (p = book_mark_find (edit, line); p; p = q) {
	q = p->prev;
	if (p->line == line && (p->c == c || c == -1)) {
	    r = 0;
	    edit->force |= REDRAW_LINE;
	    edit->book_mark = p->prev;
	    p->prev->next = p->next;
	    if (p->next)
		p->next->prev = p->prev;
	    g_free (p);
	    break;
	}
    }
/* if there is only our dummy book mark left, clear it for speed */
    if (edit->book_mark->line == -1 && !edit->book_mark->next) {
	g_free (edit->book_mark);
	edit->book_mark = 0;
    }
    return r;
}

/* clear all bookmarks matching this color, if c is -1 clears all */
void book_mark_flush (WEdit * edit, int c)
{
    struct _book_mark *p, *q;
    if (!edit->book_mark)
	return;
    edit->force |= REDRAW_PAGE;
    while (edit->book_mark->prev)
	edit->book_mark = edit->book_mark->prev;
    for (q = edit->book_mark->next; q; q = p) {
	p = q->next;
	if (q->c == c || c == -1) {
	    q->prev->next = q->next;
	    if (p)
		p->prev = q->prev;
	    g_free (q);
	}
    }
    if (!edit->book_mark->next) {
	g_free (edit->book_mark);
	edit->book_mark = 0;
    }
}

/* shift down bookmarks after this line */
void book_mark_inc (WEdit * edit, int line)
{
    if (edit->book_mark) {
	struct _book_mark *p;
	p = book_mark_find (edit, line);
	for (p = p->next; p; p = p->next) {
	    p->line++;
	}
    }
}

/* shift up bookmarks after this line */
void book_mark_dec (WEdit * edit, int line)
{
    if (edit->book_mark) {
	struct _book_mark *p;
	p = book_mark_find (edit, line);
	for (p = p->next; p; p = p->next) {
	    p->line--;
	}
    }
}
