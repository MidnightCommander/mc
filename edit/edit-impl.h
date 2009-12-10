/* edit.h - editor private API

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

/** \file edit-impl.h
 *  \brief Header: editor low level data handling and cursor fundamentals
 *  \author Paul Sheer
 *  \date 1996, 1997
 */

#ifndef MC_EDIT_IMPL_H
#define MC_EDIT_IMPL_H

#include <stdio.h>

#include "../src/dialog.h"	/* cb_ret_t */
#include "../src/keybind.h"	/* global_keymap_t */

#include "../edit/edit.h"

#define SEARCH_DIALOG_OPTION_NO_SCANF	(1 << 0)
#define SEARCH_DIALOG_OPTION_NO_REGEX	(1 << 1)
#define SEARCH_DIALOG_OPTION_NO_CASE	(1 << 2)
#define SEARCH_DIALOG_OPTION_BACKWARDS	(1 << 3)
#define SEARCH_DIALOG_OPTION_BOOKMARK	(1 << 4)

#define EDIT_KEY_EMULATION_NORMAL 0
#define EDIT_KEY_EMULATION_EMACS  1
#define EDIT_KEY_EMULATION_USER   2

#define REDRAW_LINE          (1 << 0)
#define REDRAW_LINE_ABOVE    (1 << 1)
#define REDRAW_LINE_BELOW    (1 << 2)
#define REDRAW_AFTER_CURSOR  (1 << 3)
#define REDRAW_BEFORE_CURSOR (1 << 4)
#define REDRAW_PAGE          (1 << 5)
#define REDRAW_IN_BOUNDS     (1 << 6)
#define REDRAW_CHAR_ONLY     (1 << 7)
#define REDRAW_COMPLETELY    (1 << 8)

#define EDIT_TEXT_HORIZONTAL_OFFSET	0
#define EDIT_TEXT_VERTICAL_OFFSET	1

#define EDIT_RIGHT_EXTREME option_edit_right_extreme
#define EDIT_LEFT_EXTREME option_edit_left_extreme
#define EDIT_TOP_EXTREME option_edit_top_extreme
#define EDIT_BOTTOM_EXTREME option_edit_bottom_extreme

/*
 * The editor keeps data in two arrays of buffers.
 * All buffers have the same size, which must be a power of 2.
 */

/* Configurable: log2 of the buffer size in bytes */
#ifndef S_EDIT_BUF_SIZE
#define S_EDIT_BUF_SIZE 16
#endif

/* Size of the buffer */
#define EDIT_BUF_SIZE (((off_t) 1) << S_EDIT_BUF_SIZE)

/* Buffer mask (used to find cursor position relative to the buffer) */
#define M_EDIT_BUF_SIZE (EDIT_BUF_SIZE - 1)

/*
 * Configurable: Maximal allowed number of buffers in each buffer array.
 * This number can be increased for systems with enough physical memory.
 */
#ifndef MAXBUFF
#define MAXBUFF 1024
#endif

/* Maximal length of file that can be opened */
#define SIZE_LIMIT (EDIT_BUF_SIZE * (MAXBUFF - 2))

/* Initial size of the undo stack, in bytes */
#define START_STACK_SIZE 32

/* Some codes that may be pushed onto or returned from the undo stack */
#define CURS_LEFT	601
#define CURS_RIGHT	602
#define DELCHAR		603
#define BACKSPACE	604
#define STACK_BOTTOM	605
#define CURS_LEFT_LOTS	606
#define CURS_RIGHT_LOTS	607
#define COLUMN_ON	608
#define COLUMN_OFF	609
#define MARK_1		1000
#define MARK_2		700000000
#define KEY_PRESS	1400000000

/* Tabs spaces: (sofar only HALF_TAB_SIZE is used: */
#define TAB_SIZE		option_tab_spacing
#define HALF_TAB_SIZE		((int) option_tab_spacing / 2)

/* max count stack files */
#define MAX_HISTORY_MOVETO     50
#define LINE_STATE_WIDTH	8

typedef struct edit_stack_type {
    long line;
    char *filename;
} edit_stack_type;

struct macro {
    unsigned long command;
    int ch;
};

/* type for file which is currently being edited */
typedef enum {
    EDIT_FILE_COMMON	= 0,
    EDIT_FILE_SYNTAX	= 1,
    EDIT_FILE_MENU	= 2
} edit_current_file_t;

