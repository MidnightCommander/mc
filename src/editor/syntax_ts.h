/*
   Tree-sitter syntax highlighting integration - public API.
 */

/** \file syntax_ts.h
 *  \brief Header: tree-sitter syntax highlighting for the editor
 */

#ifndef MC__EDIT_SYNTAX_TS_H
#define MC__EDIT_SYNTAX_TS_H

#ifdef HAVE_TREE_SITTER

/*** global variables defined in .c file ********************************************************/

/*** declarations of public functions ***********************************************************/

gboolean ts_init_for_file (WEdit *edit, const char *forced_grammar);
char *ts_config_reverse_lookup (const char *config_name, const char *display_value);
void ts_load_grammar_registry (void);  /* exposed for mc-syntax-dump */
void ts_free (WEdit *edit);
int ts_get_color_at (WEdit *edit, off_t byte_index);
void ts_rebuild_highlight_cache (WEdit *edit, off_t range_start, off_t range_end);

/* Functions from syntax.c needed by the tree-sitter module */
size_t read_one_line (char **line, FILE *f);
int this_try_alloc_color_pair (tty_color_pair_t *color);
const char *get_first_editor_line (WEdit *edit);

#endif /* HAVE_TREE_SITTER */

#endif /* MC__EDIT_SYNTAX_TS_H */
