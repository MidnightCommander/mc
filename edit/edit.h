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

#ifdef HAVE_SLANG
#    define HAVE_SYNTAXH 1
#endif

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

#include <fcntl.h>

#include <stdlib.h>
#if !defined(STDC_HEADERS) && defined(HAVE_MALLOC_H)
#    include <malloc.h>
#endif

#if defined(HAVE_RX_H) && defined(HAVE_REGCOMP)
#    include <rx.h>
#else
#    include <regex.h>
#endif

#include "src/global.h"
#include "src/tty.h"
#include "src/main.h"		/* for char *shell */
#include "src/mad.h"
#include "src/dlg.h"
#include "src/widget.h"
#include "src/color.h"
#include "src/dialog.h"
#include "src/mouse.h"
#include "src/help.h"
#include "src/key.h"
#include "src/wtools.h"		/* for QuickWidgets */
#include "src/cmd.h"		/* for menu_edit_cmd */
#include "src/win.h"
#include "vfs/vfs.h"
#include "src/menu.h"
#include "edit-widget.h"
#include "src/user.h" /* for user_menu_cmd, must be after edit-widget.h */

#    define WANT_WIDGETS
     
#    define WIDGET_COMMAND (WIDGET_USER + 10)
#    define N_menus 5

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

#define MOD_ABNORMAL		(1 << 0)
#define MOD_UNDERLINED		(1 << 1)
#define MOD_BOLD		(1 << 2)
#define MOD_HIGHLIGHTED		(1 << 3)
#define MOD_MARKED		(1 << 4)
#define MOD_ITALIC		(1 << 5)
#define MOD_CURSOR		(1 << 6)
#define MOD_INVERSE		(1 << 7)

#define EDIT_TEXT_HORIZONTAL_OFFSET 0
#define EDIT_TEXT_VERTICAL_OFFSET 1
#define FONT_OFFSET_X 0
#define FONT_OFFSET_Y 0

#define EDIT_RIGHT_EXTREME option_edit_right_extreme
#define EDIT_LEFT_EXTREME option_edit_left_extreme
#define EDIT_TOP_EXTREME option_edit_top_extreme
#define EDIT_BOTTOM_EXTREME option_edit_bottom_extreme

/*there are a maximum of ... */
/*... edit buffers, each of which is ... */
#define EDIT_BUF_SIZE 0x10000
/* ...bytes in size. */

/*x / EDIT_BUF_SIZE equals x >> ... */
#define S_EDIT_BUF_SIZE 16

/* x % EDIT_BUF_SIZE is equal to x && ... */
#define M_EDIT_BUF_SIZE 0xFFFF

#define SIZE_LIMIT (EDIT_BUF_SIZE * (MAXBUFF - 2))
/* Note a 16k stack is 64k of data and enough to hold (usually) around 10
   pages of undo info. */

/* undo stack */
#define START_STACK_SIZE 32


/*some codes that may be pushed onto or returned from the undo stack: */
#define CURS_LEFT 601
#define CURS_RIGHT 602
#define DELETE 603
#define BACKSPACE 604
#define STACK_BOTTOM 605
#define CURS_LEFT_LOTS 606
#define CURS_RIGHT_LOTS 607
#define COLUMN_ON 608
#define COLUMN_OFF 609
#define MARK_1 1000
#define MARK_2 700000000
#define KEY_PRESS 1400000000

/*Tabs spaces: (sofar only HALF_TAB_SIZE is used: */
#define TAB_SIZE		option_tab_spacing
#define HALF_TAB_SIZE		((int) option_tab_spacing / 2)

struct selection {
   unsigned char * text;
   int len;
};

#define MAX_WORDS_PER_CONTEXT	1024
#define MAX_CONTEXTS		128

struct key_word {
    char *keyword;
    unsigned char first;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
    time_t time;
#define NO_COLOR 0x7FFFFFFF
#define SPELLING_ERROR 0x7EFEFEFE
    int line_start;
    int bg;
    int fg;
};

struct context_rule {
    char *left;
    unsigned char first_left;
    char *right;
    unsigned char first_right;
    char line_start_left;
    char line_start_right;
    int single_char;
    int between_delimiters;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
    char *keyword_first_chars;
    int spelling;
/* first word is word[1] */
    struct key_word **keyword;
};

int edit_drop_hotkey_menu (WEdit * e, int key);
void edit_menu_cmd (WEdit * e);
void edit_init_menu_emacs (void);
void edit_init_menu_normal (void);
void edit_done_menu (void);
int edit_raw_key_query (char *heading, char *query, int cancel);
char *strcasechr (const unsigned char *s, int c);
int edit (const char *_file, int line);
int edit_translate_key (WEdit * edit, unsigned int x_keycode, long x_key, int x_state, int *cmd, int *ch);

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

char *edit_get_buffer_as_text (WEdit * edit);
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
int edit_get_col (WEdit * edit);
long edit_bol (WEdit * edit, long current);
long edit_eol (WEdit * edit, long current);
void edit_update_curs_row (WEdit * edit);
void edit_update_curs_col (WEdit * edit);

