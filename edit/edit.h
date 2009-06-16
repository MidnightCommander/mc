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

#include "../src/global.h"	/* PATH_SEP_STR */

/* Editor widget */
struct WEdit;
typedef struct WEdit WEdit;

#define EDIT_KEY_EMULATION_NORMAL 0
#define EDIT_KEY_EMULATION_EMACS  1
#define EDIT_KEY_EMULATION_USER   2

extern int option_word_wrap_line_length;
extern int option_typewriter_wrap;
extern int option_auto_para_formatting;
extern int option_tab_spacing;
extern int option_fill_tabs_with_spaces;
extern int option_return_does_auto_indent;
extern int option_backspace_through_tabs;
extern int option_fake_half_tabs;
extern int option_persistent_selections;
extern int option_line_state;
extern int option_save_mode;
extern int option_save_position;
extern int option_syntax_highlighting;
extern char *option_backup_ext;

/* what editor are we going to emulate? */
extern int edit_key_emulation;

extern int edit_confirm_save;

extern int visible_tabs;
extern int visible_tws;

extern int simple_statusbar;

/* used in main() */
void edit_stack_init (void);
void edit_stack_free (void);

int edit_file (const char *_file, int line);

const char *edit_get_file_name (const WEdit *edit);
int edit_get_curs_col (const WEdit *edit);
const char *edit_get_syntax_type (const WEdit *edit);

/* editor home directory */
#define EDIT_DIR		".mc" PATH_SEP_STR "cedit"

/* file names */
#define EDIT_SYNTAX_FILE	EDIT_DIR PATH_SEP_STR "Syntax"
#define EDIT_CLIP_FILE		EDIT_DIR PATH_SEP_STR "cooledit.clip"
#define EDIT_MACRO_FILE		EDIT_DIR PATH_SEP_STR "cooledit.macros"
#define EDIT_BLOCK_FILE		EDIT_DIR PATH_SEP_STR "cooledit.block"
#define EDIT_TEMP_FILE		EDIT_DIR PATH_SEP_STR "cooledit.temp"

#define EDIT_GLOBAL_MENU	"cedit.menu"
#define EDIT_LOCAL_MENU		".cedit.menu"
#define EDIT_HOME_MENU		EDIT_DIR PATH_SEP_STR "menu"

#endif				/* MC_EDIT_H */
