/* Mount/umount support for the Midnight Commander
 *
 * Copyright (C) 1998-1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
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
#include <stdio.h>
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

#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <gnome.h>
#include "../vfs/vfs.h"
#include "dialog.h"
#include "gdesktop.h"
#include "gmount.h"
#include "util.h"


static gboolean
option_has_user (char *str)
{
	char *p;
	if ((p = strstr (str, "user")) != NULL){
		if ((p - 2) >= str){
			if (strncmp (p - 2, "nouser", 6) == 0)
				return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

#ifdef MOUNTED_GETMNTENT1

/*
 * Returns wheter devname is mountable:
 * NULL if it is not or
 * g_strdup()ed string with the mount point
 */
char *
is_block_device_mountable (char *devname)
{
	FILE *f;
	struct mntent *mnt;
	char *retval = NULL;
	
	if (getuid () == 0)
		return NULL;
	
	f = setmntent ("/etc/fstab", "r");
	if (f == NULL)
		return NULL;

	while ((mnt = getmntent (f))){
		if (option_has_user (mnt->mnt_opts)){
			retval = g_strdup (mnt->mnt_dir);
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

static GList *
get_mountable_devices (void)
{
	FILE *f;
	struct mntent *mnt;
	GList *list = NULL;
	
	f = setmntent ("/etc/fstab", "r");
	if (f == NULL)
		return NULL;

	while ((mnt = getmntent (f))){
		if (option_has_user (mnt->mnt_opts)){
			list = g_list_prepend (list, g_strdup (mnt->mnt_fsname));
			break;
		}
	}
	endmntent (f);
	return list;
}

#else

char *
is_block_device_mountable (char *devname)
{
	return NULL;
}

static GList *
get_mountable_devices (void)
{
	return NULL;
}

gboolean
is_block_device_mounted (char *devname)
{
	return TRUE;
}

#endif


/* Cleans up the desktop directory from device files */
static void
cleanup_devices (void)
{
	DIR *dir;
	struct dirent *dent;

	dir = mc_opendir (desktop_directory);
	if (!dir) {
		g_warning ("Could not clean up desktop devices");
		return;
	}

	while ((dent = mc_readdir (dir)) != NULL) {
		char *full_name;
		char *buf;
		int size;

		full_name = g_concat_dir_and_file (desktop_directory, dent->d_name);
		if (gnome_metadata_get (full_name, "is-desktop-device", &size, &buf) == 0) {
			mc_unlink (full_name);
			gnome_metadata_delete (full_name);
			g_free (buf);
		}
		g_free (full_name);
	}

	mc_closedir (dir);
}

/* Creates the desktop link for the specified device */
static void
create_device_link (char *dev_name, char *short_dev_name, char *caption, char *icon)
{
	char *full_name;

	full_name = g_concat_dir_and_file (desktop_directory, short_dev_name);
	if (mc_symlink (dev_name, full_name) != 0) {
		message (FALSE,
			 _("Warning"),
			 _("Could not symlink %s to %s; "
			   "will not have such a desktop device icon."),
			 dev_name, full_name);
		return;
	}

	gnome_metadata_set (full_name, "icon-filename", strlen (icon) + 1, icon);
	gnome_metadata_set (full_name, "icon-caption", strlen (caption) + 1, caption);
	gnome_metadata_set (full_name, "is-desktop-device", 1, full_name); /* hack a boolean value */

	g_free (full_name);
}

/* Creates the desktop links to the mountable devices */
static void
setup_devices (void)
{
	GList *list, *l;
	int floppy_count;
	int hd_count;
	int generic_count;

	list = get_mountable_devices ();

	floppy_count = hd_count = generic_count = 0;

	for (l = list; l; l = l->next) {
		char *dev_name;
		char *short_dev_name;
		char *format;
		char *icon;
		int count;
		char buffer[128];

		dev_name = l->data;
		short_dev_name = x_basename (dev_name);

		/* Create the format/icon/count.  This could use better heuristics. */

		if (strncmp (short_dev_name, "fd", 2) == 0) {
			format = _("Floppy %d");
			icon = "i-floppy.png";
			count = floppy_count++;
		} else if (strncmp (short_dev_name, "hd", 2) == 0
			   || strncmp (short_dev_name, "sd", 2) == 0) {
			format = _("Disk %d");
			icon = "i-blockdev.png";
			count = hd_count++;
		} else {
			format = _("Device %d");
			icon = "i-blockdev.png";
			count = generic_count++;
		}

		g_snprintf (buffer, sizeof (buffer), format, count);

		/* Create the actual link */

		create_device_link (dev_name, short_dev_name, buffer, icon);
		g_free (l->data);
	}

	g_list_free (list);
}

void
gmount_setup_devices (int cleanup)
{
	if (cleanup)
		cleanup_devices ();

	setup_devices ();
}
