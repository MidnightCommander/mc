/** \file fs.h
 *  \brief Header: fs compatibility definitions
 */

/* Include file to use opendir/closedir/readdir */

#ifndef MC_FS_H
#define MC_FS_H

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

/*** typedefs(not structures) and defined constants **********************************************/

/* Replacement for permission bits missing in sys/stat.h */
#ifndef S_ISLNK
#   define S_ISLNK(x) 0
#endif

#ifndef S_ISSOCK
#   define S_ISSOCK(x) 0
#endif

#ifndef S_ISFIFO
#   define S_ISFIFO(x) 0
#endif

#ifndef S_ISCHR
#   define S_ISCHR(x) 0
#endif

#ifndef S_ISBLK
#   define S_ISBLK(x) 0
#endif

/* Door is something that only exists on Solaris */
#ifndef S_ISDOOR
#   define S_ISDOOR(x) 0
#endif

/* Special named files are widely used in QNX6 */
#ifndef S_ISNAM
#   define S_ISNAM(x) 0
#endif


#ifndef MAXPATHLEN
#   define MC_MAXPATHLEN 4096
#else
#   define MC_MAXPATHLEN MAXPATHLEN
#endif

/* unistd.h defines _POSIX_VERSION on POSIX.1 systems. */
#define NLENGTH(dirent) (strlen ((dirent)->d_name))
#define DIRENT_LENGTH_COMPUTED 1

#ifndef MAXNAMLEN
#   define MC_MAXFILENAMELEN 256
#else
#   define MC_MAXFILENAMELEN MAXNAMLEN
#endif

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

static inline void
compute_namelen (struct dirent *dent __attribute__ ((unused)))
{
#ifdef DIRENT_LENGTH_COMPUTED
    return;
#else
    dent->d_namlen = strlen (dent);
#endif
}

#endif
