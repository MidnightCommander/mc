
#ifndef MC__DIALOG_SWITCH_H
#define MC__DIALOG_SWITCH_H

#include <sys/types.h>

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void dialog_switch_add (struct Dlg_head *h);
void dialog_switch_remove (struct Dlg_head *h);
size_t dialog_switch_num (void);

void dialog_switch_next (void);
void dialog_switch_prev (void);
void dialog_switch_list (void);

int dialog_switch_process_pending (void);
void dialog_switch_got_winch (void);
void dialog_switch_shutdown (void);

/*** inline functions ****************************************************************************/
#endif /* MC__DIALOG_SWITCH_H */
