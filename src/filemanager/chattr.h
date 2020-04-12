/** \file chattr.h
 *  \brief Header: chattr command
 */

#ifndef MC__CHATTR_H
#define MC__CHATTR_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void chattr_cmd (void);
const char *chattr_get_as_str (unsigned long attr);

/*** inline functions ****************************************************************************/

#endif /* MC__CHATTR_H */
