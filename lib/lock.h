
/** \file
 *  \brief Header: file locking
 *  \author Adam Byrtek
 *  \date 2003
 *  Look at lock.c for more details
 */

#ifndef MC_LOCK_H
#define MC_LOCK_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

int lock_file (const char *fname);
int unlock_file (const char *fname);

/*** inline functions ****************************************************************************/

#endif /* MC_LOCK_H */
