#ifndef MC__EDIT_EVENT_H
#define MC__EDIT_EVENT_H

#include "lib/event.h"

#include "editwidget.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef enum
{
    NONE,
    TO_LEFT,
    TO_RIGHT,
    TO_WORD_BEGIN,
    TO_WORD_END,
    TO_UP,
    TO_DOWN,
    TO_PARAGRAPH_UP,
    TO_PARAGRAPH_DOWN,
    TO_SCROLL_UP,
    TO_SCROLL_DOWN,
    TO_PAGE_UP,
    TO_PAGE_DOWN,
    TO_PAGE_BEGIN,
    TO_PAGE_END,
    TO_LINE_BEGIN,
    TO_LINE_END,
    TO_FILE_BEGIN,
    TO_FILE_END,
} mc_editor_event_direction_t;

typedef struct
{
    struct WEdit *editor;
    int char_for_insertion;
    long line;
} mc_editor_event_data_insert_char_t;

typedef struct
{
    struct WEdit *editor;
    gboolean is_mark;
    gboolean is_column;
    mc_editor_event_direction_t direction;
    long line;
} mc_editor_event_data_move_cursor_t;

typedef struct
{
    struct WEdit *editor;
    gboolean return_value;
} mc_editor_event_data_ret_boolean_t;


typedef struct
{
    WEdit *editor;
    const char *menu_file;
    int selected_entry;
} mc_editor_event_data_user_menu_t;


/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void mc_editor_init_events (GError ** error);

gboolean mc_editor_cmd_backspace (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_delete (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_delete_word (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_delete_line (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_enter (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_insert_char_raw (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_insert_char (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_undo (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_redo (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_toggle_fullscreen (event_info_t * event_info, gpointer data,
                                          GError ** error);
gboolean mc_editor_cmd_move_cursor (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_tab (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_switch_insert_overwrite (event_info_t * event_info, gpointer data,
                                                GError ** error);
gboolean mc_editor_cmd_mark (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_mark_column (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_mark_all (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_unmark (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_mark_word (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_mark_line (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_bookmark_toggle (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_bookmark_flush (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_bookmark_next (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_bookmark_prev (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_block_copy (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_block_delete (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_block_move (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_block_move_to_left (event_info_t * event_info, gpointer data,
                                           GError ** error);
gboolean mc_editor_cmd_block_move_to_right (event_info_t * event_info, gpointer data,
                                            GError ** error);
gboolean mc_editor_cmd_ext_clip_copy (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_ext_clip_cut (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_ext_clip_paste (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_paste_from_history (event_info_t * event_info, gpointer data,
                                           GError ** error);
gboolean mc_editor_cmd_format_paragraph (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_format_paragraph_auto (event_info_t * event_info, gpointer data,
                                              GError ** error);
gboolean mc_editor_cmd_save_as (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_save_confirm (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_save_block (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_insert_file (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_file_load_prev (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_file_load_next (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_syntax_show_dialog (event_info_t * event_info, gpointer data,
                                           GError ** error);
gboolean mc_editor_cmd_search (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_replace (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_complete (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_match_keyword (event_info_t * event_info, gpointer data, GError ** error);

gboolean mc_editor_cmd_print_current_date (event_info_t * event_info, gpointer data,
                                           GError ** error);
gboolean mc_editor_cmd_goto_line (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_macro_delete (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_goto_matching_bracket (event_info_t * event_info, gpointer data,
                                              GError ** error);
gboolean mc_editor_cmd_user_menu (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_sort (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_run_external_command (event_info_t * event_info, gpointer data,
                                             GError ** error);
gboolean mc_editor_cmd_mail (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_insert_literal (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_macro_record_start_stop (event_info_t * event_info, gpointer data,
                                                GError ** error);
gboolean mc_editor_cmd_macro_repeat_start_stop (event_info_t * event_info, gpointer data,
                                                GError ** error);
gboolean mc_editor_cmd_macro_store (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_macro_repeat (event_info_t * event_info, gpointer data, GError ** error);

#ifdef HAVE_ASPELL
gboolean mc_editor_cmd_spell_set_language (event_info_t * event_info, gpointer data,
                                           GError ** error);
gboolean mc_editor_cmd_spell_check (event_info_t * event_info, gpointer data, GError ** error);
gboolean mc_editor_cmd_spell_suggest_word (event_info_t * event_info, gpointer data,
                                           GError ** error);
#endif /* HAVE_ASPELL */

#ifdef HAVE_CHARSET
gboolean mc_editor_cmd_select_codepage (event_info_t * event_info, gpointer data, GError ** error);
#endif /* HAVE_CHARSET */


/*** inline functions ****************************************************************************/

#endif /* MC__EDIT_EVENT_H */
