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
#define S_ISLNK(x) 0
#endif

#ifndef S_ISSOCK
#define S_ISSOCK(x) 0
#endif

#ifndef S_ISFIFO
#define S_ISFIFO(x) 0
#endif

#ifndef S_ISCHR
#define S_ISCHR(x) 0
#endif

#ifndef S_ISBLK
#define S_ISBLK(x) 0
#endif

/* Door is something that only exists on Solaris */
#ifndef S_ISDOOR
#define S_ISDOOR(x) 0
#endif

/* Special named files are widely used in QNX6 */
#ifndef S_ISNAM
#define S_ISNAM(x) 0
#endif

#ifndef PATH_MAX
#ifdef _POSIX_VERSION
#define PATH_MAX _POSIX_PATH_MAX
#else
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 1024
#endif
#endif
#endif

#ifndef MAXPATHLEN
#define MC_MAXPATHLEN 4096
#else
#define MC_MAXPATHLEN MAXPATHLEN
#endif

/* unistd.h defines _POSIX_VERSION on POSIX.1 systems. */
#define NLENGTH(dirent) (strlen ((dirent)->d_name))
#define DIRENT_LENGTH_COMPUTED 1

/* DragonFlyBSD doesn't provide MAXNAMLEN macro */
#ifndef MAXNAMLEN
#define MAXNAMLEN NAME_MAX
#endif

#define MC_MAXFILENAMELEN MAXNAMLEN

#define DIR_IS_DOT(x) ((x)[0] == '.' && (x)[1] == '\0')
#define DIR_IS_DOTDOT(x) ((x)[0] == '.' && (x)[1] == '.' && (x)[2] == '\0')

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

static inline void
compute_namelen (struct dirent *dent __attribute__ ((unused)))
{
#ifdef DIRENT_LENGTH_COMPUTED
    (void) dent;
    return;
#else
    dent->d_namlen = strlen (dent);
#endif
}

#endif