void edit_block_copy_cmd (WEdit * edit);
void edit_block_move_cmd (WEdit * edit);
int edit_block_delete_cmd (WEdit * edit);
int edit_block_delete (WEdit * edit);
void edit_delete_line (WEdit * edit);

int edit_delete (WEdit * edit);
void edit_insert (WEdit * edit, int c);
int edit_cursor_move (WEdit * edit, long increment);
void edit_push_action (WEdit * edit, long c,...);
void edit_push_key_press (WEdit * edit);
void edit_insert_ahead (WEdit * edit, int c);
int edit_save_file (WEdit * edit, const char *filename);
long edit_write_stream (WEdit * edit, FILE * f);
char *edit_get_write_filter (char *writename, const char *filename);
int edit_save_cmd (WEdit * edit);
int edit_save_confirm_cmd (WEdit * edit);
int edit_save_as_cmd (WEdit * edit);
WEdit *edit_init (WEdit * edit, int lines, int columns, const char *filename, const char *text, const char *dir, unsigned long text_size);
int edit_clean (WEdit * edit);
int edit_renew (WEdit * edit);
int edit_new_cmd (WEdit * edit);
int edit_reload (WEdit * edit, const char *filename, const char *text, const char *dir, unsigned long text_size);
int edit_load_cmd (WEdit * edit);
void edit_mark_cmd (WEdit * edit, int unmark);
void edit_set_markers (WEdit * edit, long m1, long m2, int c1, int c2);
void edit_push_markers (WEdit * edit);
void edit_quit_cmd (WEdit * edit);
void edit_replace_cmd (WEdit * edit, int again);
void edit_search_cmd (WEdit * edit, int again);
int edit_save_block (WEdit * edit, const char *filename, long start, long finish);
int edit_save_block_cmd (WEdit * edit);
int edit_insert_file_cmd (WEdit * edit);
int edit_insert_file (WEdit * edit, const char *filename);
void edit_block_process_cmd (WEdit * edit, const char *shell_cmd, int block);
char *catstrs (const char *first,...);
void edit_refresh_cmd (WEdit * edit);
void edit_date_cmd (WEdit * edit);
void edit_goto_cmd (WEdit * edit);
int eval_marks (WEdit * edit, long *start_mark, long *end_mark);
void edit_status (WEdit * edit);
int edit_execute_command (WEdit * edit, int command, int char_for_insertion);
int edit_execute_key_command (WEdit * edit, int command, int char_for_insertion);
void edit_update_screen (WEdit * edit);
int edit_printf (WEdit * e, const char *fmt,...);
int edit_print_string (WEdit * e, const char *s);
void edit_move_to_line (WEdit * e, long line);
void edit_move_display (WEdit * e, long line);
void edit_word_wrap (WEdit * edit);
unsigned char *edit_get_block (WEdit * edit, long start, long finish, int *l);
int edit_sort_cmd (WEdit * edit);
void edit_help_cmd (WEdit * edit);
void edit_left_word_move (WEdit * edit, int s);
void edit_right_word_move (WEdit * edit, int s);
void edit_get_selection (WEdit * edit);

int edit_save_macro_cmd (WEdit * edit, struct macro macro[], int n);
int edit_load_macro_cmd (WEdit * edit, struct macro macro[], int *n, int k);
void edit_delete_macro_cmd (WEdit * edit);

int edit_copy_to_X_buf_cmd (WEdit * edit);
int edit_cut_to_X_buf_cmd (WEdit * edit);
void edit_paste_from_X_buf_cmd (WEdit * edit);

void edit_paste_from_history (WEdit *edit);

void edit_split_filename (WEdit * edit, const char *name);

#define CWidget Widget
void edit_set_syntax_change_callback (void (*callback) (CWidget *));
void edit_load_syntax (WEdit * edit, char **names, char *type);
void edit_free_syntax_rules (WEdit * edit);
void edit_get_syntax_color (WEdit * edit, long byte_index, int *fg, int *bg);
int edit_check_spelling (WEdit * edit);


void book_mark_insert (WEdit * edit, int line, int c);
int book_mark_query_color (WEdit * edit, int line, int c);
int book_mark_query_all (WEdit * edit, int line, int *c);
struct _book_mark *book_mark_find (WEdit * edit, int line);
int book_mark_clear (WEdit * edit, int line, int c);
void book_mark_flush (WEdit * edit, int c);
void book_mark_inc (WEdit * edit, int line);
void book_mark_dec (WEdit * edit, int line);

void user_menu (WEdit *edit);

#define CPushFont(x,y)
#define CPopFont()
#define FIXED_FONT 1

/* put OS2/NT/WIN95 defines here */

#ifdef USE_O_TEXT
#    define MY_O_TEXT O_TEXT
#else
#    define MY_O_TEXT 0
#endif

#define FONT_PIX_PER_LINE 1
#define FONT_MEAN_WIDTH 1
     
