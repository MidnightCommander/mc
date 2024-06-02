#ifndef MC__EDIT_ASPELL_H
#define MC__EDIT_ASPELL_H

#include "lib/global.h"         /* include <glib.h> */

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void aspell_init (void);
void aspell_clean (void);

int edit_suggest_current_word (WEdit * edit);
void edit_spellcheck_file (WEdit * edit);
void edit_set_spell_lang (void);

const char *spell_dialog_lang_list_show (const GPtrArray * languages);

/*** inline functions ****************************************************************************/

#endif /* MC__EDIT_ASPELL_H */
