/** \file syntax.h
 *  \brief Header: syntax highlighting via external command for mcview
 */

#ifndef MC__VIEWER_SYNTAX_H
#define MC__VIEWER_SYNTAX_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/** Default syntax highlighting command */
#define MCVIEW_SYNTAX_DEFAULT_CMD "source-highlight --failsafe --out-format=esc -i %s"

/** Maximum file size (in bytes) for syntax highlighting.
 *  Files larger than this are displayed without highlighting to avoid
 *  long processing times from external highlighters. */
#define MCVIEW_SYNTAX_MAX_FILE_SIZE (2 * 1024 * 1024)

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

extern char *mcview_syntax_command;

/*** declarations of public functions ************************************************************/

/** Extract the binary name (first word) from a command template.
 *  Returns a newly allocated string, or NULL if template is empty/NULL. */
char *mcview_syntax_extract_binary (const char *cmd_template);

/** Build a shell command by substituting %s with the shell-escaped filename.
 *  Returns a newly allocated string, or NULL on error (no %s, NULL args). */
char *mcview_syntax_build_command (const char *cmd_template, const char *filename);

/** Check if the configured syntax highlighter binary is available.
 *  Returns TRUE if found in PATH (or is an absolute path that exists). */
gboolean mcview_syntax_command_available (void);

/** Get the short name of the current backend (e.g. "bat", "srchi").
 *  Returns a static string, never NULL. */
const char *mcview_syntax_get_short_name (void);

/** Show a dialog to choose the syntax highlighting backend.
 *  Only installed backends are shown. Returns TRUE if selection changed. */
gboolean mcview_syntax_options_dialog (void);

/*** inline functions ****************************************************************************/

#endif /* MC__VIEWER_SYNTAX_H */
