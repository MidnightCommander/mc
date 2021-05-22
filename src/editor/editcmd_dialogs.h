#ifndef MC__EDITCMD_DIALOGS_H
#define MC__EDITCMD_DIALOGS_H

#include "src/editor/edit.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

int editcmd_dialog_raw_key_query (const char *heading, const char *query, gboolean cancel);
char *editcmd_dialog_completion_show (const WEdit * edit, GQueue * compl, int max_width);
void editcmd_dialog_select_definition_show (WEdit * edit, char *match_expr, GPtrArray * def_hash);

/*** inline functions ****************************************************************************/

#endif /* MC__EDITCMD_DIALOGS_H */
