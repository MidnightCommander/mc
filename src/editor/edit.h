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
#include "lib/vfs/vfs.h"        /* vfs_path_t */

/*** typedefs(not structures) and defined constants **********************************************/

#define DEFAULT_WRAP_LINE_LENGTH 72

#define EDIT(x) ((WEdit *)(x))
#define CONST_EDIT(x) ((const WEdit *)(x))

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* Editor widget */
struct WEdit;
typedef struct WEdit WEdit;

typedef struct
{
    int word_wrap_line_length;
    gboolean typewriter_wrap;
    gboolean auto_para_formatting;
    gboolean fill_tabs_with_spaces;
    gboolean return_does_auto_indent;
    gboolean backspace_through_tabs;
    gboolean fake_half_tabs;
    gboolean persistent_selections;
    gboolean drop_selection_on_copy;    /* whether we need to drop selection on copy to buffer */
    gboolean cursor_beyond_eol;
    gboolean cursor_after_inserted_block;
    gboolean state_full_filename;
    gboolean line_state;
    int line_state_width;
    int save_mode;
    gboolean confirm_save;      /* queries on a save */
    gboolean save_position;
    gboolean syntax_highlighting;
    gboolean group_undo;
    char *backup_ext;
    char *filesize_threshold;
    char *stop_format_chars;
    gboolean visible_tabs;
    gboolean visible_tws;
    gboolean show_right_margin;
    gboolean simple_statusbar;  /* statusbar draw style */
    gboolean check_nl_at_eof;
} edit_options_t;

typedef struct
{
    vfs_path_t *file_vpath;
    long line_number;
} edit_arg_t;

/*** global variables defined in .c file *********************************************************/

extern edit_options_t edit_options;

/*** declarations of public functions ************************************************************/

/* used in main() */
void edit_stack_init (void);
void edit_stack_free (void);

gboolean edit_file (const edit_arg_t * arg);
gboolean edit_files (const GList * files);

edit_arg_t *edit_arg_vpath_new (vfs_path_t * file_vpath, long line_number);
edit_arg_t *edit_arg_new (const char *file_name, long line_number);
void edit_arg_init (edit_arg_t * arg, vfs_path_t * vpath, long line);
void edit_arg_assign (edit_arg_t * arg, vfs_path_t * vpath, long line);
void edit_arg_free (edit_arg_t * arg);

const char *edit_get_file_name (const WEdit * edit);
off_t edit_get_cursor_offset (const WEdit * edit);
long edit_get_curs_col (const WEdit * edit);
const char *edit_get_syntax_type (const WEdit * edit);

/*** inline functions ****************************************************************************/
#endif /* MC__EDIT_H */
