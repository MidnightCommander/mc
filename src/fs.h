/* Include file to use opendir/closedir/readdir */

#ifndef __FS_H
#define __FS_H

#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#include <sys/stat.h>


/* Replacement for permission bits missing in sys/stat.h */
#ifndef S_IFLNK
#   define S_IFLNK 0
#endif

#ifndef S_ISLNK
#   define S_ISLNK(x) (((x) & S_IFLNK) == S_IFLNK)
#endif

#ifndef S_IFSOCK
#   define S_IFSOCK 0
#endif

#ifndef S_ISSOCK
#   define S_ISSOCK(x) (((x) & S_IFSOCK) == S_IFSOCK)
#endif

#ifndef S_IFFIFO
#   define S_IFFIFO 0
#endif

#ifndef S_ISFIFO
#   define S_ISFIFO(x) (((x) & S_IFFIFO) == S_IFFIFO)
#endif

#ifndef S_IFCHR
#   define S_IFCHR 0
#endif

#ifndef S_ISCHR
#   define S_ISCHR(x) (((x) & S_IFCHR) == S_IFCHR)
#endif

#ifndef S_IFBLK
#   define S_IFBLK 0
#endif

#ifndef S_ISBLK
#   define S_ISBLK(x) (((x) & S_IFBLK) == S_IFBLK)
#endif


#ifndef MAXPATHLEN
#   define MC_MAXPATHLEN 4096
#else
#   define MC_MAXPATHLEN MAXPATHLEN
#endif

/* unistd.h defines _POSIX_VERSION on POSIX.1 systems. */
#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#   define NLENGTH(dirent) (strlen ((dirent)->d_name))
#   define DIRENT_LENGTH_COMPUTED 1
#elif defined(_MSC_VER)
/* dirent provided by glib */
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

static inline void compute_namelen(struct dirent* dent) {
#ifdef DIRENT_LENGTH_COMPUTED
    return;
#else
    dent->d_namlen = strlen (dent);
#endif
}

#endif /* !__FS_H */
