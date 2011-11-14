/*
   editor private API
 */

/** \file edit-impl.h
 *  \brief Header: editor low level data handling and cursor fundamentals
 *  \author Paul Sheer
 *  \date 1996, 1997
 */

#ifndef MC__EDIT_IMPL_H
#define MC__EDIT_IMPL_H

#include <stdio.h>

#include "lib/search.h"         /* mc_search_type_t */
#include "lib/widget.h"         /* cb_ret_t */

#include "edit.h"

/*** typedefs(not structures) and defined constants **********************************************/

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
#define EDIT_TEXT_VERTICAL_OFFSET   0
#define EDIT_WITH_FRAME             1

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
#define CURS_LEFT       601
#define CURS_RIGHT      602
#define DELCHAR         603
#define BACKSPACE       604
#define STACK_BOTTOM    605
#define CURS_LEFT_LOTS  606
#define CURS_RIGHT_LOTS 607
#define COLUMN_ON       608
#define COLUMN_OFF      609
#define DELCHAR_BR      610
#define BACKSPACE_BR    611
#define MARK_1          1000
#define MARK_2          500000000
#define MARK_CURS       1000000000
#define KEY_PRESS       1500000000

/* Tabs spaces: (sofar only HALF_TAB_SIZE is used: */
#define TAB_SIZE      option_tab_spacing
#define HALF_TAB_SIZE ((int) option_tab_spacing / 2)

/* max count stack files */
#define MAX_HISTORY_MOVETO     50
#define LINE_STATE_WIDTH 8

#define LB_NAMES (LB_MAC + 1)

#define get_sys_error(s) (s)

#define edit_error_dialog(h,s) query_dialog (h, s, D_ERROR, 1, _("&Dismiss"))

#define edit_query_dialog2(h,t,a,b) query_dialog (h, t, D_NORMAL, 2, a, b)
#define edit_query_dialog3(h,t,a,b,c) query_dialog (h, t, D_NORMAL, 3, a, b, c)

/*** enums ***************************************************************************************/

/* line breaks */
typedef enum
{
    LB_ASIS = 0,
    LB_UNIX,
    LB_WIN,
    LB_MAC
} LineBreaks;

typedef enum
{
    EDIT_QUICK_SAVE = 0,
    EDIT_SAFE_SAVE,
    EDIT_DO_BACKUP
} edit_save_mode_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/* search/replace options */
typedef struct edit_search_options_t
{
    mc_search_type_t type;
    gboolean case_sens;
    gboolean backwards;
    gboolean only_in_selection;
    gboolean whole_words;
    gboolean all_codepages;
} edit_search_options_t;

typedef struct edit_stack_type
{
    long line;
    char *filename;
} edit_stack_type;

struct Widget;
struct WMenuBar;

/*** global variables defined in .c file *********************************************************/

extern const char VERTICAL_MAGIC[5];
/* if enable_show_tabs_tws ==1 then use visible_tab visible_tws */
extern int enable_show_tabs_tws;

extern edit_search_options_t edit_search_options;

extern int edit_stack_iterator;
extern edit_stack_type edit_history_moveto[MAX_HISTORY_MOVETO];

extern int option_line_state_width;

extern int option_max_undo;
extern int option_auto_syntax;

extern int option_edit_right_extreme;
extern int option_edit_left_extreme;
extern int option_edit_top_extreme;
extern int option_edit_bottom_extreme;

extern const char *option_whole_chars_search;
extern gboolean search_create_bookmark;

/*** declarations of public functions ************************************************************/

