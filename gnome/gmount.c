/* Mount/umount support for the Midnight Commander
 *
 * Copyright (C) 1998-1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <stdio.h>

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

#ifdef HAVE_CORBA
#include <libgnorba/gnorba.h>
#include "magicdev.h"
#endif HAVE_CORBA

#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <gnome.h>
#include "../vfs/vfs.h"
#include "dialog.h"
#include "gdesktop.h"
#include "gmount.h"
#include "util.h"
#include "i18n.h"



/* Information for a mountable device */
typedef struct {
	char *devname;
	char *mount_point;

	/* This is just a good guess */
	enum {
		TYPE_UNKNOWN,
		TYPE_CDROM,
		TYPE_NFS
	} type;
} devname_info_t;



static gboolean
option_has_user (char *str)
{
	char *p;
	if ((p = strstr (str, "user")) != NULL) {
		if ((p - 2) >= str) {
			if (strncmp (p - 2, "nouser", 6) == 0)
				return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}



#ifdef MOUNTED_GETMNTENT1

/* Returns whether the mount entry has the owner flag and our euid matches the
 * device file.
 */
static gboolean
option_has_owner (struct mntent *mnt)
{
	struct stat st;

	if (strstr (mnt->mnt_opts, "owner") == NULL)
		return FALSE;

	if (stat (mnt->mnt_fsname, &st) != 0)
		return FALSE;

	if (st.st_uid != geteuid ())
		return FALSE;

	return TRUE;
}

/* Returns whether devname is mountable: NULL if it is not or g_strdup()ed
 * string with the mount point.
 */
char *
is_block_device_mountable (char *mount_point)
{
	FILE *f;
	struct mntent *mnt;
	char *retval = NULL;

	f = setmntent ("/etc/fstab", "r");
	if (f == NULL)
		return NULL;

	while ((mnt = getmntent (f)) != NULL) {
		if (strcmp (mnt->mnt_dir, mount_point) != 0) {
			/* This second test is for compatibility with older
			 * desktops that might be using this.
			 */
			if (strcmp (mnt->mnt_dir, mount_point) != 0)
				continue;
		}

		if (getuid () == 0) {
			retval = g_strdup (mnt->mnt_dir);
			break;
		}

		if (option_has_user (mnt->mnt_opts) || option_has_owner (mnt)) {
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

	while ((mnt = getmntent (f)) != NULL) {
		if (strcmp (mnt->mnt_dir, filename) == 0)
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
	if (f == NULL) {
		message (1, MSG_ERROR, _("Could not open the /etc/fstab file"));
		return NULL;
	}

	while ((mnt = getmntent (f)) != NULL) {
		if (option_has_user (mnt->mnt_opts) || option_has_owner (mnt)) {
			devname_info_t *dit;

			dit = g_new0 (devname_info_t, 1);
			dit->devname = g_strdup (mnt->mnt_fsname);
			dit->mount_point = g_strdup (mnt->mnt_dir);
			dit->type = TYPE_UNKNOWN;

			if (strcmp (mnt->mnt_type, "iso9660") == 0)
				dit->type = TYPE_CDROM;

			if (strcmp (mnt->mnt_type, "nfs") == 0)
				dit->type = TYPE_NFS;

			list = g_list_prepend (list, dit);
		}
	}

	endmntent (f);

	list = g_list_reverse (list);
	return list;
}

char *
mount_point_to_device (char *mount_point)
{
	FILE *f;
	struct mntent *mnt;

	f = setmntent ("/etc/fstab", "r");
	if (f == NULL)
		return NULL;

	while ((mnt = getmntent (f))) {
		if (strcmp (mnt->mnt_dir, mount_point) == 0) {
			char *v;

			v = g_strdup (mnt->mnt_fsname);
			endmntent (f);
			return v;
		}
	}
	endmntent (f);
	return NULL;
}



#else

char *
is_block_device_mountable (char *mount_point)
{
	return NULL;
}

static GList *
get_mountable_devices (void)
{
	return NULL;
}

gboolean
is_block_device_mounted (char *mount_point)
{
	return TRUE;
}

char *
mount_point_to_device (char *mount_point)
{
	return NULL;
}
#endif



/* Returns whether our supported automounter is running */
static gboolean
automounter_is_running (void)
{
#if defined(HAVE_CORBA)
	CORBA_Object server;
	CORBA_Environment ev;
	gboolean result = FALSE;

	CORBA_exception_init (&ev);

        server = goad_server_activate_with_id (NULL,
					       "GOAD:magicdev:19990913",
                                               GOAD_ACTIVATE_EXISTING_ONLY,
					       NULL);

	if (!CORBA_Object_is_nil (server, &ev)) {
		result = GNOME_MagicDev_is_running (server, &ev);
		if (ev._major != CORBA_NO_EXCEPTION)
			result = FALSE;

		CORBA_Object_release (server, &ev);
	}

	return result;

#else  /* !HAVE_CORBA */
	return FALSE;
#endif /* HAVE_CORBA */
}



/* Cleans up the desktop directory from device files.  Includes block devices
 * and printers.
 */
void
desktop_cleanup_devices (void)
{
	DIR *dir;
	struct dirent *dent;

	dir = mc_opendir (desktop_directory);
	if (!dir) {
		g_warning ("Could not clean up desktop devices");
		return;
	}

	gnome_metadata_lock ();
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
	gnome_metadata_unlock ();

	mc_closedir (dir);
}

/* Creates the desktop link for the specified device */
static void
create_device_link (char *dev_name, char *short_dev_name, char *caption, char *icon,
		    gboolean ejectable)
{
	char *full_name;
	char *icon_full;
	char type = 'D';
	char ejectable_c = ejectable;

	icon_full = g_concat_dir_and_file (ICONDIR, icon);
	full_name = g_concat_dir_and_file (desktop_directory, short_dev_name);
	if (mc_symlink (dev_name, full_name) != 0) {
		message (FALSE,
			 _("Warning"),
			 _("Could not symlink %s to %s; "
			   "will not have such a desktop device icon."),
			 dev_name, full_name);
		return;
	}

	gnome_metadata_set (full_name, "icon-filename", strlen (icon_full) + 1, icon_full);
	gnome_metadata_set (full_name, "icon-caption", strlen (caption) + 1, caption);
	gnome_metadata_set (full_name, "device-is-ejectable", 1, &ejectable_c);
	gnome_metadata_set (full_name, "is-desktop-device", 1, &type); /* hack a boolean value */

	g_free (full_name);
	g_free (icon_full);
}

/* Creates the desktop links to the mountable devices */
static void
setup_devices (void)
{
	GList *list, *l;
	int floppy_count;
	int hd_count;
	int cdrom_count;
	int generic_count;
	int nfs_count;
	int automounter;

	list = get_mountable_devices ();
	automounter = automounter_is_running ();

	hd_count = 0;
	cdrom_count = 0;
	floppy_count = 0;
	generic_count = 0;

	for (l = list; l; l = l->next) {
		devname_info_t *dit = l->data;
		char *dev_name;
		char *short_dev_name;
		char *format;
		char *icon;
		int count;
		char buffer[128];
		gboolean release_format;
		gboolean ejectable;
		gboolean do_create;

		dev_name = dit->devname;
		short_dev_name = x_basename (dev_name);

		release_format = FALSE;
		ejectable = FALSE;
		do_create = TRUE;

		/* Create the format/icon/count.  This could use better heuristics. */
		if (dit->type == TYPE_CDROM || strncmp (short_dev_name, "cdrom", 5) == 0) {
			format = _("CD-ROM %d");
			icon = "i-cdrom.png";
			count = cdrom_count++;
			ejectable = TRUE;

			/* If our supported automounter is running, then we can
			 * be nice and create only the links for CD-ROMS that
			 * are actually mounted.
			 */
			if (automounter && !is_block_device_mounted (dit->mount_point))
				do_create = FALSE;
		} else if (strncmp (short_dev_name, "fd", 2) == 0) {
			format = _("Floppy %d");
			icon = "i-floppy.png";
			count = floppy_count++;
			ejectable = TRUE;
		} else if (strncmp (short_dev_name, "hd", 2) == 0
			   || strncmp (short_dev_name, "sd", 2) == 0) {
			format = _("Disk %d");
			icon = "i-blockdev.png";
			count = hd_count++;
		} else if (dit->type == TYPE_NFS) {
			release_format = TRUE;
			format = g_strdup_printf (_("NFS dir %s"), dit->mount_point);
			icon = "i-nfs.png";
			count = nfs_count++;
		} else {
			format = _("Device %d");
			icon = "i-blockdev.png";
			count = generic_count++;
		}

		g_snprintf (buffer, sizeof (buffer), format, count);
		if (release_format) {
			g_free (format);
			format = NULL;
		}

		/* Create the actual link */

		if (do_create)
			create_device_link (dit->mount_point, short_dev_name,
					    buffer, icon, ejectable);

		g_free (dit->devname);
		g_free (dit->mount_point);
		g_free (dit);
	}

	g_list_free (list);
}

void
gmount_setup_devices (void)
{
	setup_devices ();
}
