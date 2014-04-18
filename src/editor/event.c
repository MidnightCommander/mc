/*
   Editor's events definitions.

   Copyright (C) 2012-2014
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2014

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

#include <config.h>

#include "lib/global.h"
#include "lib/event.h"

#include "src/setup.h"          /* option_tab_spacing */

#include "edit-impl.h"
#include "event.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_editor_init_events (GError ** error)
{
    /* *INDENT-OFF* */
    event_init_group_t core_group_events[] =
    {
        {"backspace", mc_editor_cmd_backspace, NULL},
        {"delete", mc_editor_cmd_delete, NULL},
        {"delete_word_left", mc_editor_cmd_delete_word, (void *) TO_LEFT},
        {"delete_word_right", mc_editor_cmd_delete_word, (void *) TO_RIGHT},

        {"delete_line", mc_editor_cmd_delete_line, (void *) NONE},
        {"delete_line_to_begin", mc_editor_cmd_delete_line, (void *) TO_LINE_BEGIN},
        {"delete_line_to_end", mc_editor_cmd_delete_line, (void *) TO_LINE_END},

        {"enter", mc_editor_cmd_enter, NULL},
        {"insert_char_raw", mc_editor_cmd_insert_char_raw, NULL},
        {"insert_char", mc_editor_cmd_insert_char, NULL},
        {"undo", mc_editor_cmd_undo, NULL},
        {"redo", mc_editor_cmd_redo, NULL},
        {"toggle_fullscreen", mc_editor_cmd_toggle_fullscreen, NULL},
        {"tab", mc_editor_cmd_tab, NULL},
        {"switch_insert_overwrite", mc_editor_cmd_switch_insert_overwrite, NULL},

        {"move_cursor", mc_editor_cmd_move_cursor, NULL},

        {"mark", mc_editor_cmd_mark, NULL},
        {"mark_column", mc_editor_cmd_mark_column, NULL},
        {"mark_all", mc_editor_cmd_mark_all, NULL},
        {"unmark", mc_editor_cmd_unmark, NULL},
        {"mark_word", mc_editor_cmd_mark, NULL},
        {"mark_line", mc_editor_cmd_mark, NULL},

        {"bookmark_toggle", mc_editor_cmd_bookmark_toggle, NULL},
        {"bookmark_flush", mc_editor_cmd_bookmark_flush, NULL},
        {"bookmark_next", mc_editor_cmd_bookmark_next, NULL},
        {"bookmark_prev", mc_editor_cmd_bookmark_prev, NULL},

        {"block_copy", mc_editor_cmd_block_copy, NULL},
        {"block_delete", mc_editor_cmd_block_delete, NULL},
        {"block_move", mc_editor_cmd_block_move, NULL},
        {"block_move_to_left", mc_editor_cmd_block_move_to_left, NULL},
        {"block_move_to_right", mc_editor_cmd_block_move_to_right, NULL},

        {"ext_clip_copy", mc_editor_cmd_ext_clip_copy, NULL},
        {"ext_clip_cut", mc_editor_cmd_ext_clip_cut, NULL},
        {"ext_clip_paste", mc_editor_cmd_ext_clip_paste, NULL},
        {"paste_from_history", mc_editor_cmd_paste_from_history, NULL},

        {"format_paragraph", mc_editor_cmd_format_paragraph, (void *) FALSE},
        {"format_paragraph_force", mc_editor_cmd_format_paragraph, (void *) TRUE},
        {"format_paragraph_auto", mc_editor_cmd_format_paragraph_auto, NULL},

        {"save_as", mc_editor_cmd_save_as, NULL},
        {"save_confirm", mc_editor_cmd_save_confirm, NULL},
        {"save_block", mc_editor_cmd_save_block, NULL},

        {"insert_file", mc_editor_cmd_insert_file, NULL},
        {"file_load_prev", mc_editor_cmd_file_load_prev, NULL},
        {"file_load_next", mc_editor_cmd_file_load_next, NULL},
        {"syntax_show_dialog", mc_editor_cmd_syntax_show_dialog, NULL},

        {"search", mc_editor_cmd_search, (void *) FALSE},
        {"search_continue", mc_editor_cmd_search, (void *) TRUE},
        {"replace", mc_editor_cmd_replace, (void *) FALSE},
        {"replace_continue", mc_editor_cmd_replace, (void *) TRUE},

        {"complete", mc_editor_cmd_complete, (void *) FALSE},
        {"match_keyword", mc_editor_cmd_match_keyword, (void *) FALSE},
        {"print_current_date", mc_editor_cmd_print_current_date, (void *) FALSE},
        {"goto_line", mc_editor_cmd_goto_line, (void *) FALSE},
        {"goto_matching_bracket", mc_editor_cmd_goto_matching_bracket, (void *) FALSE},
        {"user_menu", mc_editor_cmd_user_menu, (void *) FALSE},
        {"sort", mc_editor_cmd_sort, (void *) FALSE},
        {"run_external_command", mc_editor_cmd_run_external_command, (void *) FALSE},
        {"mail", mc_editor_cmd_mail, (void *) FALSE},
        {"insert_literal", mc_editor_cmd_insert_literal, (void *) FALSE},

        {"macro_record_start_stop", mc_editor_cmd_macro_record_start_stop, (void *) FALSE},
        {"macro_repeat_start_stop", mc_editor_cmd_macro_repeat_start_stop, (void *) FALSE},
        {"macro_store", mc_editor_cmd_macro_store, (void *) FALSE},
        {"macro_delete", mc_editor_cmd_macro_delete, (void *) FALSE},
        {"macro_repeat", mc_editor_cmd_macro_repeat, (void *) FALSE},

#ifdef HAVE_ASPELL
        {"spell_set_language", mc_editor_cmd_spell_set_language, (void *) FALSE},
        {"spell_check", mc_editor_cmd_spell_check, (void *) FALSE},
        {"spell_suggest_word", mc_editor_cmd_spell_suggest_word, (void *) FALSE},
#endif /* HAVE_ASPELL */

#ifdef HAVE_CHARSET
        {"select_codepage", mc_editor_cmd_select_codepage, (void *) FALSE},
#endif /* HAVE_CHARSET */

        {NULL, NULL, NULL}
    };
    /* *INDENT-ON* */

    /* *INDENT-OFF* */
    event_init_t standard_events[] =
    {
        {MCEVENT_GROUP_EDITOR, core_group_events},
        {NULL, NULL}
    };
    /* *INDENT-ON* */

    mc_event_mass_add (standard_events, error);

}

/* --------------------------------------------------------------------------------------------- */