/* line breaks */
typedef enum {
    LB_ASIS = 0,
    LB_UNIX,
    LB_WIN,
    LB_MAC
} LineBreaks;
#define LB_NAMES (LB_MAC + 1)

struct Widget;
struct WMenuBar;

extern const char VERTICAL_MAGIC[5];
/* if enable_show_tabs_tws ==1 then use visible_tab visible_tws */
extern int enable_show_tabs_tws;
int edit_drop_hotkey_menu (WEdit *e, int key);
void edit_menu_cmd (WEdit *e);
void edit_init_menu (struct WMenuBar *menubar);
void menu_save_mode_cmd (void);
int edit_translate_key (WEdit *edit, long x_key, int *cmd, int *ch);
int edit_get_byte (WEdit * edit, long byte_index);
char *edit_get_byte_ptr (WEdit * edit, long byte_index);
char *edit_get_buf_ptr (WEdit * edit, long byte_index);
int edit_get_utf (WEdit * edit, long byte_index, int *char_width);
int edit_get_prev_utf (WEdit * edit, long byte_index, int *char_width);
int edit_count_lines (WEdit * edit, long current, int upto);
long edit_move_forward (WEdit * edit, long current, int lines, long upto);
long edit_move_forward3 (WEdit * edit, long current, int cols, long upto);
long edit_move_backward (WEdit * edit, long current, int lines);
void edit_scroll_screen_over_cursor (WEdit * edit);
void edit_render_keypress (WEdit * edit);
void edit_scroll_upward (WEdit * edit, unsigned long i);
void edit_scroll_downward (WEdit * edit, int i);
void edit_scroll_right (WEdit * edit, int i);
void edit_scroll_left (WEdit * edit, int i);
void edit_move_up (WEdit * edit, unsigned long i, int scroll);
void edit_move_down (WEdit * edit, unsigned long i, int scroll);
void edit_move_to_prev_col (WEdit *edit, long p);
int edit_get_col (WEdit * edit);
long edit_bol (WEdit * edit, long current);
long edit_eol (WEdit * edit, long current);
void edit_update_curs_row (WEdit * edit);
void edit_update_curs_col (WEdit * edit);
void edit_find_bracket (WEdit * edit);
int edit_reload_line (WEdit *edit, const char *filename, long line);

void edit_block_copy_cmd (WEdit * edit);
void edit_block_move_cmd (WEdit * edit);
int edit_block_delete_cmd (WEdit * edit);
void edit_delete_line (WEdit * edit);
void insert_spaces_tab (WEdit * edit, int half);

int edit_delete (WEdit * edit, const int byte_delete);
void edit_insert (WEdit * edit, int c);
void edit_cursor_move (WEdit * edit, long increment);
void edit_move_block_to_right (WEdit * edit);
void edit_move_block_to_left (WEdit * edit);
void edit_push_action (WEdit * edit, long c, ...);
void edit_push_key_press (WEdit * edit);
void edit_insert_ahead (WEdit * edit, int c);
long edit_write_stream (WEdit * edit, FILE * f);
char *edit_get_write_filter (const char *writename, const char *filename);
int edit_save_confirm_cmd (WEdit * edit);
int edit_save_as_cmd (WEdit * edit);
WEdit *edit_init (WEdit *edit, int lines, int columns,
		  const char *filename, long line);
int edit_clean (WEdit * edit);
int edit_ok_to_exit (WEdit *edit);
int edit_renew (WEdit * edit);
int edit_new_cmd (WEdit * edit);
int edit_reload (WEdit *edit, const char *filename);
int edit_load_cmd (WEdit * edit, edit_current_file_t what);
void edit_mark_cmd (WEdit * edit, int unmark);
void edit_set_markers (WEdit * edit, long m1, long m2, int c1, int c2);
void edit_push_markers (WEdit * edit);
void edit_replace_cmd (WEdit * edit, int again);
void edit_search_cmd (WEdit * edit, int again);
void edit_complete_word_cmd (WEdit * edit);
void edit_get_match_keyword_cmd (WEdit *edit);
int edit_save_block (WEdit * edit, const char *filename, long start, long finish);
int edit_save_block_cmd (WEdit * edit);
int edit_insert_file_cmd (WEdit * edit);
void edit_insert_column_of_text (WEdit * edit, unsigned char *data, int size, int width);
int edit_insert_column_of_text_from_file (WEdit * edit, int file);
int edit_insert_file (WEdit * edit, const char *filename);
int edit_load_back_cmd (WEdit * edit);
int edit_load_forward_cmd (WEdit * edit);
void edit_block_process_cmd (WEdit * edit, const char *shell_cmd, int block);
void edit_refresh_cmd (WEdit * edit);
void edit_date_cmd (WEdit * edit);
void edit_goto_cmd (WEdit * edit);
int eval_marks (WEdit * edit, long *start_mark, long *end_mark);
void edit_status (WEdit * edit);
void edit_execute_key_command (WEdit *edit, unsigned long command,
			       int char_for_insertion);