gboolean edit_add_window (Dlg_head * h, int y, int x, int lines, int cols, const char *f, int fline);
WEdit *find_editor (const Dlg_head *h);
gboolean edit_widget_is_editor (const Widget *w);
gboolean edit_drop_hotkey_menu (Dlg_head * h, int key);
void edit_menu_cmd (Dlg_head * h);
void user_menu (WEdit * edit, const char *menu_file, int selected_entry);
void edit_init_menu (struct WMenuBar *menubar);
void edit_save_mode_cmd (void);
gboolean edit_translate_key (WEdit * edit, long x_key, int *cmd, int *ch);
int edit_get_byte (WEdit * edit, long byte_index);
int edit_get_utf (WEdit * edit, long byte_index, int *char_width);
long edit_count_lines (WEdit * edit, long current, long upto);
long edit_move_forward (WEdit * edit, long current, long lines, long upto);
long edit_move_forward3 (WEdit * edit, long current, int cols, long upto);
long edit_move_backward (WEdit * edit, long current, long lines);
void edit_scroll_screen_over_cursor (WEdit * edit);
void edit_render_keypress (WEdit * edit);
void edit_scroll_upward (WEdit * edit, unsigned long i);
void edit_scroll_downward (WEdit * edit, int i);
void edit_scroll_right (WEdit * edit, int i);
void edit_scroll_left (WEdit * edit, int i);
void edit_move_up (WEdit * edit, unsigned long i, int scroll);
void edit_move_down (WEdit * edit, unsigned long i, int scroll);
void edit_move_to_prev_col (WEdit * edit, long p);
int edit_get_col (WEdit * edit);
long edit_bol (WEdit * edit, long current);
long edit_eol (WEdit * edit, long current);
void edit_update_curs_row (WEdit * edit);
void edit_update_curs_col (WEdit * edit);
void edit_find_bracket (WEdit * edit);
gboolean edit_reload_line (WEdit * edit, const char *filename, long line);
void edit_set_codeset (WEdit * edit);

void edit_block_copy_cmd (WEdit * edit);
void edit_block_move_cmd (WEdit * edit);
int edit_block_delete_cmd (WEdit * edit);
void edit_delete_line (WEdit * edit);

int edit_delete (WEdit * edit, const int byte_delete);
void edit_insert (WEdit * edit, int c);
void edit_cursor_move (WEdit * edit, long increment);
void edit_push_undo_action (WEdit * edit, long c, ...);
void edit_push_redo_action (WEdit * edit, long c, ...);
void edit_push_key_press (WEdit * edit);
void edit_insert_ahead (WEdit * edit, int c);
long edit_write_stream (WEdit * edit, FILE * f);
char *edit_get_write_filter (const char *writename, const char *filename);
int edit_save_confirm_cmd (WEdit * edit);
int edit_save_as_cmd (WEdit * edit);
WEdit *edit_init (WEdit * edit, int y, int x, int lines, int cols, const char *filename, long line);
gboolean edit_clean (WEdit * edit);
gboolean edit_ok_to_exit (WEdit * edit);
gboolean edit_load_cmd (Dlg_head * h);
gboolean edit_load_syntax_file (Dlg_head * h);
gboolean edit_load_menu_file (Dlg_head * h);
gboolean edit_close_cmd (WEdit * edit);
void edit_mark_cmd (WEdit * edit, int unmark);
void edit_mark_current_word_cmd (WEdit * edit);
void edit_mark_current_line_cmd (WEdit * edit);
void edit_set_markers (WEdit * edit, long m1, long m2, int c1, int c2);
void edit_push_markers (WEdit * edit);
void edit_replace_cmd (WEdit * edit, int again);
void edit_search_cmd (WEdit * edit, gboolean again);
int edit_search_cmd_callback (const void *user_data, gsize char_offset);
void edit_complete_word_cmd (WEdit * edit);
void edit_get_match_keyword_cmd (WEdit * edit);
int edit_save_block (WEdit * edit, const char *filename, long start, long finish);
int edit_save_block_cmd (WEdit * edit);
gboolean edit_insert_file_cmd (WEdit * edit);
void edit_insert_over (WEdit * edit);
int edit_insert_column_of_text_from_file (WEdit * edit, int file,
                                          long *start_pos, long *end_pos, int *col1, int *col2);
