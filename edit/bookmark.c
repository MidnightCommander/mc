/* editor book mark handling

   Copyright (C) 2001, 2002, 2003, 2005, 2007 Free Software Foundation, Inc.

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


/*
 *****************************************************************************
 * collapsed lines algorithm
 *
 *****************************************************************************
 */

/* returns the first collapsed region on or before this line */
GList *
book_mark_collapse_find (GList * list, int line)
{
    GList *cl, *l;
    struct collapsed_lines *collapsed;
    l = list;
    if (!l)
        return NULL;
    l = g_list_first (list);
    cl = l;
    while (cl) {
        collapsed = (struct collapsed_lines *) cl->data;
        if ( collapsed->start_line <= line && line <= collapsed->end_line )
            return cl;
        cl = g_list_next (cl);
    }
    return NULL;
}

/* insert a collapsed at this line */
GList *
book_mark_collapse_insert (GList *list, const int start_line, const int end_line,
                           int state)
{
    struct collapsed_lines *p, *q;
    p = g_new0 (struct collapsed_lines, 1);
    p->start_line = start_line;
    p->end_line = end_line;
    p->state = state;

    GList *link, *newlink, *tmp;
    /*
     * Go to the last position and traverse the list backwards
     * starting from the second last entry to make sure that we
     * are not removing the current link.
     */
    list = g_list_append (list, p);
    list = g_list_last (list);
    link = g_list_previous (list);

    int sl = 0;
    int el = 0;

    while ( link ) {
        newlink = g_list_previous (link);
        q = (struct collapsed_lines *) link->data;
        sl = q->start_line;
        el = q->end_line;
        if (((sl == start_line) || (el == end_line) ||
           (sl == end_line) || (el == start_line)) ||
           ((sl < start_line) && (el > start_line) && (el < end_line)) ||
           ((sl > start_line) && (sl < end_line) && (el > end_line))) {
            g_free (link->data);
            tmp = g_list_remove_link (list, link);
            g_list_free_1 (link);
        }
        link = newlink;
    }
    return list;
}


/* returns true if a collapsed exists at this line
 * return start_line, end_line if found region
 *
 */
int book_mark_collapse_query (GList * list, const int line,
                              int *start_line,
                              int *end_line,
                              int *state)
{
    GList *cl;
    struct collapsed_lines *p;

    *start_line = 0;
    *end_line = 0;
    *state = 0;

    cl = book_mark_collapse_find (list, line);
    if ( cl ){
        p = (struct collapsed_lines *) cl->data;
        *start_line = p->start_line;
        *end_line = p->end_line;
        *state = p->state;
        return 1;
    }
    return 0;
}

int book_mark_get_collapse_state (GList * list, const int line)
{
    int start_line;
    int end_line;
    int state;
    int c = 0;

    c = book_mark_collapse_query (list, line, &start_line, &end_line, &state);
//    mc_log("start_line: %ld, end_line: %ld, line: %ld [%i]\n", start_line, end_line, line, c);
    if ( c == 0 )
        return C_LINES_DEFAULT;
    if ( line == start_line ) {
        if ( state )
            return C_LINES_COLLAPSED;
        else
            return C_LINES_ELAPSED;
    }
    if ( line > start_line && line< end_line ) {
        if ( state )
            return C_LINES_MIDDLE_C;
        else
            return C_LINES_MIDDLE_E;
    }
    if ( line == end_line ) {
        if ( state )
            return C_LINES_MIDDLE_C;
        else
            return C_LINES_LAST;
    }
}


/* shift down bookmarks after this line */
void book_mark_collapse_inc (GList * list, int line)
{
    GList *cl, *l;
    struct collapsed_lines *collapsed;
    l = list;
    if (!l)
        return;
    l = g_list_first (list);
    cl = l;
    while (cl) {
        collapsed = (struct collapsed_lines *) cl->data;
        if ( collapsed->start_line >= line ) {
            collapsed->start_line++;
            collapsed->end_line++;
        } else if ( collapsed->end_line >= line ){
            collapsed->end_line++;
        }
    }
}

