#ifndef MC__EDITCMD_DIALOGS_H
#define MC__EDITCMD_DIALOGS_H

#include "src/editor/edit.h"

/*** typedefs(not structures) and defined constants **********************************************/

struct etags_hash_struct;

#define B_REPLACE_ALL (B_USER+1)
#define B_REPLACE_ONE (B_USER+2)
#define B_SKIP_REPLACE (B_USER+3)

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void editcmd_dialog_replace_show (WEdit * edit, const char *search_default,
                                  const char *replace_default, char **search_text,
                                  char **replace_text);
gboolean editcmd_dialog_search_show (WEdit * edit);
int editcmd_dialog_raw_key_query (const char *heading, const char *query, gboolean cancel);
char *editcmd_dialog_completion_show (const WEdit * edit, GQueue * compl, int max_width);
void editcmd_dialog_select_definition_show (WEdit * edit, char *match_expr, GPtrArray * def_hash);
int editcmd_dialog_replace_prompt_show (WEdit * edit, char *from_text, char *to_text, int xpos,
                                        int ypos);

/*** inline functions ****************************************************************************/

#endif /* MC__EDITCMD_DIALOGS_H */
