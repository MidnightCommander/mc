#ifndef MC_UNIXCOMPAT_H
#define MC_UNIXCOMPAT_H

/* This header file collects differences between the various Unix
 * variants that are supported by the Midnight Commander and provides
 * replacement routines if they are not natively available.
 */

/* The major/minor macros are not specified in SUSv3, so we can only hope
 * they are provided by the operating system or emulate it.
 */

#include <sys/types.h>		/* BSD */
#ifdef HAVE_SYS_MKDEV_H
# include <sys/mkdev.h>		/* Solaris 9 */
#endif
#ifdef HAVE_SYS_SYSMACROS_H
# include <sys/sysmacros.h>	/* AIX */
#endif

#ifndef major
# warning major() is undefined. Device numbers will not be shown correctly.
# define major(devnum) (((devnum) >> 8) & 0xff)
#endif
#ifndef minor
# warning minor() is undefined. Device numbers will not be shown correctly.
# define minor(devnum) (((devnum) & 0xff))
#endif
#ifndef makedev
# warning makedev() is undefined. Device numbers will not be shown correctly.
# define makedev(major,minor) ((((major) & 0xff) << 8) | ((minor) & 0xff))
#endif

#endif
