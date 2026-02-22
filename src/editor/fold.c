/*
   Editor code folding

   Copyright (C) 2025
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
 *  \brief Source: editor code folding
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"

#include "edit-impl.h"
#include "editwidget.h"

/* --------------------------------------------------------------------------------------------- */
/*** global variables ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** file scope macro definitions ****************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** file scope type declarations ****************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** forward declarations (file scope functions) *************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** file scope variables ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Find the fold that contains the given line.
 *
 * @param edit editor object
 * @param line line number to check
 * @return pointer to fold if line is the start line or within fold range, NULL otherwise
 */
edit_fold_t *
edit_fold_find (WEdit *edit, long line)
{
    edit_fold_t *p;

    if (edit->folds == NULL)
        return NULL;

    for (p = edit->folds; p != NULL; p = p->next)
    {
        if (line >= p->line_start && line <= p->line_start + p->line_count)
            return p;
        if (p->line_start > line)
            break;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Check if a line is hidden inside a fold (not the fold start line itself).
 *
 * @param edit editor object
 * @param line line number to check
 * @return TRUE if line is hidden
 */
gboolean
edit_fold_is_hidden (WEdit *edit, long line)
{
    edit_fold_t *f;

    f = edit_fold_find (edit, line);
    if (f == NULL)
        return FALSE;

    return (line > f->line_start && line <= f->line_start + f->line_count);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Create a new fold region.  The first visible line is line_start,
 * and line_count lines below it become hidden.
 *
 * If the new fold overlaps an existing fold, the existing fold is removed first.
 *
 * @param edit editor object
 * @param line_start first line of the fold
 * @param line_count number of lines to hide
 */
void
edit_fold_make (WEdit *edit, long line_start, long line_count)
{
    edit_fold_t *p, *q, *new_fold;

    if (line_count <= 0)
        return;

    /* remove any folds that overlap with the new region */
    p = edit->folds;
    while (p != NULL)
    {
        q = p->next;
        /* overlap: fold [p->line_start, p->line_start + p->line_count]
           intersects [line_start, line_start + line_count] */
        if (p->line_start + p->line_count >= line_start && p->line_start <= line_start + line_count)
        {
            /* remove p */
            if (p->prev != NULL)
                p->prev->next = p->next;
            else
                edit->folds = p->next;
            if (p->next != NULL)
                p->next->prev = p->prev;
            g_free (p);
        }
        p = q;
    }

    /* create and insert new fold in sorted order */
    new_fold = g_new0 (edit_fold_t, 1);
    new_fold->line_start = line_start;
    new_fold->line_count = line_count;

    if (edit->folds == NULL || edit->folds->line_start > line_start)
    {
        /* insert at head */
        new_fold->next = edit->folds;
        new_fold->prev = NULL;
        if (edit->folds != NULL)
            edit->folds->prev = new_fold;
        edit->folds = new_fold;
    }
    else
    {
        /* find insertion point */
        for (p = edit->folds; p->next != NULL && p->next->line_start <= line_start; p = p->next)
            ;
        new_fold->next = p->next;
        new_fold->prev = p;
        if (p->next != NULL)
            p->next->prev = new_fold;
        p->next = new_fold;
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Remove the fold that contains the given line.
 *
 * @param edit editor object
 * @param line line number
 * @return TRUE if a fold was removed
 */
gboolean
edit_fold_remove (WEdit *edit, long line)
{
    edit_fold_t *f;

    f = edit_fold_find (edit, line);
    if (f == NULL)
        return FALSE;

    if (f->prev != NULL)
        f->prev->next = f->next;
    else
        edit->folds = f->next;

    if (f->next != NULL)
        f->next->prev = f->prev;

    g_free (f);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Get the next visible line number after the given line.
 * If the line is a fold start, skip over its hidden lines.
 *
 * @param edit editor object
 * @param line current line number
 * @return next visible line number
 */
long
edit_fold_next_visible (WEdit *edit, long line)
{
    edit_fold_t *f;

    f = edit_fold_find (edit, line);
    if (f != NULL && line == f->line_start)
        return f->line_start + f->line_count + 1;

    /* if inside a fold (shouldn't normally happen for cursor), jump past it */
    if (f != NULL && line > f->line_start)
        return f->line_start + f->line_count + 1;

    return line + 1;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Get the previous visible line number before the given line.
 *
 * @param edit editor object
 * @param line current line number
 * @return previous visible line number
 */
long
edit_fold_prev_visible (WEdit *edit, long line)
{
    edit_fold_t *f;

    if (line <= 0)
        return 0;

    f = edit_fold_find (edit, line - 1);
    if (f != NULL && (line - 1) > f->line_start)
        return f->line_start;

    return line - 1;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Remove all folds.
 *
 * @param edit editor object
 */
void
edit_fold_flush (WEdit *edit)
{
    edit_fold_t *p, *q;

    for (p = edit->folds; p != NULL; p = q)
    {
        q = p->next;
        g_free (p);
    }
    edit->folds = NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Shift fold line numbers down by 1 for all folds after the given line.
 * Called when a new line is inserted.
 *
 * @param edit editor object
 * @param line line where insertion happened
 */
void
edit_fold_inc (WEdit *edit, long line)
{
    edit_fold_t *p;

    for (p = edit->folds; p != NULL; p = p->next)
    {
        if (p->line_start > line)
            p->line_start++;
        else if (line > p->line_start && line <= p->line_start + p->line_count)
            p->line_count++;
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Shift fold line numbers up by 1 for all folds after the given line.
 * Called when a line is deleted.
 *
 * @param edit editor object
 * @param line line where deletion happened
 */
void
edit_fold_dec (WEdit *edit, long line)
{
    edit_fold_t *p, *q;

    for (p = edit->folds; p != NULL; p = q)
    {
        q = p->next;
        if (p->line_start > line)
            p->line_start--;
        else if (line > p->line_start && line <= p->line_start + p->line_count)
        {
            p->line_count--;
            if (p->line_count <= 0)
            {
                /* fold collapsed — remove it */
                if (p->prev != NULL)
                    p->prev->next = p->next;
                else
                    edit->folds = p->next;
                if (p->next != NULL)
                    p->next->prev = p->prev;
                g_free (p);
            }
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Calculate the visual width of the fold indicator text "...} (N lines)".
 *
 * Uses str_term_width1 for correct i18n/UTF-8 handling.
 *
 * @param fold fold structure
 * @return visual column width of the fold indicator text
 */
int
edit_fold_indicator_width (const struct edit_fold_t *fold)
{
    (void) fold;
    /* fold indicator is "...}" — always 4 columns */
    return 4;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Toggle fold at the current cursor line.
 *
 * If the cursor is on a fold start, unfold it.
 * If a selection is active, fold the selected lines.
 * Otherwise, find an opening bracket on the line, match it, and fold that range.
 *
 * @param edit editor object
 */
void
edit_fold_toggle (WEdit *edit)
{
    long line;
    edit_fold_t *fold;

    line = edit->buffer.curs_line;
    fold = edit_fold_find (edit, line);

    if (fold != NULL && line == fold->line_start)
    {
        /* existing fold — unfold */
        edit_fold_remove (edit, fold->line_start);
    }
    else
    {
        off_t start_mark, end_mark;

        if (eval_marks (edit, &start_mark, &end_mark))
        {
            /* selection active — fold selected lines */
            long line1, line2;

            line1 = edit_buffer_count_lines (&edit->buffer, 0, start_mark);
            line2 = edit_buffer_count_lines (&edit->buffer, 0, end_mark);
            if (line2 > line1)
            {
                edit_fold_make (edit, line1, line2 - line1);
                edit_mark_cmd (edit, TRUE);
            }
        }
        else
        {
            /* no selection — find { on this line, match } */
            off_t bol, eol, pos;

            bol = edit_buffer_get_current_bol (&edit->buffer);
            eol = edit_buffer_get_current_eol (&edit->buffer);

            for (pos = bol; pos < eol; pos++)
            {
                if (strchr ("{[(", edit_buffer_get_byte (&edit->buffer, pos)) != NULL)
                {
                    off_t match;

                    edit_cursor_move (edit, pos - edit->buffer.curs1);
                    match = edit_get_bracket (edit, 0, 0);
                    if (match >= 0)
                    {
                        long line2;

                        line2 = edit_buffer_count_lines (&edit->buffer, 0, match);
                        if (line2 > line)
                        {
                            edit_fold_make (edit, line, line2 - line);
                            break;
                        }
                    }
                }
            }
        }
    }

    edit->mark1 = edit->mark2 = edit->buffer.curs1;
    edit->force |= REDRAW_PAGE;
}

/* --------------------------------------------------------------------------------------------- */
