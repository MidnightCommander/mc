/* edit.h - main include file

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.
*/

#ifndef __EDIT_H
#define __EDIT_H

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <errno.h>

#include <stdlib.h>

#include "src/global.h"

#define N_menus 5

#define SEARCH_DIALOG_OPTION_NO_SCANF	1
#define SEARCH_DIALOG_OPTION_NO_REGEX	2
#define SEARCH_DIALOG_OPTION_NO_CASE	4
#define SEARCH_DIALOG_OPTION_BACKWARDS	8
#define SEARCH_DIALOG_OPTION_BOOKMARK	16

#define EDIT_KEY_EMULATION_NORMAL 0
#define EDIT_KEY_EMULATION_EMACS  1

#define REDRAW_LINE          (1 << 0)
#define REDRAW_LINE_ABOVE    (1 << 1)
#define REDRAW_LINE_BELOW    (1 << 2)
#define REDRAW_AFTER_CURSOR  (1 << 3)
#define REDRAW_BEFORE_CURSOR (1 << 4)
#define REDRAW_PAGE          (1 << 5)
#define REDRAW_IN_BOUNDS     (1 << 6)
#define REDRAW_CHAR_ONLY     (1 << 7)
#define REDRAW_COMPLETELY    (1 << 8)

#define EDIT_TEXT_HORIZONTAL_OFFSET 0
#define EDIT_TEXT_VERTICAL_OFFSET 1

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
#define CURS_LEFT 601
#define CURS_RIGHT 602
#define DELCHAR 603
#define BACKSPACE 604
#define STACK_BOTTOM 605
#define CURS_LEFT_LOTS 606
#define CURS_RIGHT_LOTS 607
#define COLUMN_ON 608
#define COLUMN_OFF 609
#define MARK_1 1000
#define MARK_2 700000000
#define KEY_PRESS 1400000000

/* Tabs spaces: (sofar only HALF_TAB_SIZE is used: */
#define TAB_SIZE		option_tab_spacing
#define HALF_TAB_SIZE		((int) option_tab_spacing / 2)

struct macro {
    short command;
    short ch;
};

struct WEdit;
typedef struct WEdit WEdit;

int edit_drop_hotkey_menu (WEdit *e, int key);
void edit_menu_cmd (WEdit *e);
void edit_init_menu_emacs (void);
void edit_init_menu_normal (void);
void edit_done_menu (void);
void menu_save_mode_cmd (void);
int edit_raw_key_query (char *heading, char *query, int cancel);
int edit (const char *_file, int line);
int edit_translate_key (WEdit *edit, long x_key, int *cmd, int *ch);

#ifndef NO_INLINE_GETBYTE
int edit_get_byte (WEdit * edit, long byte_index);
#else
static inline int edit_get_byte (WEdit * edit, long byte_index)
{
    unsigned long p;
    if (byte_index >= (edit->curs1 + edit->curs2) || byte_index < 0)
	return '\n';

    if (byte_index >= edit->curs1) {
	p = edit->curs1 + edit->curs2 - byte_index - 1;
	return edit->buffers2[p >> S_EDIT_BUF_SIZE][EDIT_BUF_SIZE - (p & M_EDIT_BUF_SIZE) - 1];
    } else {
	return edit->buffers1[byte_index >> S_EDIT_BUF_SIZE][byte_index & M_EDIT_BUF_SIZE];
    }
}
#endif

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
void edit_move_down (WEdit * edit, int i, int scroll);
int edit_get_col (WEdit * edit);
long edit_bol (WEdit * edit, long current);
long edit_eol (WEdit * edit, long current);
void edit_update_curs_row (WEdit * edit);
void edit_update_curs_col (WEdit * edit);

void edit_block_copy_cmd (WEdit * edit);
void edit_block_move_cmd (WEdit * edit);
int edit_block_delete_cmd (WEdit * edit);
void edit_delete_line (WEdit * edit);