void edit_update_screen (WEdit * edit);
int edit_print_string (WEdit * e, const char *s);
void edit_move_to_line (WEdit * e, long line);
void edit_move_display (WEdit * e, long line);
void edit_word_wrap (WEdit * edit);
int edit_sort_cmd (WEdit * edit);
int edit_ext_cmd (WEdit * edit);
void edit_help_cmd (WEdit * edit);

int edit_save_macro_cmd (WEdit * edit, struct macro macro[], int n);
int edit_load_macro_cmd (WEdit * edit, struct macro macro[], int *n, int k);
void edit_delete_macro_cmd (WEdit * edit);

int edit_copy_to_X_buf_cmd (WEdit * edit);
int edit_cut_to_X_buf_cmd (WEdit * edit);
void edit_paste_from_X_buf_cmd (WEdit * edit);

void edit_select_codepage_cmd (WEdit *edit);
void edit_insert_literal_cmd (WEdit *edit);
void edit_execute_macro_cmd (WEdit *edit);
void edit_begin_end_macro_cmd(WEdit *edit);

void edit_paste_from_history (WEdit *edit);

void edit_set_filename (WEdit *edit, const char *name);

void edit_load_syntax (WEdit * edit, char ***pnames, const char *type);
void edit_free_syntax_rules (WEdit * edit);
void edit_get_syntax_color (WEdit * edit, long byte_index, int *color);


void book_mark_insert (WEdit * edit, int line, int c);
int book_mark_query_color (WEdit * edit, int line, int c);
int book_mark_query_all (WEdit * edit, int line, int *c);
struct _book_mark *book_mark_find (WEdit * edit, int line);
int book_mark_clear (WEdit * edit, int line, int c);
void book_mark_flush (WEdit * edit, int c);
void book_mark_inc (WEdit * edit, int line);
void book_mark_dec (WEdit * edit, int line);

int line_is_blank (WEdit *edit, long line);
int edit_indent_width (WEdit *edit, long p);
void edit_insert_indent (WEdit *edit, int indent);
void edit_options_dialog (void);
void edit_syntax_dialog (void);
void edit_mail_dialog (WEdit *edit);
void format_paragraph (WEdit *edit, int force);

/* either command or char_for_insertion must be passed as -1 */
void edit_execute_cmd (WEdit *edit, unsigned long command, int char_for_insertion);

#define get_sys_error(s) (s)

#define edit_error_dialog(h,s) query_dialog (h, s, D_ERROR, 1, _("&Dismiss"))

#define edit_query_dialog2(h,t,a,b) query_dialog (h, t, D_NORMAL, 2, a, b)
#define edit_query_dialog3(h,t,a,b,c) query_dialog (h, t, D_NORMAL, 3, a, b, c)

#define color_palette(x) win->color[x].pixel

#define NUM_SELECTION_HISTORY 64

#ifndef MAX_PATH_LEN
#ifdef PATH_MAX
#define MAX_PATH_LEN PATH_MAX
#else
#define MAX_PATH_LEN 1024
#endif
#endif

extern int edit_stack_iterator;
extern edit_stack_type edit_history_moveto [MAX_HISTORY_MOVETO];

extern WEdit *wedit;
extern struct WMenuBar *edit_menubar;

extern const global_keymap_t *editor_map;
extern const global_keymap_t *editor_x_map;

extern int option_line_state_width;

typedef enum {
    EDIT_QUICK_SAVE = 0,
    EDIT_SAFE_SAVE,
    EDIT_DO_BACKUP
} edit_save_mode_t;

extern int option_max_undo;
extern int option_auto_syntax;
extern char *option_syntax_type;

extern int option_edit_right_extreme;
extern int option_edit_left_extreme;
extern int option_edit_top_extreme;
extern int option_edit_bottom_extreme;

extern const char *option_whole_chars_search;
extern int search_create_bookmark;

extern int column_highlighting;

#endif				/* MC_EDIT_IMPL_H */