long edit_insert_file (WEdit * edit, const char *filename);
gboolean edit_load_back_cmd (WEdit * edit);
gboolean edit_load_forward_cmd (WEdit * edit);
void edit_block_process_cmd (WEdit * edit, int macro_number);
void edit_refresh_cmd (void);
void edit_syntax_onoff_cmd (Dlg_head * h);
void edit_show_tabs_tws_cmd (Dlg_head * h);
void edit_show_margin_cmd (Dlg_head * h);
void edit_date_cmd (WEdit * edit);
void edit_goto_cmd (WEdit * edit);
int eval_marks (WEdit * edit, long *start_mark, long *end_mark);
void edit_info_status (WEdit * edit);
void edit_status (WEdit * edit);
void edit_execute_key_command (WEdit * edit, unsigned long command, int char_for_insertion);
void edit_update_screen (WEdit * edit);
void edit_save_size (WEdit * edit);
gboolean edit_handle_move_resize (WEdit * edit, unsigned long command);
void edit_move_to_line (WEdit * e, long line);
void edit_move_display (WEdit * e, long line);
void edit_word_wrap (WEdit * edit);
int edit_sort_cmd (WEdit * edit);
int edit_ext_cmd (WEdit * edit);

int edit_store_macro_cmd (WEdit * edit);
gboolean edit_load_macro_cmd (WEdit * edit);
void edit_delete_macro_cmd (WEdit * edit);
gboolean edit_repeat_macro_cmd (WEdit * edit);

int edit_copy_to_X_buf_cmd (WEdit * edit);
int edit_cut_to_X_buf_cmd (WEdit * edit);
void edit_paste_from_X_buf_cmd (WEdit * edit);

void edit_select_codepage_cmd (WEdit * edit);
void edit_insert_literal_cmd (WEdit * edit);
gboolean edit_execute_macro (WEdit * edit, int hotkey);
void edit_begin_end_macro_cmd (WEdit * edit);
void edit_begin_end_repeat_cmd (WEdit * edit);

void edit_paste_from_history (WEdit * edit);

void edit_set_filename (WEdit * edit, const char *name);

void edit_load_syntax (WEdit * edit, char ***pnames, const char *type);
void edit_free_syntax_rules (WEdit * edit);
void edit_get_syntax_color (WEdit * edit, long byte_index, int *color);

void book_mark_insert (WEdit * edit, size_t line, int c);
int book_mark_query_color (WEdit * edit, int line, int c);
int book_mark_query_all (WEdit * edit, int line, int *c);
struct _book_mark *book_mark_find (WEdit * edit, int line);
int book_mark_clear (WEdit * edit, int line, int c);
void book_mark_flush (WEdit * edit, int c);
void book_mark_inc (WEdit * edit, int line);
void book_mark_dec (WEdit * edit, int line);
void book_mark_serialize (WEdit * edit, int color);
void book_mark_restore (WEdit * edit, int color);

int line_is_blank (WEdit * edit, long line);
int edit_indent_width (WEdit * edit, long p);
void edit_insert_indent (WEdit * edit, int indent);
void edit_options_dialog (Dlg_head *h);
void edit_syntax_dialog (WEdit * edit);
void edit_mail_dialog (WEdit * edit);
void format_paragraph (WEdit * edit, int force);

unsigned int edit_unlock_file (WEdit * edit);
unsigned int edit_lock_file (WEdit * edit);

/* either command or char_for_insertion must be passed as -1 */
void edit_execute_cmd (WEdit * edit, unsigned long command, int char_for_insertion);

/*** inline functions ****************************************************************************/

/**
 * Load a new file into the editor.  If it fails, preserve the old file.
 * To do it, allocate a new widget, initialize it and, if the new file
 * was loaded, copy the data to the old widget.
 * Return TRUE on success, FALSE on failure.
 */
static inline gboolean
edit_reload (WEdit * edit, const char *filename)
{
    return edit_reload_line (edit, filename, 0);
}

#endif /* MC__EDIT_IMPL_H */