int edit_delete (WEdit * edit);
void edit_insert (WEdit * edit, int c);
int edit_cursor_move (WEdit * edit, long increment);
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
int edit_load_cmd (WEdit * edit);
void edit_mark_cmd (WEdit * edit, int unmark);
void edit_set_markers (WEdit * edit, long m1, long m2, int c1, int c2);
void edit_push_markers (WEdit * edit);
void edit_replace_cmd (WEdit * edit, int again);
void edit_search_cmd (WEdit * edit, int again);
void edit_complete_word_cmd (WEdit * edit);
int edit_save_block (WEdit * edit, const char *filename, long start, long finish);
int edit_save_block_cmd (WEdit * edit);
int edit_insert_file_cmd (WEdit * edit);
int edit_insert_file (WEdit * edit, const char *filename);
void edit_block_process_cmd (WEdit * edit, const char *shell_cmd, int block);
char *catstrs (const char *first, ...);
void freestrs (void);
void edit_refresh_cmd (WEdit * edit);
void edit_date_cmd (WEdit * edit);
void edit_goto_cmd (WEdit * edit);
int eval_marks (WEdit * edit, long *start_mark, long *end_mark);
void edit_status (WEdit * edit);
int edit_execute_key_command (WEdit * edit, int command, int char_for_insertion);
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

void edit_paste_from_history (WEdit *edit);

void edit_set_filename (WEdit *edit, const char *name);

void edit_load_syntax (WEdit * edit, char **names, char *type);
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
void edit_mail_dialog (WEdit *edit);
void format_paragraph (WEdit *edit, int force);

/* either command or char_for_insertion must be passed as -1 */
int edit_execute_cmd (WEdit * edit, int command, int char_for_insertion);

#define get_sys_error(s) (s)

#define edit_error_dialog(h,s) query_dialog (h, s, 0, 1, _("&Dismiss"))

#define edit_message_dialog(h,s) query_dialog (h, s, 0, 1, _("&OK"))
#define edit_query_dialog2(h,t,a,b) query_dialog(h,t,0,2,a,b)
#define edit_query_dialog3(h,t,a,b,c) query_dialog(h,t,0,3,a,b,c)
#define edit_query_dialog4(h,t,a,b,c,d) query_dialog(h,t,0,4,a,b,c,d)

#define color_palette(x) win->color[x].pixel

#define NUM_SELECTION_HISTORY 64

#ifndef MAX_PATH_LEN
#ifdef PATH_MAX
#define MAX_PATH_LEN PATH_MAX
#else
#define MAX_PATH_LEN 1024
#endif
#endif

/*
   what editor are we going to emulate? one of EDIT_KEY_EMULATION_NORMAL
   or EDIT_KEY_EMULATION_EMACS
 */
extern int edit_key_emulation;

extern WEdit *wedit;
struct Menu;
extern struct Menu *EditMenuBar[];
struct WMenu;
extern struct WMenu *edit_menubar;

extern int option_word_wrap_line_length;
extern int option_typewriter_wrap;
extern int option_auto_para_formatting;
extern int option_tab_spacing;
extern int option_fill_tabs_with_spaces;
extern int option_return_does_auto_indent;
extern int option_backspace_through_tabs;
extern int option_fake_half_tabs;

typedef enum {
    EDIT_QUICK_SAVE = 0,
    EDIT_SAFE_SAVE,
    EDIT_DO_BACKUP
} edit_save_mode_t;

extern int option_save_mode;
extern int option_save_position;
extern int option_backup_ext_int;
extern int option_max_undo;
extern int option_syntax_highlighting;
extern int editor_option_check_nl_at_eof;

extern int option_edit_right_extreme;
extern int option_edit_left_extreme;
extern int option_edit_top_extreme;
extern int option_edit_bottom_extreme;

extern char *option_whole_chars_search;
extern char *option_backup_ext;

extern int edit_confirm_save;
extern int column_highlighting;

/* File names */
#define EDIT_DIR           PATH_SEP_STR ".mc" PATH_SEP_STR "cedit"
#define SYNTAX_FILE        EDIT_DIR PATH_SEP_STR "Syntax"
#define CLIP_FILE          EDIT_DIR PATH_SEP_STR "cooledit.clip"
#define MACRO_FILE         EDIT_DIR PATH_SEP_STR "cooledit.macros"
#define BLOCK_FILE         EDIT_DIR PATH_SEP_STR "cooledit.block"
#define TEMP_FILE          EDIT_DIR PATH_SEP_STR "cooledit.temp"

#endif 				/* __EDIT_H */