#define get_sys_error(s) (s)
#define open mc_open
#define close(f) mc_close(f)
#define read(f,b,c) mc_read(f,b,c)
#define write(f,b,c) mc_write(f,b,c)
#define stat(f,s) mc_stat(f,s)
#define mkdir(s,m) mc_mkdir(s,m)
#define itoa MY_itoa

#define edit_get_load_file(d,f,h) input_dialog (h, " Enter file name: ", f)
#define edit_get_save_file(d,f,h) input_dialog (h, " Enter file name: ", f)
#define CMalloc(x) malloc(x)
     
#define set_error_msg(s) edit_init_error_msg = strdup(s)

#ifdef _EDIT_C

#define edit_error_dialog(h,s) set_error_msg(s)
char *edit_init_error_msg = NULL;

#else				/* ! _EDIT_C */

#define edit_error_dialog(h,s) query_dialog (h, s, 0, 1, _("&Dismiss"))
#define edit_message_dialog(h,s) query_dialog (h, s, 0, 1, _("&Ok"))
extern char *edit_init_error_msg;

#endif				/* ! _EDIT_C */


#define get_error_msg(s) edit_init_error_msg
#define edit_query_dialog2(h,t,a,b) query_dialog(h,t,0,2,a,b)
#define edit_query_dialog3(h,t,a,b,c) query_dialog(h,t,0,3,a,b,c)
#define edit_query_dialog4(h,t,a,b,c,d) query_dialog(h,t,0,4,a,b,c,d)

#define color_palette(x) win->color[x].pixel

extern char *home_dir;

#define NUM_SELECTION_HISTORY 64

#ifndef MAX_PATH_LEN
#ifdef PATH_MAX
#define MAX_PATH_LEN PATH_MAX
#else
#define MAX_PATH_LEN 1024
#endif
#endif

#ifdef _EDIT_C

struct selection selection =
{0, 0};
int current_selection = 0;
/* Note: selection.text = selection_history[current_selection].text */
struct selection selection_history[NUM_SELECTION_HISTORY] =
{
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0}
};

/*
   what editor are we going to emulate? one of EDIT_KEY_EMULATION_NORMAL
   or EDIT_KEY_EMULATION_EMACS
 */
int edit_key_emulation = EDIT_KEY_EMULATION_NORMAL;

int option_word_wrap_line_length = 72;
int option_typewriter_wrap = 0;
int option_auto_para_formatting = 0;
int option_international_characters = 0;
int option_tab_spacing = 8;
int option_fill_tabs_with_spaces = 0;
int option_return_does_auto_indent = 1;
int option_backspace_through_tabs = 0;
int option_fake_half_tabs = 1;
int option_save_mode = 0;
int option_backup_ext_int = -1;
int option_find_bracket = 1;
int option_max_undo = 32768;

int option_editor_fg_normal = 26;
int option_editor_fg_bold = 8;
int option_editor_fg_italic = 10;

int option_edit_right_extreme = 0;
int option_edit_left_extreme = 0;
int option_edit_top_extreme = 0;
int option_edit_bottom_extreme = 0;

int option_editor_bg_normal = 1;
int option_editor_bg_abnormal = 0;
int option_editor_bg_marked = 2;
int option_editor_bg_marked_abnormal = 9;
int option_editor_bg_highlighted = 12;
int option_editor_fg_cursor = 18;

char *option_whole_chars_search = "0123456789abcdefghijklmnopqrstuvwxyz_";
char *option_chars_move_whole_word = "!=&|<>^~ !:;, !'!`!.?!\"!( !) !Aa0 !+-*/= |<> ![ !] !\\#! ";
char *option_backup_ext = "~";

#else				/* ! _EDIT_C */

extern struct selection selection;
extern struct selection selection_history[];
extern int current_selection;

/*
   what editor are we going to emulate? one of EDIT_KEY_EMULATION_NORMAL
   or EDIT_KEY_EMULATION_EMACS
 */
extern int edit_key_emulation;
extern WEdit *wedit;

extern int option_word_wrap_line_length;
extern int option_typewriter_wrap;
extern int option_auto_para_formatting;
extern int option_international_characters;
extern int option_tab_spacing;
extern int option_fill_tabs_with_spaces;
extern int option_return_does_auto_indent;
extern int option_backspace_through_tabs;
extern int option_fake_half_tabs;
extern int option_save_mode;
extern int option_backup_ext_int;
extern int option_find_bracket;
extern int option_max_undo;

extern int option_editor_fg_normal;
extern int option_editor_fg_bold;
extern int option_editor_fg_italic;

extern int option_edit_right_extreme;
extern int option_edit_left_extreme;
extern int option_edit_top_extreme;
extern int option_edit_bottom_extreme;

extern int option_editor_bg_normal;
extern int option_editor_bg_abnormal;
extern int option_editor_bg_marked;
extern int option_editor_bg_marked_abnormal;
extern int option_editor_bg_highlighted;
extern int option_editor_fg_cursor;

extern char *option_whole_chars_search;
extern char *option_chars_move_whole_word;
extern char *option_backup_ext;

extern int edit_confirm_save;

#endif				/* ! _EDIT_C */
#endif 				/* __EDIT_H */
