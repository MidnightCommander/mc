/* Mount/umount support for the Midnight Commander
 *
 * Copyright (C) 1998-1999 The Free Software Foundation
 *
 * Author: Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#include <config.h>

#ifdef STDC_HEADERS
#include <stdlib.h>
#else
void free (void *ptr);
#endif
#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if defined (MOUNTED_GETFSSTAT)	/* __alpha running OSF_1 */
#include <sys/mount.h>
#include <sys/fs_types.h>
#endif				/* MOUNTED_GETFSSTAT */

#ifdef MOUNTED_GETMNTENT1	/* 4.3BSD, SunOS, HP-UX, Dynix, Irix.  */
#include <mntent.h>
#if !defined(MOUNTED)
#if defined(MNT_MNTTAB)		/* HP-UX.  */
#define MOUNTED MNT_MNTTAB
#endif
#if defined(MNTTABNAME)		/* Dynix.  */
#define MOUNTED MNTTABNAME
#endif
#endif
#endif

#ifdef MOUNTED_GETMNTINFO	/* 4.4BSD.  */
#include <sys/mount.h>
#endif

#ifdef MOUNTED_GETMNT		/* Ultrix.  */
#include <sys/mount.h>
#include <sys/fs_types.h>
#endif

#ifdef MOUNTED_FREAD		/* SVR2.  */
#include <mnttab.h>
#endif

#ifdef MOUNTED_FREAD_FSTYP	/* SVR3.  */
#include <mnttab.h>
#include <sys/fstyp.h>
#include <sys/statfs.h>
#endif

#ifdef MOUNTED_GETMNTENT2	/* SVR4.  */
#include <sys/mnttab.h>
#endif

#ifdef MOUNTED_VMOUNT		/* AIX.  */
#include <fshelp.h>
#include <sys/vfs.h>
#endif

/* 4.4BSD2 derived systems */
#if defined(__bsdi__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__)
#  ifndef MOUNT_UFS
#    define xBSD
#  endif
#endif

/* void error (void);  FIXME -- needed? */

#ifdef DOLPHIN
/* So special that it's not worth putting this in autoconf.  */
#undef MOUNTED_FREAD_FSTYP
#define MOUNTED_GETMNTTBL
#endif

#include <glib.h>
#include "gmount.h"

#ifdef MOUNTED_GETMNTENT1
gboolean
is_block_device_mountable (char *devname)
{
	FILE *f;
	struct mntent *mnt;
	gboolean retval = FALSE;
	
	if (getuid () == 0)
		return TRUE;
	
	f = setmntent ("/etc/fstab", "r");
	if (f == NULL)
		return FALSE;

	while ((mnt = getmntent (f))){
		if (strstr (mnt->mnt_opts, "user")){
			retval = TRUE;
			break;
		}
	}
	endmntent (f);
	return retval;
}

gboolean
is_block_device_mounted (char *filename)
{
	FILE *f;
	struct mntent *mnt;
	gboolean retval = FALSE;
	
	f = setmntent ("/etc/mtab", "r");
	if (f == NULL)
		return FALSE;

	while ((mnt = getmntent (f))){
		if (strcmp (mnt->mnt_fsname, filename) == 0)
			retval = TRUE;
	}
	endmntent (f);
	return retval;
}

GList *
get_list_of_mountable_devices (void)
{
	FILE *f;
	struct mntent *mnt;
	GList *list = NULL;
	
	f = setmntent ("/etc/fstab", "r");
	if (f == NULL)
		return NULL;

	while ((mnt = getmntent (f))){
		if (strstr (mnt->mnt_opts, "user")){
			list = g_list_prepend (list, g_strdup (mnt->mnt_fsname));
			break;
		}
	}
	endmntent (f);
	return list;
}

#else

gboolean
is_block_device_mountable (char *devname)
{
	return FALSE;
}

GList *
get_list_of_mountable_devices ()
{
	return NULL;
}

gboolean
is_block_device_mounted (char *devname)
{
	return TRUE;
}

#endif
