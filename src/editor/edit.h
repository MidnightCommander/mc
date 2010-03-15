/* edit.h - editor public API

   Copyright (C) 1996, 1997, 2009 Free Software Foundation, Inc.

   Authors: 1996, 1997 Paul Sheer
            2009 Andrew Borodin

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

/** \file edit.h
 *  \brief Header: editor public API
 *  \author Paul Sheer
 *  \date 1996, 1997
 *  \author Andrew Borodin
 *  \date 2009
 */

#ifndef MC_EDIT_H
#define MC_EDIT_H

#include "lib/global.h"	/* PATH_SEP_STR */
#include "lib/fileloc.h"

#define C_LINES_ELAPSED		1       /* first line elapsed region */
#define C_LINES_COLLAPSED	2       /* first line collapsed region */
#define C_LINES_MIDDLE_E	3       /* in middle collapsed */
#define C_LINES_MIDDLE_C	4       /* in middle elapsed */
#define C_LINES_LAST		5       /* last line elapsed region */
#define C_LINES_DEFAULT		0

/* Editor widget */
struct WEdit;
typedef struct WEdit WEdit;

extern int option_word_wrap_line_length;
extern int option_typewriter_wrap;
extern int option_auto_para_formatting;
extern int option_fill_tabs_with_spaces;
extern int option_return_does_auto_indent;
extern int option_backspace_through_tabs;
extern int option_fake_half_tabs;
extern int option_persistent_selections;
extern int option_cursor_beyond_eol;
extern int option_line_state;
extern int option_save_mode;
extern int option_save_position;
extern int option_syntax_highlighting;
extern char *option_backup_ext;

extern int edit_confirm_save;

extern int visible_tabs;
extern int visible_tws;

extern int simple_statusbar;
extern int option_check_nl_at_eof;
extern int show_right_margin;

/* used in main() */
void edit_stack_init (void);
void edit_stack_free (void);

int edit_file (const char *_file, int line);

const char *edit_get_file_name (const WEdit *edit);
int edit_get_curs_col (const WEdit *edit);
const char *edit_get_syntax_type (const WEdit *edit);

typedef struct collapsed_lines {
    int start_line;			/* first collpsed line number */
    int end_line;			/* end collapsed line number */
    int state;				/* 1 - collapsed; 0 - elapsed  */
} collapsed_lines;

GList * book_mark_collapse_insert (GList * list, const int start_line, const int end_line, int state);
int book_mark_collapse_query (GList *list, const int line, int *start_line, int *end_line, int *state);
int book_mark_get_collapse_state (GList *list, const int line, collapsed_lines *cl);
void book_mark_collapse_inc (GList * list, int line);
void book_mark_collapse_dec (GList * list, int line);

#endif				/* MC_EDIT_H */
