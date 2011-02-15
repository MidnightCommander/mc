/**
 * \file
 * \brief Header: local FS
 */

#ifndef MC__VFS_LOCAL_H
#define MC__VFS_LOCAL_H

#include "lib/vfs/vfs.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

extern void init_localfs (void);

/* these functions are used by other filesystems, so they are
 * published here. */
extern int local_close (void *data);
extern ssize_t local_read (void *data, char *buffer, size_t count);
extern int local_fstat (void *data, struct stat *buf);
extern int local_errno (struct vfs_class *me);
extern off_t local_lseek (void *data, off_t offset, int whence);

/*** inline functions ****************************************************************************/
#endif
