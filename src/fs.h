/* Include file to use opendir/closedir/readdir */

#ifndef __FS_H
#define __FS_H
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#ifndef HAS_NO_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/stat.h>

#ifndef MAXPATHLEN
#   define MC_MAXPATHLEN 4096
#else
#   define MC_MAXPATHLEN MAXPATHLEN
#endif

/* unistd.h defines _POSIX_VERSION on POSIX.1 systems. */
#if defined(HAVE_DIRENT_H) || defined(_POSIX_VERSION)
# ifdef NEEDS_LOCAL_DIRENT_H
#   include "dirent.h"
# else
#   include <dirent.h>
# endif
#   define NLENGTH(dirent) (strlen ((dirent)->d_name))
#   define DIRENT_LENGTH_COMPUTED 1
#else
#   define dirent direct
#   define NLENGTH(dirent) ((dirent)->d_namlen)

#   ifdef HAVE_SYS_NDIR_H
#       include <sys/ndir.h>
#   endif /* HAVE_SYS_NDIR_H */

#   ifdef HAVE_SYS_DIR_H
#       include <sys/dir.h>
#   endif /* HAVE_SYS_DIR_H */

#   ifdef HAVE_NDIR_H
#       include <ndir.h>
#   endif /* HAVE_NDIR_H */
#endif /* not (HAVE_DIRENT_H or _POSIX_VERSION) */

#endif
