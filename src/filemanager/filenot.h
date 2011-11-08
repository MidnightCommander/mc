/** \file  file.h
 *  \brief Header: File and directory operation routines
 */

#ifndef MC__FILENOT_H
#define MC__FILENOT_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* Misc Unix functions */
int my_mkdir (const vfs_path_t * s, mode_t mode);
int my_rmdir (const char *s);

/*** inline functions ****************************************************************************/

#endif /* MC__FILE_H */
