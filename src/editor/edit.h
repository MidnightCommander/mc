/*
   Editor public API
 */

/** \file edit.h
 *  \brief Header: editor public API
 *  \author Paul Sheer
 *  \date 1996, 1997
 *  \author Andrew Borodin
 *  \date 2009, 2012
 */

#ifndef MC__EDIT_H
#define MC__EDIT_H

#include "lib/global.h"         /* PATH_SEP_STR */
#include "lib/fileloc.h"
#include "lib/vfs/vfs.h"        /* vfs_path_t */

/*** typedefs(not structures) and defined constants **********************************************/

#define DEFAULT_WRAP_LINE_LENGTH 72

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* Editor widget */
struct WEdit;
typedef struct WEdit WEdit;

/*** global variables defined in .c file *********************************************************/

extern int option_word_wrap_line_length;
extern int option_typewriter_wrap;
extern int option_auto_para_formatting;
extern int option_fill_tabs_with_spaces;
extern int option_return_does_auto_indent;
extern int option_backspace_through_tabs;
extern int option_fake_half_tabs;
extern int option_persistent_selections;
extern int option_cursor_beyond_eol;
extern gboolean option_cursor_after_inserted_block;
extern int option_line_state;
extern int option_save_mode;
extern int option_save_position;
extern int option_syntax_highlighting;
extern int option_group_undo;
extern char *option_backup_ext;

extern int edit_confirm_save;

extern int visible_tabs;
extern int visible_tws;

extern int simple_statusbar;
extern int option_check_nl_at_eof;
extern int show_right_margin;

/*** declarations of public functions ************************************************************/

/* used in main() */
void edit_stack_init (void);
void edit_stack_free (void);

gboolean edit_file (const vfs_path_t * file_vpath, long line);
gboolean edit_files (const GList * files);

char *edit_get_file_name (const WEdit * edit);
long edit_get_curs_col (const WEdit * edit);
const char *edit_get_syntax_type (const WEdit * edit);

/*** inline functions ****************************************************************************/
#endif /* MC__EDIT_H */
