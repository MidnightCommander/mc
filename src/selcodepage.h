
/** \file selcodepage.h
 *  \brief Header: user %interface for charset %selection
 */

#ifndef MC__SELCODEPAGE_H
#define MC__SELCODEPAGE_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* some results of select_charset() */
#define SELECT_CHARSET_CANCEL       -2  // dialog has been canceled
#define SELECT_CHARSET_NO_TRANSLATE -1  // 1st item ("No translation") has been selected
/* In other cases select_charset() returns non-negative value
 * which is number of codepage in codepage list */

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

int select_charset (int center_y, int center_x, int current_charset);
gboolean do_set_codepage (int);
gboolean do_select_codepage (void);

/*** inline functions ****************************************************************************/

#endif
